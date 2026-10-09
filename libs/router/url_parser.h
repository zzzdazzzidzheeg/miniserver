#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ParsedUrl {
    std::vector<std::string> path_segments;
    std::unordered_map<std::string, std::string> query_params;
    bool valid = true;
    bool is_asterisk = false;
    std::string error;
};

// Decodes percent escapes and treats '+' as a space.
std::string decode_percent_encoding(std::string_view encoded_text);

// Parses origin-form, absolute-form, and asterisk-form HTTP request targets.
ParsedUrl parse_url(std::string_view raw_request_url);
