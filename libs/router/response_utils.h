#pragma once

#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include "http_types.h"

namespace beast = boost::beast;
namespace http = beast::http;

namespace http_utils {

    inline HttpStringResponse make_error_response(
        int status,
        std::string_view message,
        int version);

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
    inline HttpResponse make_file_response(
        int status, 
        const std::filesystem::path& root_directory,
        const std::filesystem::path& relative_path,
        int version = 11) 
    {
        namespace fs = std::filesystem;
        if (relative_path.empty() || relative_path.is_absolute()) {
            return make_error_response(400, "Invalid file path", version);
        }
        for (const auto& component : relative_path) {
            if (component == "..") {
                return make_error_response(400, "Invalid file path", version);
            }
        }

        std::error_code path_ec;
        const fs::path canonical_root = fs::canonical(root_directory, path_ec);
        if (path_ec) {
            return make_error_response(500, "Unable to open file", version);
        }
        const fs::path canonical_file = fs::canonical(canonical_root / relative_path, path_ec);
        if (path_ec) {
            if (path_ec == std::errc::no_such_file_or_directory ||
                path_ec == std::errc::not_a_directory) {
                return make_error_response(404, "File not found", version);
            }
            if (path_ec == std::errc::permission_denied) {
                return make_error_response(403, "File access denied", version);
            }
            return make_error_response(500, "Unable to open file", version);
        }

        auto root_it = canonical_root.begin();
        auto file_it = canonical_file.begin();
        for (; root_it != canonical_root.end() && file_it != canonical_file.end(); ++root_it, ++file_it) {
            if (*root_it != *file_it) {
                return make_error_response(403, "File access denied", version);
            }
        }
        if (root_it != canonical_root.end()) {
            return make_error_response(403, "File access denied", version);
        }

        // file_body требует особой инициализации через piecewise_construct
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(http::int_to_status(status), version)
        };
        
        res.set(http::field::server, "miniserver/1.0");
        // В реальном проекте здесь можно добавить логику определения MIME-типа по расширению файла
        res.set(http::field::content_type, "application/octet-stream");
        
        // File-open failures are converted to ordinary HTTP error responses below.
        beast::error_code ec;
        res.body().open(canonical_file.string().c_str(), beast::file_mode::scan, ec);
        if (ec) {
            if (ec == boost::system::errc::no_such_file_or_directory) {
                return make_error_response(404, "File not found", version);
            }
            if (ec == boost::system::errc::permission_denied) {
                return make_error_response(403, "File access denied", version);
            }
            return make_error_response(500, "Unable to open file", version);
        }
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
    inline HttpStringResponse make_error_response(
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
