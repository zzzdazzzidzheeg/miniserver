#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

struct ServerConfig {
    std::string address = "0.0.0.0";
    unsigned short port = 4433;
    std::string certificate_file;
    std::string private_key_file;
    std::size_t max_connections = 1024;
};

struct SessionConfig {
    std::chrono::seconds handshake_timeout{10};
    std::chrono::seconds read_timeout{15};
    std::chrono::seconds write_timeout{15};
    std::chrono::seconds shutdown_timeout{10};
    std::size_t header_limit = 8192;
    std::uint64_t body_limit = 1024 * 1024;
};
