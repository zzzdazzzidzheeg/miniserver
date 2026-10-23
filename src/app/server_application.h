#pragma once

#include <memory>
#include <string>

#include "router.h"
#include "server.h"
#include "application_config.h"
#include "middleware.h"
#include "sqlite_database.h"

class ServerApplication {
public:
    explicit ServerApplication(ApplicationConfig config);

    void add_route(std::string method, std::string path, RouteAccess access, Router::Handler handler);
    void add_middleware(Middleware middleware);
    database::SQLiteDatabase& database() noexcept;
    void initialize();
    void run();
    void stop();

private:
    ApplicationConfig config_;
    std::unique_ptr<database::SQLiteDatabase> database_;
    Router router_;
    std::unique_ptr<Server> server_;
};
