#pragma once

#include <string>
#include "server_config.h"
#include "logger.h"

struct LoggingConfig {
    LogLevel level = LogLevel::Info;
    std::string file;
};

struct ApplicationConfig {
    ServerConfig server;
    SessionConfig session;
    LoggingConfig logging;
};

ApplicationConfig load_application_config(const std::string& path);
