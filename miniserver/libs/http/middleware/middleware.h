#pragma once

#include "http_types.h"

#include <functional>

using MiddlewareNext = std::function<HttpResponse(HttpRequestContext&)>;
using Middleware = std::function<HttpResponse(HttpRequestContext&, const MiddlewareNext&)>;
using AsyncMiddlewareNext = std::function<void(
    const std::shared_ptr<HttpRequestContext>&, ResponseCompletion)>;
using AsyncMiddleware = std::function<void(
    const std::shared_ptr<HttpRequestContext>&, const AsyncMiddlewareNext&,
    ResponseCompletion)>;
