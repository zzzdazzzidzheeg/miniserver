#pragma once

#include "http_types.h"

#include <functional>

using MiddlewareNext = std::function<HttpResponse(HttpRequestContext&)>;
using Middleware = std::function<HttpResponse(HttpRequestContext&, const MiddlewareNext&)>;
