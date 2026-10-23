#include <exception>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "logger.h"
#include "handlers.h"
#include "app/server_application.h"
#include "app/application_config.h"
#include "request_logging.h"
#include "auth_handlers.h"
#include "sqlite_account_service.h"
#include "authentication_middleware.h"

namespace {

std::string find_default_config(const char* executable_path) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path executable = fs::absolute(executable_path, error).lexically_normal();
    if (!error) {
        const fs::path executable_dir = executable.parent_path();
        const fs::path candidates[] = {
            executable_dir / "config/server.json",
            executable_dir.parent_path() / "config/server.json",
            fs::current_path(error) / "config/server.json"
        };
        for (const auto& candidate : candidates) {
            error.clear();
            if (fs::is_regular_file(candidate, error) && !error) return candidate.string();
        }
    }
    return "config/server.json";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = argc > 1 ? argv[1] : find_default_config(argv[0]);
        auto config = load_application_config(config_path);
        const std::string authentication_provider = config.authentication.provider;
        const auto access_token_lifetime = std::chrono::seconds(
            config.authentication.access_token_lifetime_seconds);
        Logger::configure(config.logging.level, config.logging.file);
        log_info("Application", "Starting server");

        std::unique_ptr<auth::SQLiteAccountService> accounts;
        ServerApplication app(std::move(config));
        if (authentication_provider == "bearer") {
            accounts = std::make_unique<auth::SQLiteAccountService>(
                app.database(), access_token_lifetime);
            app.add_authenticator("bearer", auth::make_bearer_authenticator(
                [&accounts](std::string_view token) { return accounts->authenticate(token); }));
            app.add_route("POST", "/api/auth/login", RouteAccess::Public,
                auth::make_login_handler([&accounts](std::string_view username, std::string_view password) {
                    return accounts->login(username, password);
                }));
            app.add_route("POST", "/api/auth/register", RouteAccess::Public,
                auth::make_registration_handler([&accounts](const nlohmann::json& payload) {
                    return accounts->register_account(payload);
                }));
        }
        app.add_middleware(make_request_logging_middleware());
        app.add_route("GET", "/api/ping", RouteAccess::Public, handlers::PingHandler{});
        app.add_route("GET", "/api/echo", RouteAccess::Public, handlers::EchoHandler{});
        app.add_route("GET", "/api/info", RouteAccess::Public,
            handlers::InfoHandler{"1.0", authentication_provider == "bearer"});
        app.initialize();
        app.run();
        app.stop();
        log_info("Application", "Shutdown complete");
        return 0;
    } catch (const std::exception& error) {
        log_error("Application", std::string("Failed to start server: ") + error.what());
        return 1;
    }
}
