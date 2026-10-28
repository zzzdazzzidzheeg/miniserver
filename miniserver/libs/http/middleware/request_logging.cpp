#include "request_logging.h"

#include "logger.h"

#include <string>
#include <string_view>
#include <variant>

Middleware make_request_logging_middleware() {
    return [](const std::shared_ptr<HttpRequestContext>& context,
              const MiddlewareNext& next, ResponseCompletion completion) {
        const auto& request = context->request;
        const std::string_view target(request.target().data(), request.target().size());
        const auto query_start = target.find('?');
        const auto path = target.substr(0, query_start);
        log_info("HTTP", "Request " + std::string(request.method_string()) +
            " " + std::string(path));

        next(context, [completion = std::move(completion)](HttpResponse response) mutable {
            const auto status = std::visit(
                [](const auto& message) { return message.result_int(); }, response);
            log_info("HTTP", "Response " + std::to_string(status));
            completion(std::move(response));
        });
    };
}

AsyncMiddleware make_async_request_logging_middleware() {
    return make_request_logging_middleware();
}
