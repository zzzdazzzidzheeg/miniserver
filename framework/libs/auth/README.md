# Authentication

Authentication is optional. Mark each route as `RouteAccess::Public` or `RouteAccess::Authenticated`; the router fails closed if a protected route reaches its handler without a principal.

The generic authentication middleware does not store users or issue credentials. The application provides an authenticator that validates credentials and returns a principal, or `std::nullopt` when credentials are missing or invalid. The built-in bearer adapter extracts a Bearer token and delegates validation to an application callback:

```cpp
#include "authentication_middleware.h"

app.add_authenticator("bearer", auth::make_bearer_authenticator(
    [](std::string_view token) -> std::optional<Principal> {
        return validate_access_token(token); // Application-owned validation/storage.
    }));

app.add_route("GET", "/api/profile", RouteAccess::Authenticated,
    [](HttpRequestContext& context) {
        const nlohmann::json result = {{"subject", context.principal->subject}};
        return http_utils::make_json_response(200, result, context.request.version());
    });
```

Select the registered provider in `authentication.provider` in the application config (for example, `"bearer"`). The default `"none"` disables authentication middleware. When a provider is selected, the application must register it before `initialize()`; startup fails if it has not. `authentication.challenge` controls the `WWW-Authenticate` value. Other credential methods can be added as named authenticators and selected through the same config field.

The bearer adapter rejects malformed or missing credentials. It does not accept tokens on its own: token signature/lookup, expiry, revocation, and principal mapping remain the validator's responsibility. Never use an accept-all validator or put live bearer tokens in source-controlled config.

The challenge string is returned in `WWW-Authenticate` for rejected credentials. It must match the credential scheme implemented by the application. Protected requests without valid credentials receive `401`; public routes bypass authentication. If the authenticator middleware is missing for a protected route, the router returns `500` and does not call the handler.

Authentication establishes identity. Handlers or application services must still enforce authorization for each protected operation and resource.

## Login and registration handlers

The handler factories provide common HTTP and JSON handling while leaving credential storage and account policy configurable. The sample app wires them to `auth::SQLiteAccountService`; applications can instead provide their own callbacks:

```cpp
app.add_route("POST", "/api/auth/login", RouteAccess::Public,
    auth::make_login_handler([&](std::string_view username, std::string_view password) {
        return account_service.login(username, password); // optional<IssuedAccessToken>
    }));

app.add_route("POST", "/api/auth/register", RouteAccess::Public,
    auth::make_registration_handler([&](const nlohmann::json& payload) {
        return account_service.register_account(payload); // RegistrationOutcome
    }));
```

Login expects a JSON object with non-empty `username` and `password` strings. The callback verifies the credentials and issues a token; invalid credentials receive a generic `401`. Registration passes its JSON object to the application callback, which can accept extra fields, persist the account, send confirmation messages, or reject signup. It returns `201` for an immediately created account, `202` when confirmation is required, and a generic `400` when registration is rejected. The factories do not create users, hash passwords, issue tokens, or send email; those policies belong to the supplied service.

The sample app also provides `POST /api/auth/logout` and `DELETE /api/auth/me`; both require authentication and return `204` on success. Logout revokes only the presented bearer token. Account deletion removes the authenticated account and all its stored access tokens. Custom applications can use `make_logout_handler()` and `make_account_deletion_handler()` with callbacks that implement their own credential and account storage policies.

`SQLiteAccountService` is the included sample implementation. It stores users through `database::SQLiteDatabase::users`, stores only SHA-256 hashes of random opaque access tokens, and rejects expired or revoked tokens. Passwords are hashed with PBKDF2-HMAC-SHA256 and a per-password random salt. The sample registration activates accounts immediately and does not send email; use custom callbacks for confirmation flows. Password hashing is synchronous, so high-load applications should move it off the server's single event-loop thread.
