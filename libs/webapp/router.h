#pragma once

#include <boost/beast.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include "url_parser.h"

namespace beast = boost::beast;
namespace http = beast::http;

// Используем string_body как базовый. 
// Если позже понадобится file_body, можно заменить на std::variant.
using HttpResponse = http::response<http::string_body>;

class Router : public std::enable_shared_from_this<Router> {
public:
    // Тип обработчика: функция, принимающая запрос и возвращающая ответ
    using Handler = std::function<HttpResponse(const http::request<http::string_body>&)>;

    Router();

    // Регистрация маршрута
    void add_route(std::string method, std::string path, Handler handler);
    
    // Основной метод маршрутизации
    HttpResponse route(const http::request<http::string_body>& req) const;
    void setup_router();

private:
    // Ключ: "METHOD PATH" (например, "GET /api/status")
    std::unordered_map<std::string, Handler> routes_;
    
    // Обработчик по умолчанию (404)
    Handler default_handler_;
};