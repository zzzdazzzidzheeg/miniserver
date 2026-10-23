#include "sqlite_account_service.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace auth {
namespace {

constexpr int password_iterations = 600000;
constexpr std::size_t password_salt_size = 16;
constexpr std::size_t password_hash_size = 32;
constexpr std::size_t access_token_size = 32;
constexpr std::int64_t max_token_lifetime_seconds = 30 * 24 * 60 * 60;
constexpr char hex_digits[] = "0123456789abcdef";

std::string hex_encode(const unsigned char* bytes, std::size_t size) {
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result.push_back(hex_digits[bytes[i] >> 4]);
        result.push_back(hex_digits[bytes[i] & 0x0f]);
    }
    return result;
}

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool hex_decode(std::string_view text, unsigned char* output, std::size_t output_size) {
    if (text.size() != output_size * 2) return false;
    for (std::size_t i = 0; i < output_size; ++i) {
        const int high = hex_value(text[i * 2]);
        const int low = hex_value(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        output[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return true;
}

std::array<unsigned char, password_hash_size> derive_password_hash(
    std::string_view password, const std::array<unsigned char, password_salt_size>& salt) {
    std::array<unsigned char, password_hash_size> derived{};
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()), password_iterations,
            EVP_sha256(), static_cast<int>(derived.size()), derived.data()) != 1) {
        throw std::runtime_error("Password hashing failed");
    }
    return derived;
}

std::string hash_password(std::string_view password) {
    std::array<unsigned char, password_salt_size> salt{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Secure random number generation failed");
    }
    const auto derived = derive_password_hash(password, salt);
    return "pbkdf2-sha256$600000$" + hex_encode(salt.data(), salt.size()) +
        "$" + hex_encode(derived.data(), derived.size());
}

bool verify_password(std::string_view password, std::string_view encoded) {
    constexpr std::string_view prefix = "pbkdf2-sha256$600000$";
    constexpr std::size_t salt_hex_size = password_salt_size * 2;
    constexpr std::size_t hash_hex_size = password_hash_size * 2;
    if (encoded.size() != prefix.size() + salt_hex_size + 1 + hash_hex_size ||
        encoded.substr(0, prefix.size()) != prefix || encoded[prefix.size() + salt_hex_size] != '$') {
        return false;
    }

    std::array<unsigned char, password_salt_size> salt{};
    std::array<unsigned char, password_hash_size> expected{};
    const auto salt_text = encoded.substr(prefix.size(), salt_hex_size);
    const auto hash_text = encoded.substr(prefix.size() + salt_hex_size + 1);
    if (!hex_decode(salt_text, salt.data(), salt.size()) ||
        !hex_decode(hash_text, expected.data(), expected.size())) {
        return false;
    }
    const auto actual = derive_password_hash(password, salt);
    return CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
}

std::string sha256_hex(std::string_view value) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_size = 0;
    if (EVP_Digest(value.data(), value.size(), digest.data(), &digest_size,
            EVP_sha256(), nullptr) != 1 || digest_size != 32) {
        throw std::runtime_error("Access token hashing failed");
    }
    return hex_encode(digest.data(), digest_size);
}

std::string create_access_token() {
    std::array<unsigned char, access_token_size> token{};
    if (RAND_bytes(token.data(), static_cast<int>(token.size())) != 1) {
        throw std::runtime_error("Secure random number generation failed");
    }
    return hex_encode(token.data(), token.size());
}

std::int64_t unix_time_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

constexpr std::string_view dummy_password_hash =
    "pbkdf2-sha256$600000$00000000000000000000000000000000$"
    "0000000000000000000000000000000000000000000000000000000000000000";

} // namespace

SQLiteAccountService::SQLiteAccountService(database::SQLiteDatabase& database,
        std::chrono::seconds access_token_lifetime)
    : database_(database), access_token_lifetime_seconds_(access_token_lifetime.count()) {
    if (access_token_lifetime_seconds_ <= 0 ||
        access_token_lifetime_seconds_ > max_token_lifetime_seconds) {
        throw std::invalid_argument("Access token lifetime must be between 1 second and 30 days");
    }
    database_.execute(
        "CREATE TABLE IF NOT EXISTS access_tokens ("
        "token_hash TEXT PRIMARY KEY, "
        "user_id INTEGER NOT NULL, "
        "expires_at INTEGER NOT NULL"
        ")");
    database_.execute("CREATE INDEX IF NOT EXISTS access_tokens_expiry ON access_tokens(expires_at)");
}

std::optional<IssuedAccessToken> SQLiteAccountService::login(
        std::string_view username, std::string_view password) {
    if (username.empty() || password.empty() || username.size() > 255 || password.size() > 1024) {
        return std::nullopt;
    }

    const auto user = database_.users.find_by_username(username);
    const bool password_valid = verify_password(password,
        user ? std::string_view(user->password_hash) : dummy_password_hash);
    if (!user || !user->confirmed || !password_valid) return std::nullopt;

    const std::string token = create_access_token();
    const std::int64_t expires_at = unix_time_seconds() + access_token_lifetime_seconds_;
    const std::string token_hash = sha256_hex(token);
    database_.execute("DELETE FROM access_tokens WHERE expires_at <= ?", {unix_time_seconds()});
    database_.execute("INSERT INTO access_tokens (token_hash, user_id, expires_at) VALUES (?, ?, ?)",
        {token_hash, user->id, expires_at});
    return IssuedAccessToken{token, static_cast<std::uint64_t>(access_token_lifetime_seconds_)};
}

RegistrationOutcome SQLiteAccountService::register_account(const nlohmann::json& payload) {
    if (!payload.contains("username") || !payload.at("username").is_string() ||
        !payload.contains("password") || !payload.at("password").is_string()) {
        return RegistrationOutcome::Rejected;
    }

    const std::string username = payload.at("username").get<std::string>();
    const std::string password = payload.at("password").get<std::string>();
    if (username.empty() || username.size() > 255 || password.size() < 8 || password.size() > 1024) {
        return RegistrationOutcome::Rejected;
    }

    std::optional<std::string> email;
    if (payload.contains("email")) {
        if (!payload.at("email").is_string()) return RegistrationOutcome::Rejected;
        email = payload.at("email").get<std::string>();
        if (email->empty() || email->size() > 320) return RegistrationOutcome::Rejected;
    }

    try {
        database_.users.add(database::NewUser{
            username, std::move(email), hash_password(password), true
        });
    } catch (const database::DuplicateUserError&) {
        return RegistrationOutcome::Rejected;
    } catch (const std::invalid_argument&) {
        return RegistrationOutcome::Rejected;
    }
    return RegistrationOutcome::Created;
}

std::optional<Principal> SQLiteAccountService::authenticate(std::string_view bearer_token) {
    if (bearer_token.size() != access_token_size * 2) return std::nullopt;
    for (const char value : bearer_token) {
        if (hex_value(value) < 0) return std::nullopt;
    }

    const auto rows = database_.query(
        "SELECT users.id FROM access_tokens "
        "JOIN users ON users.id = access_tokens.user_id "
        "WHERE access_tokens.token_hash = ? AND access_tokens.expires_at > ? "
        "AND users.confirmed = 1",
        {sha256_hex(bearer_token), unix_time_seconds()});
    if (rows.empty()) return std::nullopt;
    return Principal{std::to_string(std::get<std::int64_t>(rows.front().at("id")))};
}

} // namespace auth
