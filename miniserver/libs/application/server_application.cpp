#include "server_application.h"
#include "request_logging.h"
#include "logger.h"

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace {

std::string find_default_config(const std::string& executable_path) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path executable = fs::absolute(executable_path, error).lexically_normal();
    if (!error) {
        const fs::path executable_dir = executable.parent_path();
        const fs::path candidates[] = {
            executable_dir / "config/server.jsonc",
            executable_dir / "config/server.json",
            executable_dir.parent_path() / "config/server.jsonc",
            executable_dir.parent_path() / "config/server.json",
            fs::current_path(error) / "config/server.jsonc",
            fs::current_path(error) / "config/server.json"
        };
        for (const auto& candidate : candidates) {
            error.clear();
            if (fs::is_regular_file(candidate, error) && !error) return candidate.string();
        }
    }

    const fs::path generated_path = !error
        ? executable.parent_path().parent_path() / "config/server.jsonc"
        : fs::path("config/server.jsonc");
    write_default_application_config(generated_path.string());
    throw std::runtime_error("No config file was found. A default config was generated at '" +
        generated_path.string() + "'. Set valid TLS certificate_file and private_key_file paths, then restart.");
}

std::unique_ptr<database::SQLiteDatabase> make_database(const ApplicationConfig& config) {
    return std::make_unique<database::SQLiteDatabase>(
        config.database.file, config.database.require_user_email);
}

} // namespace

ServerApplication::ServerApplication(ApplicationConfig config)
    : config_(std::move(config)),
      database_(make_database(config_)),
      request_statistics_(std::make_shared<RequestStatistics>()) {
    Logger::configure(config_.logging.level, config_.logging.file);
}

ServerApplication::ServerApplication(const std::string& executable_path,
                                     std::string config_path)
    : executable_path_(executable_path),
      config_path_(config_path.empty() ? find_default_config(executable_path_)
                                       : std::move(config_path)),
      config_(load_application_config(config_path_)),
      database_(make_database(config_)),
      request_statistics_(std::make_shared<RequestStatistics>()) {
    Logger::configure(config_.logging.level, config_.logging.file);
}

void ServerApplication::set_config_path(std::string path) {
    if (configuration_locked_ || server_) {
        throw std::logic_error("Config path must be set before configuring or initializing the application");
    }
    if (path.empty()) path = find_default_config(executable_path_);

    auto new_config = load_application_config(path);
    auto new_database = make_database(new_config);
    Logger::configure(new_config.logging.level, new_config.logging.file);
    config_ = std::move(new_config);
    database_ = std::move(new_database);
    config_path_ = std::move(path);
}

const std::string& ServerApplication::config_path() const noexcept {
    return config_path_;
}

const ApplicationConfig& ServerApplication::config() const noexcept {
    configuration_locked_ = true;
    return config_;
}

void ServerApplication::add_route(
    std::string method, std::string path, RouteAccess access, Router::Handler handler,
    std::optional<RouteRateLimit> rate_limit) {
    configuration_locked_ = true;
    router_.add_route(std::move(method), std::move(path), access, std::move(handler), rate_limit);
}

void ServerApplication::add_async_route(
    std::string method, std::string path, RouteAccess access, AsyncRouteHandler handler,
    std::optional<RouteRateLimit> rate_limit) {
    configuration_locked_ = true;
    router_.add_async_route(std::move(method), std::move(path), access,
        std::move(handler), rate_limit);
}

void ServerApplication::add_middleware(Middleware middleware) {
    if (server_) throw std::logic_error("Middleware must be registered before initialization");
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    configuration_locked_ = true;
    middlewares_.push_back(std::move(middleware));
}

void ServerApplication::add_async_middleware(AsyncMiddleware middleware) {
    if (server_) throw std::logic_error("Middleware must be registered before initialization");
    if (!middleware) throw std::invalid_argument("Async middleware cannot be empty");
    configuration_locked_ = true;
    async_middlewares_.push_back(std::move(middleware));
}

void ServerApplication::add_authenticator(std::string provider, auth::Authenticator authenticator) {
    if (server_) {
        throw std::logic_error("Authenticators must be registered before initialization");
    }
    if (provider.empty()) throw std::invalid_argument("Authentication provider name cannot be empty");
    if (!authenticator) throw std::invalid_argument("Authenticator cannot be empty");
    configuration_locked_ = true;
    if (!authenticators_.emplace(std::move(provider), std::move(authenticator)).second) {
        throw std::invalid_argument("Authentication provider is already registered");
    }
}

void ServerApplication::set_not_found_handler(Router::Handler handler) {
    configuration_locked_ = true;
    router_.set_not_found_handler(std::move(handler));
}

database::SQLiteDatabase& ServerApplication::database() noexcept {
    configuration_locked_ = true;
    return *database_;
}

std::shared_ptr<RequestStatistics> ServerApplication::request_statistics() const noexcept {
    return request_statistics_;
}

void ServerApplication::initialize() {
    if (server_) {
        throw std::logic_error("ServerApplication is already initialized");
    }
    configuration_locked_ = true;

    if (config_.middleware.request_logging_enabled) {
        router_.add_builtin_middleware(make_request_logging_middleware());
        router_.add_async_middleware(make_async_request_logging_middleware());
    }
    if (config_.middleware.statistics_enabled) {
        router_.add_builtin_middleware(make_request_statistics_middleware(request_statistics_));
        router_.add_async_middleware(
            make_async_request_statistics_middleware(request_statistics_));
    }
    for (auto& middleware : middlewares_) {
        router_.add_middleware(std::move(middleware));
    }
    for (auto& middleware : async_middlewares_) {
        router_.add_async_middleware(std::move(middleware));
    }

    if (config_.authentication.provider != "none") {
        const auto provider = authenticators_.find(config_.authentication.provider);
        if (provider == authenticators_.end()) {
            throw std::runtime_error("No authenticator registered for configured provider '" +
                config_.authentication.provider + "'");
        }
        router_.add_builtin_middleware(auth::make_authentication_middleware(
            provider->second, config_.authentication.challenge));
        router_.add_async_middleware(auth::make_async_authentication_middleware(
            provider->second, config_.authentication.challenge));
    }

    {
        router_.add_builtin_middleware(make_rate_limit_middleware(request_statistics_, RateLimitConfig{
            config_.middleware.rate_limit_requests,
            std::chrono::seconds(config_.middleware.rate_limit_window_seconds),
            config_.middleware.rate_limit_max_tracked_clients,
            config_.middleware.rate_limit_by_principal,
            config_.middleware.rate_limit_enabled
        }));
        router_.add_async_middleware(make_async_rate_limit_middleware(request_statistics_, RateLimitConfig{
            config_.middleware.rate_limit_requests,
            std::chrono::seconds(config_.middleware.rate_limit_window_seconds),
            config_.middleware.rate_limit_max_tracked_clients,
            config_.middleware.rate_limit_by_principal,
            config_.middleware.rate_limit_enabled
        }));
    }

    server_ = std::make_unique<Server>(config_.server, config_.session,
        [this](HttpRequest request, std::string client_address, ResponseCompletion completion) {
            router_.route_async(std::move(request), std::move(client_address),
                std::move(completion));
        });
}

void ServerApplication::run() {
    if (!server_) {
        throw std::logic_error("ServerApplication must be initialized before run");
    }
    server_->run();
}

void ServerApplication::stop() {
    if (server_) {
        server_->stop();
    }
}
