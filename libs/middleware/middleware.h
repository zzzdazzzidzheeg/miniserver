#pragma once

#include "http_types.h"

#include <functional>

using MiddlewareNext = std::function<HttpResponse(const HttpRequest&)>;
using Middleware = std::function<HttpResponse(const HttpRequest&, const MiddlewareNext&)>;
