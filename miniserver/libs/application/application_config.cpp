#include "application_config.h"

#include <fstream>
#include <filesystem>
#include <iterator>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace {

template <typename T>
T positive_value(const nlohmann::json& object, const char* key, T fallback) {
    const auto it = object.find(key);
    if (it == object.end()) return fallback;
    if (!it->is_number_unsigned() && !it->is_number_integer()) {
        throw std::runtime_error(std::string("Config field '") + key + "' must be a positive integer");
    }
    const auto value = it->get<std::int64_t>();
    if (value <= 0 || static_cast<std::uint64_t>(value) > std::numeric_limits<T>::max()) {
        throw std::runtime_error(std::string("Config field '") + key + "' is out of range");
    }
    return static_cast<T>(value);
}

const nlohmann::json& optional_object_field(const nlohmann::json& root, const char* key) {
    static const nlohmann::json empty_object = nlohmann::json::object();
    const auto it = root.find(key);
    if (it == root.end()) return empty_object;
    if (!it->is_object()) {
        throw std::runtime_error(std::string("Config section '") + key + "' must be an object");
    }
    return *it;
}

LogLevel parse_log_level(const std::string& value) {
    if (value == "debug") return LogLevel::Debug;
    if (value == "info") return LogLevel::Info;
    if (value == "warning") return LogLevel::Warning;
    if (value == "error") return LogLevel::Error;
    throw std::runtime_error("Config field 'logging.level' must be debug, info, warning, or error");
}

} // namespace

