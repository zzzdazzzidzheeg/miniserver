#include <exception>
#include <string>

#include "logger.h"
#include "handlers.h"
#include "server_application.h"

int main(int argc, char* argv[]) {
    try {
        ServerApplication app(argv[0], argc > 1 ? argv[1] : "");

        log_info("Application", "Starting server");
        handlers::register_routes(app);
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
