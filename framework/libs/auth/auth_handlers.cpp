#include "auth_handlers.h"

#include "logger.h"
#include "response_utils.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace auth {
namespace {

bool parse_credentials(const HttpRequestContext& context, std::string& username, std::string& password) {
    try {
        const auto body = nlohmann::json::parse(context.request.body());
        if (!body.is_object() || !body.contains("username") || !body.contains("password") ||
            !body.at("username").is_string() || !body.at("password").is_string()) {
            return false;
        }
        username = body.at("username").get<std::string>();
        password = body.at("password").get<std::string>();
        return !username.empty() && !password.empty();
    } catch (const nlohmann::json::exception& error) {
        log_warning("Auth", "Invalid credential JSON: " + std::string(error.what()));
        return false;
    }
}

HttpResponse bad_request(const HttpRequestContext& context) {
    return http_utils::make_error_response(400, "Invalid request", context.request.version());
}

std::optional<std::string_view> request_bearer_token(const HttpRequest& request) {
    const auto authorization = request[http::field::authorization];
    const std::string_view value(authorization.data(), authorization.size());
    constexpr std::string_view scheme = "Bearer ";
    if (value.size() <= scheme.size()) return std::nullopt;
    for (std::size_t i = 0; i < scheme.size() - 1; ++i) {
        char left = value[i];
        char right = scheme[i];
        if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
        if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
        if (left != right) return std::nullopt;
    }
    if (value[scheme.size() - 1] != ' ') return std::nullopt;
    const auto token = value.substr(scheme.size());
    if (token.empty() || token.find_first_of(" \t\r\n") != std::string_view::npos) {
        return std::nullopt;
    }
    return token;
}

} // namespace

Router::Handler make_login_handler(LoginCallback login) {
    if (!login) throw std::invalid_argument("Login callback cannot be empty");
    return [login = std::move(login)](HttpRequestContext& context) -> HttpResponse {
        std::string username;
        std::string password;
        if (!parse_credentials(context, username, password)) return bad_request(context);

        const auto issued = login(username, password);
        if (!issued || issued->token.empty()) {
            auto response = http_utils::make_error_response(
                401, "Invalid credentials", context.request.version());
            response.set(http::field::www_authenticate, "Bearer");
            return response;
        }

        return http_utils::make_json_response(200, {
            {"access_token", issued->token},
            {"token_type", "Bearer"},
            {"expires_in", issued->expires_in_seconds}
        }, context.request.version());
    };
}

Router::Handler make_registration_handler(RegistrationCallback register_account) {
    if (!register_account) throw std::invalid_argument("Registration callback cannot be empty");
    return [register_account = std::move(register_account)](HttpRequestContext& context) -> HttpResponse {
        nlohmann::json payload;
        try {
            payload = nlohmann::json::parse(context.request.body());
        } catch (const nlohmann::json::exception& error) {
            log_warning("Auth", "Invalid registration JSON: " + std::string(error.what()));
            return bad_request(context);
        }
        if (!payload.is_object()) return bad_request(context);

        switch (register_account(payload)) {
        case RegistrationOutcome::Created:
            return http_utils::make_json_response(201,
                {{"registered", true}, {"confirmation_required", false}}, context.request.version());
        case RegistrationOutcome::ConfirmationRequired:
            return http_utils::make_json_response(202,
                {{"registered", true}, {"confirmation_required", true}}, context.request.version());
        case RegistrationOutcome::Rejected:
        default:
            return http_utils::make_error_response(
                400, "Registration could not be completed", context.request.version());
        }
    };
}

Router::Handler make_logout_handler(LogoutCallback logout) {
    if (!logout) throw std::invalid_argument("Logout callback cannot be empty");
    return [logout = std::move(logout)](HttpRequestContext& context) -> HttpResponse {
        const auto token = request_bearer_token(context.request);
        if (!token || !logout(*token)) {
            return http_utils::make_error_response(401, "Authentication required", context.request.version());
        }
        HttpEmptyResponse response{http::status::no_content, context.request.version()};
        return HttpResponse{std::move(response)};
    };
}

Router::Handler make_account_deletion_handler(AccountDeletionCallback delete_account) {
    if (!delete_account) throw std::invalid_argument("Account deletion callback cannot be empty");
    return [delete_account = std::move(delete_account)](HttpRequestContext& context) -> HttpResponse {
        if (!context.principal || !delete_account(context.principal->subject)) {
            return http_utils::make_error_response(404, "Account not found", context.request.version());
        }
        HttpEmptyResponse response{http::status::no_content, context.request.version()};
        return HttpResponse{std::move(response)};
    };
}

} // namespace auth
