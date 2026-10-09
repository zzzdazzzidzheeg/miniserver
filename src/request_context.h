#pragma once

#include <boost/beast.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/encode.hpp>
#include <string>
#include <unordered_map>
#include <optional>

namespace http = boost::beast::http;
namespace urls = boost::urls;

struct RequestContext {
    // Ссылка на оригинальный запрос
    const http::request<http::string_body>& raw_request;
    
    // Разобранный URL (не владеет памятью, ссылается на target из raw_request)
    const urls::url_view& url;

    // Path-парzаметры (например, /users/:id -> {"id": "123"})
    std::unordered_map<std::string, std::string> path_params;

    // --- Удобные методы доступа ---

    // Получить path-параметр
    std::string get_path_param(const std::string& key, const std::string& default_val = "") const {
        auto it = path_params.find(key);
        return it != path_params.end() ? it->second : default_val;
    }

    // Получить query-параметр. url_view.params() возвращает итератор по парам key/value
    std::string get_query_param(const std::string& key, const std::string& default_val = "") const {
        auto it = url.params().find(key); // find() по ключу — O(1) для params_view
        return it != url.params().end() ? std::string(it->value) : default_val;
    }

    // Проверить наличие query-параметра (для флагов типа ?debug)
    bool has_query_param(const std::string& key) const {
        return url.params().find(key) != url.params().end();
    }

    // Получить путь без query и fragment
    std::string path() const {
        return std::string(url.path());
    }
};