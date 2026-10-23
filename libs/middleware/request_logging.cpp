#include "request_logging.h"

#include "logger.h"

#include <string>
#include <string_view>
#include <variant>

Middleware make_request_logging_middleware() {
    return [](const HttpRequest& request, const MiddlewareNext& next) {
        const std::string_view target(request.target().data(), request.target().size());
        const auto query_start = target.find('?');
        const auto path = target.substr(0, query_start);
        log_info("HTTP", "Request " + std::string(request.method_string()) +
            " " + std::string(path));

        auto response = next(request);
        const auto status = std::visit(
            [](const auto& message) { return message.result_int(); }, response);
        log_info("HTTP", "Response " + std::to_string(status));
        return response;
    };
}
