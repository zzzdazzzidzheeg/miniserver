#pragma once

#include "middleware.h"

Middleware make_request_logging_middleware();
AsyncMiddleware make_async_request_logging_middleware();
