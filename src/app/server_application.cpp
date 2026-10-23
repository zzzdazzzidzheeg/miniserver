#include "server_application.h"

#include <stdexcept>
#include <utility>

ServerApplication::ServerApplication(ApplicationConfig config)
    : config_(std::move(config)),
      database_(std::make_unique<database::SQLiteDatabase>(config_.database.file)) {}

void ServerApplication::add_route(
    std::string method, std::string path, RouteAccess access, Router::Handler handler) {
    router_.add_route(std::move(method), std::move(path), access, std::move(handler));
}

void ServerApplication::add_middleware(Middleware middleware) {
    router_.add_middleware(std::move(middleware));
}

database::SQLiteDatabase& ServerApplication::database() noexcept {
    return *database_;
}

void ServerApplication::initialize() {
    if (server_) {
        throw std::logic_error("ServerApplication is already initialized");
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
