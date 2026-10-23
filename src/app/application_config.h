#pragma once

#include <cstdint>
#include <string>
#include "server_config.h"
#include "logger.h"

struct LoggingConfig {
    LogLevel level = LogLevel::Info;
    std::string file;
};

struct DatabaseConfig {
    std::string file = "../data/miniserver.sqlite3";
    bool require_user_email = false;
};

struct AuthenticationConfig {
    // "none" disables authentication middleware; other names select a
    // registered provider, such as "bearer".
    std::string provider = "none";
    std::string challenge = "Bearer realm=\"miniserver\"";
    std::int64_t access_token_lifetime_seconds = 3600;
};

enum class ApplicationMode {
    ApiOnly,
    Web
};

struct ApplicationConfig {
    ServerConfig server;
    SessionConfig session;
    LoggingConfig logging;
    DatabaseConfig database;
    AuthenticationConfig authentication;
    ApplicationMode mode = ApplicationMode::ApiOnly;
    std::string web_root = "../static";
};

ApplicationConfig load_application_config(const std::string& path);
