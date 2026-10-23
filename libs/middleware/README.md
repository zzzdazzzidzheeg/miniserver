# Middleware

Middleware wraps request routing and can inspect a request, call the next middleware, or return a response early. Middleware is added to the router during application setup, before the server starts.

```cpp
app.add_middleware([](const HttpRequest& request, const MiddlewareNext& next) {
    if (!request.has(http::field::authorization)) {
        return http_utils::make_error_response(401, "Unauthorized", request.version());
    }
    return next(request);
});
```

Middleware runs in registration order. The first registered middleware wraps the rest of the chain. Call `next(request)` at most once when continuing. `make_request_logging_middleware()` is provided as a built-in example.
