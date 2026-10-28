#pragma once

#include <memory>
#include <map>
#include <string>
#include <vector>

#include "router.h"
#include "server.h"
#include "application_config.h"
#include "middleware.h"
#include "sqlite_database.h"
#include "authentication_middleware.h"
#include "statistics_middleware.h"

class ServerApplication {
public:
    explicit ServerApplication(ApplicationConfig config);
    // Loads the default config near the executable unless config_path is supplied.
    explicit ServerApplication(const std::string& executable_path,
                               std::string config_path = {});

    void set_config_path(std::string path);
    const std::string& config_path() const noexcept;
    const ApplicationConfig& config() const noexcept;

    void add_route(std::string method, std::string path, RouteAccess access,
                   Router::Handler handler,
                   std::optional<RouteRateLimit> rate_limit = std::nullopt);
    void add_async_route(std::string method, std::string path, RouteAccess access,
                         AsyncRouteHandler handler,
                         std::optional<RouteRateLimit> rate_limit = std::nullopt);
    void add_middleware(Middleware middleware);
    void add_async_middleware(AsyncMiddleware middleware);
    void add_authenticator(std::string provider, auth::Authenticator authenticator);
    void set_not_found_handler(Router::Handler handler);
    database::SQLiteDatabase& database() noexcept;
    std::shared_ptr<RequestStatistics> request_statistics() const noexcept;
    void initialize();
    void run();
    void stop();

private:
    std::string executable_path_;
    std::string config_path_;
    ApplicationConfig config_;
    std::unique_ptr<database::SQLiteDatabase> database_;
    std::shared_ptr<RequestStatistics> request_statistics_;
    std::vector<Middleware> middlewares_;
    std::map<std::string, auth::Authenticator> authenticators_;
    Router router_;
    std::unique_ptr<Server> server_;
    mutable bool configuration_locked_ = false;
};
