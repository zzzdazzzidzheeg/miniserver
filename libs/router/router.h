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
    using Handler = RequestHandler;

    Router();

    // Регистрация маршрута
    void add_route(std::string method, std::string path, Handler handler);
    void add_middleware(Middleware middleware);
    
    // Основной метод маршрутизации
    HttpResponse route(const http::request<http::string_body>& req) const;
    void setup_router();

private:
    HttpResponse route_request(const HttpRequest& req) const;

    // Ключ: "METHOD PATH" (например, "GET /api/status")
    std::unordered_map<std::string, Handler> routes_;
    std::vector<Middleware> middleware_;
    
    // Обработчик по умолчанию (404)
    Handler default_handler_;
};
