#include "server_application.h"
#include "request_logging.h"

#include <stdexcept>
#include <utility>

ServerApplication::ServerApplication(ApplicationConfig config)
    : config_(std::move(config)),
      database_(std::make_unique<database::SQLiteDatabase>(
          config_.database.file, config_.database.require_user_email)),
      request_statistics_(std::make_shared<RequestStatistics>()) {}

void ServerApplication::add_route(
    std::string method, std::string path, RouteAccess access, Router::Handler handler) {
    router_.add_route(std::move(method), std::move(path), access, std::move(handler));
}

void ServerApplication::add_middleware(Middleware middleware) {
    if (server_) throw std::logic_error("Middleware must be registered before initialization");
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    middlewares_.push_back(std::move(middleware));
}

void ServerApplication::add_authenticator(std::string provider, auth::Authenticator authenticator) {
    if (server_) {
        throw std::logic_error("Authenticators must be registered before initialization");
    }
    if (provider.empty()) throw std::invalid_argument("Authentication provider name cannot be empty");
    if (!authenticator) throw std::invalid_argument("Authenticator cannot be empty");
    if (!authenticators_.emplace(std::move(provider), std::move(authenticator)).second) {
        throw std::invalid_argument("Authentication provider is already registered");
    }
}

void ServerApplication::set_not_found_handler(Router::Handler handler) {
    router_.set_not_found_handler(std::move(handler));
}

database::SQLiteDatabase& ServerApplication::database() noexcept {
    return *database_;
}

std::shared_ptr<RequestStatistics> ServerApplication::request_statistics() const noexcept {
    return request_statistics_;
}

void ServerApplication::initialize() {
    if (server_) {
        throw std::logic_error("ServerApplication is already initialized");
    }

    if (config_.middleware.request_logging_enabled) {
        router_.add_middleware(make_request_logging_middleware());
    }
    if (config_.middleware.statistics_enabled) {
        router_.add_middleware(make_request_statistics_middleware(request_statistics_));
    }
    for (auto& middleware : middlewares_) {
        router_.add_middleware(std::move(middleware));
    }

    if (config_.authentication.provider != "none") {
        const auto provider = authenticators_.find(config_.authentication.provider);
        if (provider == authenticators_.end()) {
            throw std::runtime_error("No authenticator registered for configured provider '" +
                config_.authentication.provider + "'");
        }
        router_.add_middleware(auth::make_authentication_middleware(
            provider->second, config_.authentication.challenge));
    }

    if (config_.middleware.rate_limit_enabled) {
        router_.add_middleware(make_rate_limit_middleware(request_statistics_, RateLimitConfig{
            config_.middleware.rate_limit_requests,
            std::chrono::seconds(config_.middleware.rate_limit_window_seconds),
            config_.middleware.rate_limit_max_tracked_clients,
            config_.middleware.rate_limit_by_principal
        }));
    }

    server_ = std::make_unique<Server>(config_.server, config_.session,
        [this](const HttpRequest& request, std::string_view client_address) {
            return router_.route(request, std::string(client_address));
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
