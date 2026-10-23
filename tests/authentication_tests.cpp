#include "authentication_middleware.h"
#include "response_utils.h"
#include "router.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int response_status(const HttpResponse& response) {
    return std::visit([](const auto& message) { return message.result_int(); }, response);
}

} // namespace

int main() {
    try {
        Router router;
        router.add_route("GET", "/public", RouteAccess::Public,
            [](HttpRequestContext& context) {
                return HttpResponse{http_utils::make_text_response(
                    200, "public", context.request.version())};
            });
        router.add_route("GET", "/private", RouteAccess::Authenticated,
            [](HttpRequestContext& context) {
                return HttpResponse{http_utils::make_text_response(
                    200, context.principal->subject, context.request.version())};
            });

        HttpRequest public_request{http::verb::get, "/public", 11};
        require(response_status(router.route(public_request)) == 200,
            "Public route should work without auth middleware");

        HttpRequest private_request{http::verb::get, "/private", 11};
        require(response_status(router.route(private_request)) == 500,
            "Protected route should fail closed when auth middleware is missing");

        router.add_middleware(auth::make_authentication_middleware(
            [](const HttpRequest& request) -> std::optional<Principal> {
                const auto authorization = request[http::field::authorization];
                if (authorization != "Bearer valid") return std::nullopt;
                return Principal{"user-123"};
            },
            "Bearer realm=\"miniserver\""));

        const auto unauthorized = router.route(private_request);
        require(response_status(unauthorized) == 401,
            "Rejected credentials should receive HTTP 401");
        const auto& unauthorized_response = std::get<HttpStringResponse>(unauthorized);
        require(unauthorized_response[http::field::www_authenticate] == "Bearer realm=\"miniserver\"",
            "401 response should carry the configured challenge");

        private_request.set(http::field::authorization, "Bearer valid");
        const auto authorized = router.route(private_request);
        require(response_status(authorized) == 200,
            "Valid credentials should reach the protected route");
        require(std::get<HttpStringResponse>(authorized).body() == "user-123",
            "Authenticated principal should be available to the handler");

        private_request.set(http::field::authorization, "Bearer invalid");
        require(response_status(router.route(private_request)) == 401,
            "Invalid credentials should not reach the protected route");

        HttpRequest still_public{http::verb::get, "/public", 11};
        require(response_status(router.route(still_public)) == 200,
            "Public routes should bypass authentication");

        std::cout << "Authentication checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
