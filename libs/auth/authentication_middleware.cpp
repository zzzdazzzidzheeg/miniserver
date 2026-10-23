#include "authentication_middleware.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace auth {

namespace {

bool ascii_equal_case_insensitive(char left, char right) {
    if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
    if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
    return left == right;
}

bool is_bearer_token_char(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == '_' ||
        ch == '~' || ch == '+' || ch == '/' || ch == '=';
}

std::optional<std::string_view> extract_bearer_token(std::string_view value) {
    constexpr std::string_view scheme = "Bearer";
    if (value.size() <= scheme.size()) return std::nullopt;
    for (std::size_t i = 0; i < scheme.size(); ++i) {
        if (!ascii_equal_case_insensitive(value[i], scheme[i])) return std::nullopt;
    }
    if (value[scheme.size()] != ' ') return std::nullopt;

    std::size_t token_start = scheme.size();
    while (token_start < value.size() && value[token_start] == ' ') ++token_start;
    if (token_start == value.size()) return std::nullopt;

    const auto token = value.substr(token_start);
    for (const char ch : token) {
        if (!is_bearer_token_char(ch)) return std::nullopt;
    }
    return token;
}

} // namespace

Authenticator make_bearer_authenticator(BearerTokenValidator validator) {
    if (!validator) {
        throw std::invalid_argument("Bearer token validator cannot be empty");
    }
    return [validator = std::move(validator)](const HttpRequest& request) {
        const auto authorization = request[http::field::authorization];
        const auto token = extract_bearer_token(std::string_view(authorization.data(), authorization.size()));
        if (!token) return std::optional<Principal>{};
        return validator(*token);
    };
}

Middleware make_authentication_middleware(Authenticator authenticator, std::string challenge) {
    if (!authenticator) {
        throw std::invalid_argument("Authenticator cannot be empty");
    }
    if (challenge.empty() || challenge.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("WWW-Authenticate challenge must be non-empty and single-line");
    }

    return [authenticator = std::move(authenticator), challenge = std::move(challenge)](
        HttpRequestContext& context, const MiddlewareNext& next) {
        auto principal = authenticator(context.request);
        if (principal && principal->subject.empty()) {
            principal.reset();
        }
        context.set_authentication_result(std::move(principal), challenge);
        return next(context);
    };
}

} // namespace auth
