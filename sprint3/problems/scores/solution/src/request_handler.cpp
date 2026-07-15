#include "request_handler.h"

namespace http_handler {

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    auto hex_to_int = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hi = hex_to_int(str[i + 1]);
            int lo = hex_to_int(str[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += static_cast<char>(hi * 16 + lo);
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            result += ' ';
            continue;
        }
        result += str[i];
    }
    return result;
}

std::string_view MimeTypeByExtension(std::string ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    static const std::unordered_map<std::string, std::string_view> types = {
        {".htm", "text/html"}, {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"}, {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"}, {".tif", "image/tiff"},
        {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };
    auto it = types.find(ext);
    if (it != types.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

}  // namespace http_handler
