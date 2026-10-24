#pragma once

#include "middleware.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace auth {

using Authenticator = std::function<std::optional<Principal>(const HttpRequest&)>;
using BearerTokenValidator = std::function<std::optional<Principal>(std::string_view)>;

// Extracts a Bearer credential and delegates token validation to the application.
Authenticator make_bearer_authenticator(BearerTokenValidator validator);

// The authenticator is application-provided. challenge is the matching
// WWW-Authenticate value for the selected credential scheme.
Middleware make_authentication_middleware(Authenticator authenticator, std::string challenge);

} // namespace auth
