#pragma once

#include <cstdint>
#include <string>
#include "server_config.h"
#include "logger.h"

struct LoggingConfig {
    LogLevel level = LogLevel::Info;
    std::string file;
};

struct MiddlewareConfig {
    bool request_logging_enabled = true;
    bool statistics_enabled = true;
    bool rate_limit_enabled = false;
    std::size_t rate_limit_requests = 60;
    std::int64_t rate_limit_window_seconds = 60;
    std::size_t rate_limit_max_tracked_clients = 10000;
    bool rate_limit_by_principal = true;
};

struct DatabaseConfig {
    std::string file = "../data/miniserver.sqlite3";
    bool require_user_email = false;
};

struct RegistrationConfig {
    bool auto_activate = true;
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
    MiddlewareConfig middleware;
    DatabaseConfig database;
    RegistrationConfig registration;
    AuthenticationConfig authentication;
    ApplicationMode mode = ApplicationMode::ApiOnly;
    std::string web_root = "../static";
};

ApplicationConfig load_application_config(const std::string& path);
void write_default_application_config(const std::string& path);
