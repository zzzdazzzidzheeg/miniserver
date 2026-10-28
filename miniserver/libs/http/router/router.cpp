#include "router.h"
#include "logger.h"
#include "response_utils.h" // Подключаем наши утилиты для ответов

#include <algorithm>
#include <atomic>
#include <cctype>
#include <future>
#include <stdexcept>

namespace {

std::string normalize_method(std::string method) {
    std::transform(method.begin(), method.end(), method.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return method;
}

std::string path_from_url(const ParsedUrl& parsed_url) {
    if (parsed_url.is_asterisk) return "*";
    if (parsed_url.path_segments.empty()) return "/";

    std::string path;
    for (const auto& segment : parsed_url.path_segments) {
        path += "/";
        path += segment;
    }
    return path;
}

} // namespace

Router::Router() {
    // Инициализируем обработчик по умолчанию (404 Not Found)
    default_handler_ = [](HttpRequestContext& context) {
        const auto& req = context.request;
        log_warning("Router", "Route not found for " + std::string(req.method_string()) +
            " " + std::string(req.target()));
        return http_utils::make_text_response(
            404, 
            "Resource not found: " + std::string(req.target()), 
            req.version()
        );
    };
}

void Router::add_route(std::string method, std::string path, RouteAccess access, Handler handler,
                       std::optional<RouteRateLimit> rate_limit) {
    if (!handler) throw std::invalid_argument("Route handler cannot be empty");
    if (rate_limit && (rate_limit->requests == 0 || rate_limit->window.count() <= 0)) {
        throw std::invalid_argument("Route rate limit values must be positive");
    }
    std::string key = normalize_method(std::move(method)) + " " + std::move(path);
    routes_[std::move(key)] = Route{std::move(handler), {}, access, rate_limit};
}

void Router::add_async_route(std::string method, std::string path, RouteAccess access,
                             AsyncRouteHandler handler,
                             std::optional<RouteRateLimit> rate_limit) {
    if (!handler) throw std::invalid_argument("Async route handler cannot be empty");
    if (rate_limit && (rate_limit->requests == 0 || rate_limit->window.count() <= 0)) {
        throw std::invalid_argument("Route rate limit values must be positive");
    }
    std::string key = normalize_method(std::move(method)) + " " + std::move(path);
    routes_[std::move(key)] = Route{{}, std::move(handler), access, rate_limit};
}

void Router::add_middleware(Middleware middleware) {
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    middleware_.push_back(std::move(middleware));
}

void Router::add_builtin_middleware(Middleware middleware) {
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    middleware_.push_back(std::move(middleware));
}

void Router::add_async_middleware(AsyncMiddleware middleware) {
    add_middleware(std::move(middleware));
}

bool Router::has_async_routes() const noexcept {
    for (const auto& entry : routes_) {
        if (entry.second.async_handler) return true;
    }
    return false;
}

void Router::set_not_found_handler(Handler handler) {
    if (!handler) throw std::invalid_argument("Not-found handler cannot be empty");
    default_handler_ = std::move(handler);
}

HttpResponse Router::route(const http::request<http::string_body>& req,
                           std::string client_address) const {
    auto result = std::make_shared<std::promise<HttpResponse>>();
    auto future = result->get_future();
    route_async(HttpRequest(req), std::move(client_address),
        [result](HttpResponse response) mutable {
            try { result->set_value(std::move(response)); }
            catch (const std::future_error&) {}
        });
    return future.get();
}

void Router::route_async(HttpRequest request, std::string client_address,
                         ResponseCompletion completion) const {
    if (!completion) return;
    const ParsedUrl parsed_url = parse_url(request.target());
    const Route* matched_route = nullptr;
    std::optional<RouteMetadata> route_metadata;
    if (parsed_url.valid) {
        const std::string path = path_from_url(parsed_url);
        const std::string method = normalize_method(std::string(request.method_string()));
        const auto route = routes_.find(method + " " + path);
        if (route != routes_.end()) {
            matched_route = &route->second;
            route_metadata = RouteMetadata{method, path, route->second.access,
                route->second.rate_limit};
        }
    }

    const bool authentication_required = matched_route &&
        matched_route->access == RouteAccess::Authenticated;
    auto owned_request = std::make_shared<HttpRequest>(std::move(request));
    auto context = std::make_shared<HttpRequestContext>(std::move(owned_request),
        authentication_required, std::move(client_address), std::move(route_metadata));

    MiddlewareNext next = [this, matched_route, valid_target = parsed_url.valid,
        target_error = parsed_url.error](const auto& request_context, ResponseCompletion done) {
        dispatch_async(request_context, matched_route, valid_target, target_error,
            std::move(done));
    };
    for (auto it = middleware_.rbegin(); it != middleware_.rend(); ++it) {
        Middleware current = *it;
        MiddlewareNext downstream = std::move(next);
        next = [current = std::move(current), downstream = std::move(downstream)](
                const auto& request_context, ResponseCompletion done) {
            auto completed = std::make_shared<std::atomic<bool>>(false);
            auto continued = std::make_shared<std::atomic<bool>>(false);
            ResponseCompletion finish_once = [completed, done = std::move(done)](
                    HttpResponse response) mutable {
                if (!completed->exchange(true, std::memory_order_relaxed)) {
                    done(std::move(response));
                }
            };
            MiddlewareNext next_once = [continued, downstream, finish_once](
                    const auto& next_context, ResponseCompletion next_completion) mutable {
                if (continued->exchange(true, std::memory_order_relaxed)) return;
                auto downstream_completed = std::make_shared<std::atomic<bool>>(false);
                downstream(next_context, [downstream_completed, next_context,
                        next_completion = std::move(next_completion), finish_once](
                            HttpResponse response) mutable {
                    if (!downstream_completed->exchange(true, std::memory_order_relaxed)) {
                        try {
                            next_completion(std::move(response));
                        } catch (const std::exception& error) {
                            log_error("Router", "Middleware completion exception: " +
                                std::string(error.what()));
                            finish_once(http_utils::make_error_response(
                                500, "Internal server error", next_context->request.version()));
                        } catch (...) {
                            log_error("Router", "Unknown middleware completion exception");
                            finish_once(http_utils::make_error_response(
                                500, "Internal server error", next_context->request.version()));
                        }
                    }
                });
            };
            try {
                current(request_context, next_once, finish_once);
            } catch (const std::exception& error) {
                log_error("Router", "Async middleware exception: " + std::string(error.what()));
                finish_once(http_utils::make_error_response(500, "Internal server error",
                    request_context->request.version()));
            } catch (...) {
                log_error("Router", "Unknown async middleware exception");
                finish_once(http_utils::make_error_response(500, "Internal server error",
                    request_context->request.version()));
            }
        };
    }
    next(context, std::move(completion));
}

void Router::dispatch_async(const std::shared_ptr<HttpRequestContext>& context,
                            const Route* route, bool valid_target,
                            const std::string& target_error,
                            ResponseCompletion completion) const {
    const auto& request = context->request;
    if (!valid_target) {
        completion(http_utils::make_error_response(
            400, "Invalid request target: " + target_error, request.version()));
        return;
    }
    if (!route) {
        try {
            completion(default_handler_(*context));
        } catch (const std::exception& error) {
            log_error("Router", "Not-found handler exception: " + std::string(error.what()));
            completion(http_utils::make_error_response(500, "Internal server error", request.version()));
        } catch (...) {
            log_error("Router", "Unknown not-found handler exception");
            completion(http_utils::make_error_response(500, "Internal server error", request.version()));
        }
        return;
    }
    if (route->access == RouteAccess::Authenticated && !context->principal) {
        if (context->authentication_checked()) {
            auto response = http_utils::make_error_response(
                401, "Authentication required", request.version());
            response.set(http::field::www_authenticate, context->authentication_challenge());
            completion(std::move(response));
        } else {
            completion(http_utils::make_error_response(
                500, "Internal server error", request.version()));
        }
        return;
    }
    try {
        if (route->async_handler) {
            route->async_handler(context, completion);
        } else {
            completion(route->handler(*context));
        }
    } catch (const std::exception& error) {
        log_error("Router", "Async handler exception: " + std::string(error.what()));
        completion(http_utils::make_error_response(500, "Internal server error", request.version()));
    } catch (...) {
        log_error("Router", "Unknown async handler exception");
        completion(http_utils::make_error_response(500, "Internal server error", request.version()));
    }
}
