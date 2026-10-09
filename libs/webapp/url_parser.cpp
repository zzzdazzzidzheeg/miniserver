#include "url_parser.h"

#include <utility>

namespace {
constexpr std::size_t max_url_length = 8 * 1024;
constexpr std::size_t max_query_parameters = 128;
constexpr std::size_t max_path_segments = 128;
constexpr std::size_t max_component_length = 2 * 1024;

int hex_value(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool decode_component(std::string_view encoded, std::string& decoded, bool plus_as_space) {
    decoded.clear();
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(encoded[index]);
        if (byte == '%') {
            if (index + 2 >= encoded.size()) return false;
            const int high = hex_value(static_cast<unsigned char>(encoded[index + 1]));
            const int low = hex_value(static_cast<unsigned char>(encoded[index + 2]));
            if (high < 0 || low < 0) return false;
            decoded.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        } else if (plus_as_space && byte == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(static_cast<char>(byte));
        }
    }
    return true;
}

bool has_control_character(std::string_view value) {
    for (unsigned char byte : value) {
        if (byte < 0x20 || byte == 0x7f) return true;
    }
    return false;
}

ParsedUrl invalid_url(std::string message) {
    ParsedUrl result;
    result.valid = false;
    result.error = std::move(message);
    return result;
}
} // namespace

std::string decode_percent_encoding(std::string_view encoded_text) {
    std::string decoded;
    if (!decode_component(encoded_text, decoded, true)) return {};
    return decoded;
}

ParsedUrl parse_url(std::string_view raw_request_url) {
    if (raw_request_url.empty()) return invalid_url("empty request target");
    if (raw_request_url.size() > max_url_length) return invalid_url("request target too long");
    if (has_control_character(raw_request_url)) return invalid_url("control character in request target");
    if (raw_request_url == "*") {
        ParsedUrl result;
        result.is_asterisk = true;
        return result;
    }

    std::string absolute_target;
    std::string_view target = raw_request_url;
    const std::size_t scheme_separator = raw_request_url.find("://");
    if (scheme_separator != std::string_view::npos) {
        const std::size_t authority_start = scheme_separator + 3;
        const std::size_t path_start = raw_request_url.find_first_of("/?", authority_start);
        if (path_start != std::string_view::npos && raw_request_url[path_start] == '?') {
            absolute_target = "/";
            absolute_target.append(raw_request_url.substr(path_start));
            target = absolute_target;
        } else {
            target = path_start == std::string_view::npos
            ? std::string_view("/") : raw_request_url.substr(path_start);
        }
        if (target.empty()) return invalid_url("invalid absolute request target");
    }
    if (target.front() != '/') return invalid_url("unsupported request target form");

    const std::size_t query_separator = target.find('?');
    const std::string_view raw_path = target.substr(0, query_separator);
    const std::string_view raw_query = query_separator == std::string_view::npos
        ? std::string_view{} : target.substr(query_separator + 1);
    ParsedUrl result;

    std::size_t segment_start = 0;
    while (segment_start < raw_path.size()) {
        while (segment_start < raw_path.size() && raw_path[segment_start] == '/') ++segment_start;
        if (segment_start == raw_path.size()) break;
        const std::size_t segment_end = raw_path.find('/', segment_start);
        const std::size_t end = segment_end == std::string_view::npos ? raw_path.size() : segment_end;
        const std::string_view raw_segment = raw_path.substr(segment_start, end - segment_start);
        if (raw_segment.size() > max_component_length) return invalid_url("path segment too long");

        std::string segment;
        if (!decode_component(raw_segment, segment, false)) return invalid_url("invalid percent escape");
        if (segment == "." || segment == "..") return invalid_url("dot path segment is not allowed");
        if (has_control_character(segment)) return invalid_url("control character in path");
        result.path_segments.push_back(std::move(segment));
        if (result.path_segments.size() > max_path_segments) return invalid_url("too many path segments");
        segment_start = end;
    }

    std::size_t parameter_start = 0;
    std::size_t parameter_count = 0;
    while (parameter_start < raw_query.size()) {
        const std::size_t parameter_end = raw_query.find('&', parameter_start);
        const std::size_t end = parameter_end == std::string_view::npos ? raw_query.size() : parameter_end;
        const std::string_view pair = raw_query.substr(parameter_start, end - parameter_start);
        if (pair.size() > max_component_length) return invalid_url("query parameter too long");
        if (!pair.empty()) {
            if (++parameter_count > max_query_parameters) return invalid_url("too many query parameters");
            const std::size_t separator = pair.find('=');
            const std::string_view raw_key = pair.substr(0, separator);
            const std::string_view raw_value = separator == std::string_view::npos
                ? std::string_view{} : pair.substr(separator + 1);
            std::string key;
            std::string value;
            if (!decode_component(raw_key, key, true) || !decode_component(raw_value, value, true)) {
                return invalid_url("invalid percent escape in query");
            }
            if (has_control_character(key) || has_control_character(value)) {
                return invalid_url("control character in query");
            }
            result.query_params.insert_or_assign(std::move(key), std::move(value));
        }
        parameter_start = end == raw_query.size() ? raw_query.size() : end + 1;
    }
    return result;
}
