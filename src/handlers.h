#pragma once

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <ctime>
#include <string>
#include <utility>

#include "response_utils.h"

namespace http = boost::beast::http;

namespace handlers {

struct PingHandler {
    http::response<http::string_body> operator()(
        const http::request<http::string_body>& request) const {
        const nlohmann::json response = {
            {"status", "ok"},
            {"message", "pong"}
        };
        return http_utils::make_json_response(200, response, request.version());
    }
};

struct EchoHandler {
    http::response<http::string_body> operator()(
        const http::request<http::string_body>& request) const {
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

struct InfoHandler {
    std::string server_version;
    std::time_t start_time;

    explicit InfoHandler(std::string version)
        : server_version(std::move(version)), start_time(std::time(nullptr)) {}

    http::response<http::string_body> operator()(
        const http::request<http::string_body>& request) const {
        const nlohmann::json response = {
            {"server", "miniserver"},
            {"version", server_version},
            {"uptime_seconds", std::time(nullptr) - start_time},
            {"endpoints", {
                {{"method", "GET"}, {"path", "/api/ping"}, {"authentication_required", false}},
                {{"method", "GET"}, {"path", "/api/echo"}, {"authentication_required", false}},
                {{"method", "GET"}, {"path", "/api/info"}, {"authentication_required", false}}
            }}
        };
        return http_utils::make_json_response(200, response, request.version());
    }
};

} // namespace handlers
