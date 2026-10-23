#pragma once

#include <boost/beast/http.hpp>

#include <functional>
#include <variant>

namespace http = boost::beast::http;

using HttpRequest = http::request<http::string_body>;
using HttpStringResponse = http::response<http::string_body>;
using HttpFileResponse = http::response<http::file_body>;
using HttpEmptyResponse = http::response<http::empty_body>;
using HttpResponse = std::variant<HttpStringResponse, HttpFileResponse, HttpEmptyResponse>;
using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
