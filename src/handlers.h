#pragma once

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <ctime>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "server_application.h"
#include "auth_handlers.h"
#include "sqlite_account_service.h"
#include "authentication_middleware.h"
#include "static_file_handler.h"
#include "response_utils.h"

namespace http = boost::beast::http;

namespace handlers {

struct PingHandler {
    http::response<http::string_body> operator()(HttpRequestContext& context) const {
        const auto& request = context.request;
        const nlohmann::json response = {
            {"status", "ok"},
            {"message", "pong"}
        };
        return http_utils::make_json_response(200, response, request.version());
    }
};

struct EchoHandler {
    http::response<http::string_body> operator()(HttpRequestContext& context) const {
        const auto& request = context.request;
        try {
            const auto body = nlohmann::json::parse(request.body());
            const nlohmann::json response = {
                {"received_data", body},
                {"timestamp", std::time(nullptr)},
                {"method", request.method_string()}
            };
            return http_utils::make_json_response(200, response, request.version());
        } catch (const nlohmann::json::parse_error& error) {
            return http_utils::make_error_response(
                400, "Invalid JSON payload: " + std::string(error.what()), request.version());
        }
    }
};

// Demonstrates deferred work without blocking the server's I/O worker.
// This is for local testing only; production handlers should use an async
// client or a bounded worker pool instead of creating a thread per request.
struct AsyncDemoHandler {
    void operator()(const std::shared_ptr<HttpRequestContext>& context,
                    ResponseCompletion complete) const {
        std::thread([context, complete = std::move(complete)]() mutable {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            complete(http_utils::make_json_response(200,
                {{"status", "completed"}, {"delay_seconds", 2}},
                context->request.version()));
        }).detach();
    }
};

struct CurrentUserHandler {
    http::response<http::string_body> operator()(HttpRequestContext& context) const {
        return http_utils::make_json_response(200,
            {{"subject", context.principal->subject}}, context.request.version());
    }
};

struct InfoHandler {
    std::string server_version;
    std::time_t start_time;
    bool authentication_endpoints_enabled;
    bool registration_email_required;

    explicit InfoHandler(std::string version, bool auth_endpoints_enabled = false,
                         bool email_required = false)
        : server_version(std::move(version)), start_time(std::time(nullptr)),
          authentication_endpoints_enabled(auth_endpoints_enabled),
          registration_email_required(email_required) {}

    http::response<http::string_body> operator()(HttpRequestContext& context) const {
        const auto& request = context.request;
        nlohmann::json endpoints = nlohmann::json::array({
            {{"method", "GET"}, {"path", "/api/ping"}, {"authentication_required", false}},
            {{"method", "GET"}, {"path", "/api/echo"}, {"authentication_required", false}},
            {{"method", "GET"}, {"path", "/api/info"}, {"authentication_required", false}}
        });
        if (authentication_endpoints_enabled) {
            endpoints.push_back({{"method", "POST"}, {"path", "/api/auth/register"},
                {"authentication_required", false}});
            endpoints.push_back({{"method", "POST"}, {"path", "/api/auth/login"},
                {"authentication_required", false}});
            endpoints.push_back({{"method", "GET"}, {"path", "/api/auth/me"},
                {"authentication_required", true}});
            endpoints.push_back({{"method", "POST"}, {"path", "/api/auth/logout"},
                {"authentication_required", true}});
            endpoints.push_back({{"method", "DELETE"}, {"path", "/api/auth/me"},
                {"authentication_required", true}});
        }
        const nlohmann::json response = {
            {"server", "miniserver"},
            {"version", server_version},
            {"uptime_seconds", std::time(nullptr) - start_time},
            {"endpoints", std::move(endpoints)},
            {"registration_email_required", registration_email_required}
        };
        return http_utils::make_json_response(200, response, request.version());
    }
};

// Registers the example application's routes and optional static-file fallback.
inline void register_routes(ServerApplication& app) {
    const auto& config = app.config();
    const std::string authentication_provider = config.authentication.provider;
    const ApplicationMode application_mode = config.mode;
    const std::string web_root = config.web_root;
    const bool registration_email_required = config.database.require_user_email;
    const auto access_token_lifetime = std::chrono::seconds(
        config.authentication.access_token_lifetime_seconds);

    // Synchronous middleware wraps synchronous routes; asynchronous middleware
    // wraps deferred routes. Both styles can be registered in the same app.
    app.add_middleware([](HttpRequestContext& context, const MiddlewareNext& next) {
        auto response = next(context);
        std::visit([](auto& message) {
            message.set("X-Demo-Sync-Middleware", "applied");
        }, response);
        return response;
    });
    app.add_async_middleware([](const std::shared_ptr<HttpRequestContext>& context,
                                const AsyncMiddlewareNext& next,
                                ResponseCompletion completion) {
        next(context, [completion = std::move(completion)](HttpResponse response) mutable {
            std::visit([](auto& message) {
                message.set("X-Demo-Async-Middleware", "applied");
            }, response);
            completion(std::move(response));
        });
    });

    if (authentication_provider == "bearer") {
        auto accounts = std::make_shared<auth::SQLiteAccountService>(
            app.database(), access_token_lifetime, config.registration.auto_activate);
        app.add_authenticator("bearer", auth::make_bearer_authenticator(
            [accounts](std::string_view token) { return accounts->authenticate(token); }));
        app.add_route("POST", "/api/auth/login", RouteAccess::Public,
            auth::make_login_handler([accounts](std::string_view username,
                                                 std::string_view password) {
                return accounts->login(username, password);
            }));
        app.add_route("POST", "/api/auth/register", RouteAccess::Public,
            auth::make_registration_handler([accounts](const nlohmann::json& payload) {
                return accounts->register_account(payload);
            }));
        app.add_route("POST", "/api/auth/logout", RouteAccess::Authenticated,
            auth::make_logout_handler([accounts](std::string_view token) {
                return accounts->logout(token);
            }));
        app.add_route("DELETE", "/api/auth/me", RouteAccess::Authenticated,
            auth::make_account_deletion_handler([accounts](std::string_view subject) {
                return accounts->delete_account(subject);
            }));
        app.add_route("GET", "/api/auth/me", RouteAccess::Authenticated,
            CurrentUserHandler{});
    }

    app.add_route("GET", "/api/ping", RouteAccess::Public, PingHandler{});
    app.add_route("GET", "/api/echo", RouteAccess::Public, EchoHandler{});
    app.add_async_route("GET", "/api/async-demo", RouteAccess::Public,
        AsyncDemoHandler{});
    app.add_route("GET", "/api/info", RouteAccess::Public,
        InfoHandler{"1.0", authentication_provider == "bearer",
            registration_email_required});

    if (application_mode == ApplicationMode::Web) {
        app.set_not_found_handler(make_static_file_handler(web_root));
    }
}

} // namespace handlers