ApplicationConfig load_application_config(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open config file: " + path);

    nlohmann::json root;
    try {
        const std::string contents((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        root = nlohmann::json::parse(contents, nullptr, true, true);
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("Invalid config JSON: " + std::string(error.what()));
    }
    if (!root.is_object()) throw std::runtime_error("Config root must be a JSON object");

    ApplicationConfig config;
    const std::filesystem::path config_directory = std::filesystem::absolute(path).parent_path();
    const auto resolve_config_path = [&config_directory](const std::string& configured_path) {
        const std::filesystem::path value(configured_path);
        return value.is_absolute() ? value.string() : (config_directory / value).lexically_normal().string();
    };
    const auto& server = optional_object_field(root, "server");
    const auto& session = optional_object_field(root, "session");
    const auto& logging = optional_object_field(root, "logging");

    if (server.contains("address")) config.server.address = server.at("address").get<std::string>();
    const auto port = positive_value<std::uint32_t>(server, "port", config.server.port);
    if (port > std::numeric_limits<unsigned short>::max()) throw std::runtime_error("Config field 'port' must be at most 65535");
    config.server.port = static_cast<unsigned short>(port);
    if (server.contains("certificate_file")) config.server.certificate_file = server.at("certificate_file").get<std::string>();
    if (server.contains("private_key_file")) config.server.private_key_file = server.at("private_key_file").get<std::string>();
    config.server.max_connections = positive_value<std::size_t>(server, "max_connections", config.server.max_connections);
    config.server.worker_threads = positive_value<std::size_t>(server, "worker_threads", config.server.worker_threads);
    if (config.server.worker_threads > 256) {
        throw std::runtime_error("Config field 'server.worker_threads' must be at most 256");
    }

    config.session.handshake_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "handshake_timeout_seconds", config.session.handshake_timeout.count()));
    config.session.read_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "read_timeout_seconds", config.session.read_timeout.count()));
    config.session.write_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "write_timeout_seconds", config.session.write_timeout.count()));
    config.session.shutdown_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "shutdown_timeout_seconds", config.session.shutdown_timeout.count()));
    config.session.header_limit = positive_value<std::size_t>(session, "header_limit_bytes", config.session.header_limit);
    config.session.body_limit = positive_value<std::uint64_t>(session, "body_limit_bytes", config.session.body_limit);

    if (logging.contains("level")) config.logging.level = parse_log_level(logging.at("level").get<std::string>());
    if (logging.contains("file")) config.logging.file = logging.at("file").get<std::string>();

    if (const auto middleware = root.find("middleware"); middleware != root.end()) {
        if (!middleware->is_object()) {
            throw std::runtime_error("Config section 'middleware' must be an object");
        }
        if (const auto request_logging = middleware->find("request_logging");
            request_logging != middleware->end()) {
            if (!request_logging->is_object()) {
                throw std::runtime_error("Config section 'middleware.request_logging' must be an object");
            }
            if (request_logging->contains("enabled")) {
                if (!request_logging->at("enabled").is_boolean()) {
                    throw std::runtime_error("Config field 'middleware.request_logging.enabled' must be boolean");
                }
                config.middleware.request_logging_enabled = request_logging->at("enabled").get<bool>();
            }
        }
        if (const auto statistics = middleware->find("statistics"); statistics != middleware->end()) {
            if (!statistics->is_object()) {
                throw std::runtime_error("Config section 'middleware.statistics' must be an object");
            }
            if (statistics->contains("enabled")) {
                if (!statistics->at("enabled").is_boolean()) {
                    throw std::runtime_error("Config field 'middleware.statistics.enabled' must be boolean");
                }
                config.middleware.statistics_enabled = statistics->at("enabled").get<bool>();
            }
        }
        if (const auto rate_limit = middleware->find("rate_limit"); rate_limit != middleware->end()) {
            if (!rate_limit->is_object()) {
                throw std::runtime_error("Config section 'middleware.rate_limit' must be an object");
            }
            if (rate_limit->contains("enabled")) {
                if (!rate_limit->at("enabled").is_boolean()) {
                    throw std::runtime_error("Config field 'middleware.rate_limit.enabled' must be boolean");
                }
                config.middleware.rate_limit_enabled = rate_limit->at("enabled").get<bool>();
            }
            config.middleware.rate_limit_requests = positive_value<std::size_t>(
                *rate_limit, "requests", config.middleware.rate_limit_requests);
            config.middleware.rate_limit_window_seconds = positive_value<std::int64_t>(
                *rate_limit, "window_seconds", config.middleware.rate_limit_window_seconds);
            config.middleware.rate_limit_max_tracked_clients = positive_value<std::size_t>(
                *rate_limit, "max_tracked_clients", config.middleware.rate_limit_max_tracked_clients);
            if (rate_limit->contains("key")) {
                const auto key = rate_limit->at("key").get<std::string>();
                if (key == "principal-or-ip") config.middleware.rate_limit_by_principal = true;
                else if (key == "ip") config.middleware.rate_limit_by_principal = false;
                else throw std::runtime_error(
                    "Config field 'middleware.rate_limit.key' must be principal-or-ip or ip");
            }
        }
    }

    if (const auto database = root.find("database"); database != root.end()) {
        if (!database->is_object()) {
            throw std::runtime_error("Config section 'database' must be an object");
        }
        if (database->contains("file")) {
            config.database.file = database->at("file").get<std::string>();
        }
        if (const auto users = database->find("users"); users != database->end()) {
            if (!users->is_object()) {
                throw std::runtime_error("Config section 'database.users' must be an object");
            }
            if (users->contains("require_email")) {
                if (!users->at("require_email").is_boolean()) {
                    throw std::runtime_error("Config field 'database.users.require_email' must be boolean");
                }
                config.database.require_user_email = users->at("require_email").get<bool>();
            }
        }
    }

    if (const auto authentication = root.find("authentication"); authentication != root.end()) {
        if (!authentication->is_object()) {
            throw std::runtime_error("Config section 'authentication' must be an object");
        }
        if (authentication->contains("provider")) {
            config.authentication.provider = authentication->at("provider").get<std::string>();
        }
        if (authentication->contains("challenge")) {
            config.authentication.challenge = authentication->at("challenge").get<std::string>();
        }
        config.authentication.access_token_lifetime_seconds = positive_value<std::int64_t>(
            *authentication, "access_token_lifetime_seconds",
            config.authentication.access_token_lifetime_seconds);
        if (config.authentication.provider.empty()) {
            throw std::runtime_error("Config field 'authentication.provider' cannot be empty");
        }
        if (config.authentication.challenge.empty() ||
            config.authentication.challenge.find_first_of("\r\n") != std::string::npos) {
            throw std::runtime_error("Config field 'authentication.challenge' must be non-empty and single-line");
        }
    }

    if (const auto application = root.find("application"); application != root.end()) {
        if (!application->is_object()) {
            throw std::runtime_error("Config section 'application' must be an object");
        }
        if (application->contains("mode")) {
            const auto mode = application->at("mode").get<std::string>();
            if (mode == "api-only") config.mode = ApplicationMode::ApiOnly;
            else if (mode == "web") config.mode = ApplicationMode::Web;
            else throw std::runtime_error("Config field 'application.mode' must be api-only or web");
        }
        if (application->contains("web_root")) {
            config.web_root = application->at("web_root").get<std::string>();
        }
    }

    config.server.certificate_file = resolve_config_path(config.server.certificate_file);
    config.server.private_key_file = resolve_config_path(config.server.private_key_file);
    if (!config.logging.file.empty()) config.logging.file = resolve_config_path(config.logging.file);
    if (config.database.file != ":memory:") {
        config.database.file = resolve_config_path(config.database.file);
    }
    if (config.web_root.empty()) {
        throw std::runtime_error("Config field 'application.web_root' cannot be empty");
    }
    config.web_root = resolve_config_path(config.web_root);

    if (config.server.address.empty()) throw std::runtime_error("Config field 'address' cannot be empty");
    if (config.server.certificate_file.empty() || config.server.private_key_file.empty()) {
        throw std::runtime_error("Config must provide server.certificate_file and server.private_key_file");
    }
    if (config.database.file.empty()) {
        throw std::runtime_error("Config field 'database.file' cannot be empty");
    }
    return config;
}

