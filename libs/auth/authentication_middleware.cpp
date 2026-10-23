#include "authentication_middleware.h"

#include <stdexcept>
#include <utility>

namespace auth {

Middleware make_authentication_middleware(Authenticator authenticator, std::string challenge) {
    if (!authenticator) {
        throw std::invalid_argument("Authenticator cannot be empty");
    }
    if (challenge.empty() || challenge.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("WWW-Authenticate challenge must be non-empty and single-line");
    }

    return [authenticator = std::move(authenticator), challenge = std::move(challenge)](
        HttpRequestContext& context, const MiddlewareNext& next) {
        if (!context.requires_authentication()) {
            return next(context);
        }

        auto principal = authenticator(context.request);
        if (!principal || principal->subject.empty()) {
            HttpStringResponse response{http::status::unauthorized, context.request.version()};
            response.set(http::field::content_type, "text/plain");
            response.set(http::field::www_authenticate, challenge);
            response.body() = "Authentication required";
            response.prepare_payload();
            return HttpResponse{std::move(response)};
        }

        context.principal = std::move(*principal);
        return next(context);
    };
}

} // namespace auth
