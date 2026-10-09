#include <exception>
#include <string>

#include "logger.h"
#include "handlers.h"
#include "app/server_application.h"

int main() {
    log_info("Application: Starting server");

    try {
        ServerApplication app(4433, SERVER_CERT_PATH, SERVER_KEY_PATH);
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