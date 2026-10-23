#pragma once

#include <boost/beast/http.hpp>

#include <functional>
#include <optional>
#include <string>
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

struct Principal {
    std::string subject;
};

struct HttpRequestContext {
    explicit HttpRequestContext(const HttpRequest& http_request, bool requires_authentication = false)
        : request(http_request), authentication_required_(requires_authentication) {}

    bool requires_authentication() const noexcept {
        return authentication_required_;
    }

    const HttpRequest& request;
    std::optional<Principal> principal;

private:
    bool authentication_required_;
};

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
using RouteHandler = std::function<HttpResponse(HttpRequestContext&)>;
