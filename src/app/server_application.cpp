#include "server_application.h"

#include <stdexcept>
#include <utility>

ServerApplication::ServerApplication(ApplicationConfig config)
    : config_(std::move(config)),
      database_(std::make_unique<database::SQLiteDatabase>(
          config_.database.file, config_.database.require_user_email)) {}

void ServerApplication::add_route(
    std::string method, std::string path, RouteAccess access, Router::Handler handler) {
    router_.add_route(std::move(method), std::move(path), access, std::move(handler));
}

void ServerApplication::add_middleware(Middleware middleware) {
    router_.add_middleware(std::move(middleware));
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

void ServerApplication::initialize() {
    if (server_) {
        throw std::logic_error("ServerApplication is already initialized");
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

    server_ = std::make_unique<Server>(config_.server, config_.session,
        [this](const HttpRequest& request) {
            return router_.route(request);
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
