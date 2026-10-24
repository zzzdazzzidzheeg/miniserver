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

The router selects the route before starting middleware. Middleware runs in registration order, with the first registered middleware wrapping the rest of the chain. Call `next(context)` at most once when continuing. The context contains the original request, optional `matched_route` metadata (method, path, and access policy), and an optional authenticated principal. A middleware can therefore apply route-specific policy without redoing route matching. `make_request_logging_middleware()` is provided as a built-in example. For route authentication, use the component in `libs/auth/`.
