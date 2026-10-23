#include <exception>
#include <filesystem>
#include <string>
#include <utility>

#include "logger.h"
#include "handlers.h"
#include "app/server_application.h"
#include "app/application_config.h"
#include "request_logging.h"

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
        Logger::configure(config.logging.level, config.logging.file);
        log_info("Application: Starting server");

        ServerApplication app(std::move(config));
        app.add_middleware(make_request_logging_middleware());
        app.add_route("GET", "/api/ping", handlers::PingHandler{});
        app.add_route("GET", "/api/echo", handlers::EchoHandler{});
        app.add_route("GET", "/api/info", handlers::InfoHandler{"1.0"});
        app.initialize();
        app.run();
        app.stop();
        log_info("Application: Shutdown complete");
        return 0;
    } catch (const std::exception& error) {
        log_error(std::string("Application: Failed to start server: ") + error.what());
        return 1;
    }
}
