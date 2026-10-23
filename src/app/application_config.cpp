#include "application_config.h"

#include <fstream>
#include <filesystem>
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

const nlohmann::json& object_field(const nlohmann::json& root, const char* key) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_object()) {
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
        input >> root;
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
    const auto& server = object_field(root, "server");
    const auto& session = object_field(root, "session");
    const auto& logging = object_field(root, "logging");

    if (server.contains("address")) config.server.address = server.at("address").get<std::string>();
    const auto port = positive_value<std::uint32_t>(server, "port", config.server.port);
    if (port > std::numeric_limits<unsigned short>::max()) throw std::runtime_error("Config field 'port' must be at most 65535");
    config.server.port = static_cast<unsigned short>(port);
    if (server.contains("certificate_file")) config.server.certificate_file = server.at("certificate_file").get<std::string>();
    if (server.contains("private_key_file")) config.server.private_key_file = server.at("private_key_file").get<std::string>();
    config.server.max_connections = positive_value<std::size_t>(server, "max_connections", config.server.max_connections);

    config.session.handshake_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "handshake_timeout_seconds", config.session.handshake_timeout.count()));
    config.session.read_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "read_timeout_seconds", config.session.read_timeout.count()));
    config.session.write_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "write_timeout_seconds", config.session.write_timeout.count()));
    config.session.shutdown_timeout = std::chrono::seconds(positive_value<std::int64_t>(session, "shutdown_timeout_seconds", config.session.shutdown_timeout.count()));
    config.session.header_limit = positive_value<std::size_t>(session, "header_limit_bytes", config.session.header_limit);
    config.session.body_limit = positive_value<std::uint64_t>(session, "body_limit_bytes", config.session.body_limit);

    if (logging.contains("level")) config.logging.level = parse_log_level(logging.at("level").get<std::string>());
    if (logging.contains("file")) config.logging.file = logging.at("file").get<std::string>();

    config.server.certificate_file = resolve_config_path(config.server.certificate_file);
    config.server.private_key_file = resolve_config_path(config.server.private_key_file);
    if (!config.logging.file.empty()) config.logging.file = resolve_config_path(config.logging.file);

    if (config.server.address.empty()) throw std::runtime_error("Config field 'address' cannot be empty");
    if (config.server.certificate_file.empty() || config.server.private_key_file.empty()) {
        throw std::runtime_error("Config must provide server.certificate_file and server.private_key_file");
    }
    return config;
}
