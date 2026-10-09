#pragma once

#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace beast = boost::beast;
namespace http = beast::http;

namespace http_utils {

    // ---------------------------------------------------------
    // 1. Текстовый ответ (Plain Text)
    // ---------------------------------------------------------
    inline http::response<http::string_body> make_text_response(
        int status, 
        std::string_view text, 
        int version = 11) 
    {
        http::response<http::string_body> res{http::int_to_status(status), version};
        res.set(http::field::server, "miniserver/1.0");
        res.set(http::field::content_type, "text/plain");
        res.set("X-Content-Type-Options", "nosniff");
        res.set("Content-Security-Policy", "default-src 'none'");
        res.body() = std::string(text);
        res.prepare_payload(); // КРИТИЧЕСКИ ВАЖНО: вычисляет Content-Length
        return res;
    }

    // ---------------------------------------------------------
    // 2. JSON ответ (использует nlohmann::json)
    // ---------------------------------------------------------
    inline http::response<http::string_body> make_json_response(
        int status, 
        const nlohmann::json& json_data, 
        int version = 11) 
    {
        http::response<http::string_body> res{http::int_to_status(status), version};
        res.set(http::field::server, "miniserver/1.0");
        res.set(http::field::content_type, "application/json");
        res.set("X-Content-Type-Options", "nosniff");
        res.set("Content-Security-Policy", "default-src 'none'");
        // json.dump() превращает объект в строку. Можно использовать dump(2) для красивого форматирования
        res.body() = json_data.dump(); 
        res.prepare_payload();
        return res;
    }

    // ---------------------------------------------------------
    // 3. Файловый ответ (File Body)
    // ---------------------------------------------------------
    inline http::response<http::file_body> make_file_response(
        int status, 
        const std::string& file_path, 
        int version = 11) 
    {
        // file_body требует особой инициализации через piecewise_construct
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(http::int_to_status(status), version)
        };
        
        res.set(http::field::server, "miniserver/1.0");
        // В реальном проекте здесь можно добавить логику определения MIME-типа по расширению файла
        res.set(http::field::content_type, "application/octet-stream");
        
        // Открываем файл. Если файла нет, будет выброшено std::system_error
        beast::error_code ec;
        res.body().open(file_path.c_str(), beast::file_mode::scan, ec);
        res.prepare_payload();
        
        return res;
    }

    // ---------------------------------------------------------
    // 4. Пустой ответ (например, для 204 No Content или редиректов)
    // ---------------------------------------------------------
    inline http::response<http::empty_body> make_empty_response(
        int status, 
        int version = 11) 
    {
        http::response<http::empty_body> res{http::int_to_status(status), version};
        res.set(http::field::server, "miniserver/1.0");
        res.prepare_payload();
        return res;
    }

    // ---------------------------------------------------------
    // 5. Удобная обёртка для ошибок (возвращает JSON)
    // ---------------------------------------------------------
    inline http::response<http::string_body> make_error_response(
        int status, 
        std::string_view message, 
        int version = 11) 
    {
        nlohmann::json error_json = {
            {"error", true},
            {"status", status},
            {"message", std::string(message)}
        };
        return make_json_response(status, error_json, version);
    }

} // namespace http_utils