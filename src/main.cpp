#include <exception>
#include <string>

#include "logger.h"
#include "server.h"
#include "router.h"
#include "handlers.h"

int main() {
    log_info("Application: Starting server");

    try {
        Router router;
        router.add_route("GET", "/api/ping", handlers::PingHandler{});
        router.add_route("GET", "/api/echo", handlers::EchoHandler{});
        router.add_route("GET", "/api/info", handlers::InfoHandler{"1.0"});

        RequestHandler app_handler = [&router](const HttpRequest& request) {
            return router.route(request);
        };
        Server server(4433, SERVER_CERT_PATH, SERVER_KEY_PATH, app_handler);
        server.run();
        server.stop();
        log_info("Application: Shutdown complete");
        return 0;
    } catch (const std::exception& error) {
        log_error(std::string("Application: Failed to start server: ") + error.what());
        return 1;
    }
}