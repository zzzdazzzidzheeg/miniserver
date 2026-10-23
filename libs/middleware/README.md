# Middleware

Middleware wraps request routing and can inspect a request, call the next middleware, or return a response early. Middleware is added to the router during application setup, before the server starts.

```cpp
app.add_middleware([](HttpRequestContext& context, const MiddlewareNext& next) {
    const auto& request = context.request;
    if (!request.has(http::field::authorization)) {
        return HttpResponse{http_utils::make_error_response(
            401, "Unauthorized", request.version())};
    }
    return next(context);
});
```

Middleware runs in registration order. The first registered middleware wraps the rest of the chain. Call `next(context)` at most once when continuing. The context contains the original request, route access requirement, and an optional authenticated principal. `make_request_logging_middleware()` is provided as a built-in example. For route authentication, use the component in `libs/auth/`.
