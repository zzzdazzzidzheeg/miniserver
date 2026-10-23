#include "router.h"
#include "logger.h"
#include "response_utils.h" // Подключаем наши утилиты для ответов

#include <algorithm>
#include <cctype>
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

void Router::add_route(std::string method, std::string path, RouteAccess access, Handler handler) {
    if (!handler) throw std::invalid_argument("Route handler cannot be empty");
    std::string key = normalize_method(std::move(method)) + " " + std::move(path);
    routes_[std::move(key)] = Route{std::move(handler), access};
}

void Router::add_middleware(Middleware middleware) {
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    middleware_.push_back(std::move(middleware));
}

void Router::set_not_found_handler(Handler handler) {
    if (!handler) throw std::invalid_argument("Not-found handler cannot be empty");
    default_handler_ = std::move(handler);
}

HttpResponse Router::route(const http::request<http::string_body>& req,
                           std::string client_address) const {
    const ParsedUrl parsed_url = parse_url(req.target());
    const Route* matched_route = nullptr;
    std::optional<RouteMetadata> route_metadata;
    if (parsed_url.valid) {
        const std::string path = path_from_url(parsed_url);
        const std::string method = normalize_method(std::string(req.method_string()));
        const std::string key = method + " " + path;
        const auto route = routes_.find(key);
        if (route != routes_.end()) {
            matched_route = &route->second;
            route_metadata = RouteMetadata{method, path, route->second.access};
        }
    }
    const bool authentication_required = matched_route &&
        matched_route->access == RouteAccess::Authenticated;
    HttpRequestContext context(req, authentication_required, std::move(client_address),
        std::move(route_metadata));

    MiddlewareNext next = [this, matched_route, target_valid = parsed_url.valid,
                           target_error = parsed_url.error](HttpRequestContext& request_context) {
        return dispatch_request(request_context, matched_route, target_valid, target_error);
    };

    for (auto it = middleware_.rbegin(); it != middleware_.rend(); ++it) {
        Middleware current = *it;
        MiddlewareNext downstream = std::move(next);
        next = [current = std::move(current), downstream = std::move(downstream)](
            HttpRequestContext& request_context) {
            try {
                return current(request_context, downstream);
            } catch (const std::exception& error) {
                log_error("Router", "Middleware exception: " + std::string(error.what()));
                return HttpResponse{http_utils::make_error_response(
                    500, "Internal server error", request_context.request.version())};
            } catch (...) {
                log_error("Router", "Unknown middleware exception");
                return HttpResponse{http_utils::make_error_response(
                    500, "Internal server error", request_context.request.version())};
            }
        };
    }

    return next(context);
}

HttpResponse Router::dispatch_request(HttpRequestContext& context, const Route* route,
                                      bool valid_target, const std::string& target_error) const {
    const auto& req = context.request;
    if (!valid_target) {
        log_warning("Router", "Invalid request target");
        return http_utils::make_error_response(
            400, "Invalid request target: " + target_error, req.version());
    }

    if (route) {
        if (route->access == RouteAccess::Authenticated && !context.principal) {
            if (context.authentication_checked()) {
                auto response = http_utils::make_error_response(
                    401, "Authentication required", req.version());
                response.set(http::field::www_authenticate, context.authentication_challenge());
                return response;
            }
            log_error("Router", "Protected route has no configured authenticator");
            return http_utils::make_error_response(500, "Internal server error", req.version());
        }
        // Маршрут найден. Выполняем обработчик.
        try {
            return route->handler(context);
        } 
        catch (const std::exception& e) {
            log_error("Router", "Handler exception: " + std::string(e.what()));
            // Если обработчик упал с исключением, возвращаем 500
            return http_utils::make_error_response(
                500,
                "Internal server error",
                req.version()
            );
        } 
        catch (...) {
            log_error("Router", "Unknown handler exception");
            // Перехват любых других неизвестных исключений
            return http_utils::make_error_response(500, "Unknown internal error", req.version());
        }
    }

    // Маршрут не найден, вызываем обработчик по умолчанию
    try {
        return default_handler_(context);
    } catch (const std::exception& error) {
        log_error("Router", "Not-found handler exception: " + std::string(error.what()));
        return http_utils::make_error_response(500, "Internal server error", req.version());
    } catch (...) {
        log_error("Router", "Unknown not-found handler exception");
        return http_utils::make_error_response(500, "Internal server error", req.version());
    }
}
