#pragma once

#include <memory>
#include <string>

#include "router.h"
#include "server.h"

class ServerApplication {
public:
    ServerApplication(unsigned short port, std::string certificate_file,
        std::string private_key_file);

    void add_route(std::string method, std::string path, Router::Handler handler);
    void initialize();
    void run();
    void stop();

private:
    unsigned short port_;
    std::string certificate_file_;
    std::string private_key_file_;
    Router router_;
    std::unique_ptr<Server> server_;
};
