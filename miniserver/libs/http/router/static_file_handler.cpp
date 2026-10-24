#include "static_file_handler.h"

#include "response_utils.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace {

std::string content_type_for(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
    if (extension == ".json") return "application/json";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool is_api_path(const ParsedUrl& parsed) {
    return !parsed.path_segments.empty() && parsed.path_segments.front() == "api";
}

} // namespace

Router::Handler make_static_file_handler(std::filesystem::path document_root) {
    std::error_code error;
    const auto canonical_root = std::filesystem::canonical(document_root, error);
    if (error || !std::filesystem::is_directory(canonical_root, error) || error) {
        throw std::invalid_argument("Static document root must be an existing directory");
    }

    return [root = canonical_root](HttpRequestContext& context) -> HttpResponse {
        const auto& request = context.request;
        if (request.method() != http::verb::get) {
            auto response = http_utils::make_error_response(
                405, "Method not allowed", request.version());
            response.set(http::field::allow, "GET");
            return response;
        }

        const ParsedUrl parsed = parse_url(request.target());
        if (!parsed.valid) {
            return http_utils::make_error_response(400, "Invalid request target", request.version());
        }
        if (is_api_path(parsed)) {
            return http_utils::make_error_response(404, "Resource not found", request.version());
        }

        std::filesystem::path relative_path;
        for (const auto& segment : parsed.path_segments) relative_path /= segment;
        if (relative_path.empty()) relative_path = "index.html";

        std::error_code path_error;
        const auto requested_path = root / relative_path;
        if (std::filesystem::is_directory(requested_path, path_error) && !path_error) {
            relative_path /= "index.html";
        }
        path_error.clear();
        if (!std::filesystem::is_regular_file(root / relative_path, path_error) || path_error) {
            return http_utils::make_text_response(404, "File not found", request.version());
        }

        HttpResponse response = http_utils::make_file_response(
            200, root, relative_path, request.version());
        if (auto* file_response = std::get_if<HttpFileResponse>(&response)) {
            file_response->set(http::field::content_type, content_type_for(relative_path));
            file_response->set("X-Content-Type-Options", "nosniff");
        }
        return response;
    };
}
