#pragma once

#include "auth_handlers.h"
#include "sqlite_database.h"

#include <chrono>
#include <optional>
#include <string_view>

namespace auth {

// Minimal SQLite-backed account store used by the sample server. Applications
// with custom account/confirmation policies can continue to provide callbacks.
class SQLiteAccountService {
public:
    explicit SQLiteAccountService(database::SQLiteDatabase& database,
        std::chrono::seconds access_token_lifetime = std::chrono::hours(1),
        bool auto_activate = true);

    std::optional<IssuedAccessToken> login(
        std::string_view username, std::string_view password);
    RegistrationOutcome register_account(const nlohmann::json& payload);
    std::optional<Principal> authenticate(std::string_view bearer_token);
    bool logout(std::string_view bearer_token);
    bool delete_account(std::string_view principal_subject);

private:
    database::SQLiteDatabase& database_;
    std::int64_t access_token_lifetime_seconds_;
    bool auto_activate_;
};

} // namespace auth
