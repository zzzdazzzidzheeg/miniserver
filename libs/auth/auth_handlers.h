#pragma once

#include "router.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace auth {

struct IssuedAccessToken {
    std::string token;
    std::uint64_t expires_in_seconds = 0;
};

using LoginCallback = std::function<std::optional<IssuedAccessToken>(
    std::string_view username, std::string_view password)>;

enum class RegistrationOutcome {
    Created,
    ConfirmationRequired,
    Rejected
};

using RegistrationCallback = std::function<RegistrationOutcome(const nlohmann::json&)>;
using LogoutCallback = std::function<bool(std::string_view bearer_token)>;
using AccountDeletionCallback = std::function<bool(std::string_view principal_subject)>;

// Expects a JSON object containing non-empty "username" and "password" strings.
// The callback validates credentials and issues an access token.
Router::Handler make_login_handler(LoginCallback login);

// Passes the full JSON object to the application callback, which owns account
// validation, persistence, and any confirmation workflow.
Router::Handler make_registration_handler(RegistrationCallback register_account);

// Revokes the authenticated request's bearer token and returns 204 on success.
Router::Handler make_logout_handler(LogoutCallback logout);

// Deletes the authenticated principal's account and returns 204 on success.
Router::Handler make_account_deletion_handler(AccountDeletionCallback delete_account);

} // namespace auth
