#pragma once

#include "middleware.h"

#include <functional>
#include <optional>
#include <string>

namespace auth {

using Authenticator = std::function<std::optional<Principal>(const HttpRequest&)>;

// The authenticator is application-provided. challenge is the matching
// WWW-Authenticate value for the selected credential scheme.
Middleware make_authentication_middleware(Authenticator authenticator, std::string challenge);

} // namespace auth
