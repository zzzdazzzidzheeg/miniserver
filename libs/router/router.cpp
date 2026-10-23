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
    default_handler_ = [](const http::request<http::string_body>& req) {
        log_warning("Router: Route not found for " + std::string(req.method_string()) +
            " " + std::string(req.target()));
        return http_utils::make_text_response(
            404, 
            "Resource not found: " + std::string(req.target()), 
            req.version()
        );
    };
}

void Router::add_route(std::string method, std::string path, Handler handler) {
    std::string key = normalize_method(std::move(method)) + " " + std::move(path);
    routes_[std::move(key)] = std::move(handler);
}

void Router::add_middleware(Middleware middleware) {
    if (!middleware) throw std::invalid_argument("Middleware cannot be empty");
    middleware_.push_back(std::move(middleware));
}

HttpResponse Router::route(const http::request<http::string_body>& req) const {
    MiddlewareNext next = [this](const HttpRequest& request) {
        return route_request(request);
    };

    for (auto it = middleware_.rbegin(); it != middleware_.rend(); ++it) {
        Middleware current = *it;
        MiddlewareNext downstream = std::move(next);
        next = [current = std::move(current), downstream = std::move(downstream)](
            const HttpRequest& request) {
            try {
                return current(request, downstream);
            } catch (const std::exception& error) {
                log_error("Router: Middleware exception: " + std::string(error.what()));
                return HttpResponse{http_utils::make_error_response(
                    500, "Internal server error", request.version())};
            } catch (...) {
                log_error("Router: Unknown middleware exception");
                return HttpResponse{http_utils::make_error_response(
                    500, "Internal server error", request.version())};
            }
        };
    }

    return next(req);
}

HttpResponse Router::route_request(const http::request<http::string_body>& req) const {
    const ParsedUrl parsed_url = parse_url(req.target());
    if (!parsed_url.valid) {
        log_warning("Router: Invalid request target");
        return http_utils::make_error_response(
            400, "Invalid request target: " + parsed_url.error, req.version());
    }

    const std::string path = path_from_url(parsed_url);
    const std::string key = normalize_method(std::string(req.method_string())) + " " + path;


    // Ищем обработчик
    auto it = routes_.find(key);
    
    if (it != routes_.end()) {
        // Маршрут найден. Выполняем обработчик.
        try {
            return it->second(req);
        } 
        catch (const std::exception& e) {
            log_error("Router: Handler exception: " + std::string(e.what()));
            // Если обработчик упал с исключением, возвращаем 500
            return http_utils::make_error_response(
                500,
                "Internal server error",
                req.version()
            );
        } 
        catch (...) {
            log_error("Router: Unknown handler exception");
            // Перехват любых других неизвестных исключений
            return http_utils::make_error_response(500, "Unknown internal error", req.version());
        }
    }

    // Маршрут не найден, вызываем обработчик по умолчанию
    return default_handler_(req);
}
