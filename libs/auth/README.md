# Authentication

Authentication is optional. Mark each route as `RouteAccess::Public` or `RouteAccess::Authenticated`; the router fails closed if a protected route reaches its handler without a principal.

The component does not choose a credential scheme or store users. The application provides an authenticator that validates its credentials and returns a principal, or `std::nullopt` when credentials are missing or invalid:

```cpp
#include "authentication_middleware.h"

auth::Authenticator authenticator = [](const HttpRequest& request)
    -> std::optional<Principal> {
    const auto authorization = request[http::field::authorization];
    return validate_access_token(std::string(authorization));
};

app.add_middleware(auth::make_authentication_middleware(
    std::move(authenticator), "Bearer"));

app.add_route("GET", "/api/profile", RouteAccess::Authenticated,
    [](HttpRequestContext& context) {
        const nlohmann::json result = {{"subject", context.principal->subject}};
        return http_utils::make_json_response(200, result, context.request.version());
    });
```

The challenge string is returned in `WWW-Authenticate` for rejected credentials. It must match the credential scheme implemented by the application. Protected requests without valid credentials receive `401`; public routes bypass authentication. If the authenticator middleware is missing for a protected route, the router returns `500` and does not call the handler.

Authentication establishes identity. Handlers or application services must still enforce authorization for each protected operation and resource.
