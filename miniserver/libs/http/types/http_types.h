#pragma once

#include <boost/beast/http.hpp>

#include <functional>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace http = boost::beast::http;

using HttpRequest = http::request<http::string_body>;
using HttpStringResponse = http::response<http::string_body>;
using HttpFileResponse = http::response<http::file_body>;
using HttpEmptyResponse = http::response<http::empty_body>;
using HttpResponse = std::variant<HttpStringResponse, HttpFileResponse, HttpEmptyResponse>;

enum class RouteAccess {
    Public,
    Authenticated
};

struct RouteRateLimit {
    std::size_t requests;
    std::chrono::seconds window;
};

struct RouteMetadata {
    std::string method;
    std::string path;
    RouteAccess access = RouteAccess::Public;
    std::optional<RouteRateLimit> rate_limit;
};

struct Principal {
    std::string subject;
};

struct HttpRequestContext {
private:
    std::shared_ptr<HttpRequest> owned_request_;

public:
    explicit HttpRequestContext(const HttpRequest& http_request, bool requires_authentication = false,
                                std::string client_address = {},
                                std::optional<RouteMetadata> matched_route = std::nullopt)
        : request(http_request), client_address(std::move(client_address)),
          matched_route(std::move(matched_route)),
          authentication_required_(requires_authentication) {}

    explicit HttpRequestContext(std::shared_ptr<HttpRequest> http_request,
                                bool requires_authentication = false,
                                std::string client_address = {},
                                std::optional<RouteMetadata> matched_route = std::nullopt)
        : owned_request_(std::move(http_request)), request(*owned_request_),
          client_address(std::move(client_address)), matched_route(std::move(matched_route)),
          authentication_required_(requires_authentication) {}

    bool requires_authentication() const noexcept {
        return authentication_required_;
    }

    void set_authentication_result(std::optional<Principal> authenticated_principal,
                                   std::string challenge) {
        authentication_checked_ = true;
        principal = std::move(authenticated_principal);
        authentication_challenge_ = std::move(challenge);
    }

    bool authentication_checked() const noexcept {
        return authentication_checked_;
    }

    const std::string& authentication_challenge() const noexcept {
        return authentication_challenge_;
    }

    const HttpRequest& request;
    const std::string client_address;
    // Present when routing matched a registered method/path before middleware runs.
    const std::optional<RouteMetadata> matched_route;
    std::optional<Principal> principal;

private:
    bool authentication_required_;
    bool authentication_checked_ = false;
    std::string authentication_challenge_;
};

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
using ContextRequestHandler = std::function<HttpResponse(const HttpRequest&, std::string_view client_address)>;
using RouteHandler = std::function<HttpResponse(HttpRequestContext&)>;
using ResponseCompletion = std::function<void(HttpResponse)>;
using AsyncRouteHandler = std::function<void(
    const std::shared_ptr<HttpRequestContext>&, ResponseCompletion)>;
using AsyncContextRequestHandler = std::function<void(
    HttpRequest, std::string, ResponseCompletion)>;
