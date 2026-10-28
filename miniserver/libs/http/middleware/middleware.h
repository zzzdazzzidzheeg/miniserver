#pragma once

#include "http_types.h"

#include <functional>

using MiddlewareNext = std::function<void(
    const std::shared_ptr<HttpRequestContext>&, ResponseCompletion)>;
using Middleware = std::function<void(
    const std::shared_ptr<HttpRequestContext>&, const MiddlewareNext&,
    ResponseCompletion)>;

// Compatibility aliases: synchronous and asynchronous routes now share one
// callback-based middleware chain.
using AsyncMiddlewareNext = MiddlewareNext;
using AsyncMiddleware = Middleware;
