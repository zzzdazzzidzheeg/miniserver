#include "server_application.h"

#include <stdexcept>
#include <utility>

ServerApplication::ServerApplication(unsigned short port, std::string certificate_file,
    std::string private_key_file)
    : port_(port),
      certificate_file_(std::move(certificate_file)),
      private_key_file_(std::move(private_key_file)) {}

void ServerApplication::add_route(
    std::string method, std::string path, Router::Handler handler) {
    router_.add_route(std::move(method), std::move(path), std::move(handler));
}

void ServerApplication::initialize() {
    if (server_) {
        throw std::logic_error("ServerApplication is already initialized");
    }

    server_ = std::make_unique<Server>(port_, certificate_file_, private_key_file_,
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
