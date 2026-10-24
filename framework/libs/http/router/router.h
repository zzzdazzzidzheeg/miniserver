#pragma once

#include <boost/beast.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "http_types.h"
#include "middleware.h"
#include "url_parser.h"

namespace beast = boost::beast;
namespace http = beast::http;

class Router : public std::enable_shared_from_this<Router> {
public:
    // Тип обработчика: функция, принимающая запрос и возвращающая ответ
    using Handler = RouteHandler;

    Router();

    // Регистрация маршрута
    void add_route(std::string method, std::string path, RouteAccess access, Handler handler,
                   std::optional<RouteRateLimit> rate_limit = std::nullopt);
    void add_middleware(Middleware middleware);
    void set_not_found_handler(Handler handler);
    
    // Основной метод маршрутизации
    HttpResponse route(const http::request<http::string_body>& req,
                       std::string client_address = {}) const;
    void setup_router();

private:
    struct Route {
        Handler handler;
        RouteAccess access;
        std::optional<RouteRateLimit> rate_limit;
    };

    HttpResponse dispatch_request(HttpRequestContext& context, const Route* route,
                                  bool valid_target, const std::string& target_error) const;

    // Ключ: "METHOD PATH" (например, "GET /api/status")
    std::unordered_map<std::string, Route> routes_;
    std::vector<Middleware> middleware_;
    
    // Обработчик по умолчанию (404)
    Handler default_handler_;
};