void write_default_application_config(const std::string& path) {
    namespace fs = std::filesystem;
    const fs::path config_path(path);
    if (fs::exists(config_path)) {
        throw std::runtime_error("Refusing to overwrite existing config file: " + path);
    }
    if (!config_path.parent_path().empty()) {
        fs::create_directories(config_path.parent_path());
    }

    std::ofstream output(config_path);
    if (!output) throw std::runtime_error("Cannot create config file: " + path);
    output << R"CONFIG({
  // Application mode: "api-only" serves API routes; "web" also serves files from web_root.
  "application": {
    "mode": "api-only",
    "web_root": "../static"
  },

  // Listener settings. Replace these certificate paths with your TLS certificate and private key.
  "server": {
    "address": "127.0.0.1",
    "port": 8443,
    "certificate_file": "../certs/cert.pem",
    "private_key_file": "../certs/key.pem",
    "max_connections": 1024,
    "worker_threads": 4
  },

  // Per-connection timeouts and request size limits.
  "session": {
    "handshake_timeout_seconds": 10,
    "read_timeout_seconds": 15,
    "write_timeout_seconds": 15,
    "shutdown_timeout_seconds": 10,
    "header_limit_bytes": 8192,
    "body_limit_bytes": 1048576
  },

  // Log threshold and optional log file. An empty file writes logs to standard output.
  "logging": {
    "level": "info",
    "file": ""
  },

  // Built-in request middleware. Rate limiting is disabled until explicitly enabled.
  "middleware": {
    "request_logging": { "enabled": true },
    "statistics": { "enabled": true },
    "rate_limit": {
      "enabled": false,
      "key": "principal-or-ip",
      "requests": 60,
      "window_seconds": 60,
      "max_tracked_clients": 10000
    }
  },

  // SQLite account database. Email is optional unless require_email is true.
  "database": {
    "file": "../data/miniserver.sqlite3",
    "users": { "require_email": false }
  },

  // Select "none" or a registered authentication provider such as "bearer".
  "authentication": {
    "provider": "none",
    "challenge": "Bearer realm=\"miniserver\"",
    "access_token_lifetime_seconds": 3600
  }
}
)CONFIG";
    if (!output) throw std::runtime_error("Failed writing config file: " + path);
}
