#include "request_handler.h"

namespace http_handler {

std::string MakeErrorBody(std::string_view code, std::string_view message) {
    json::object obj;
    obj["code"] = code;
    obj["message"] = message;
    return json::serialize(obj);
}

http::response<http::string_body> MakeJsonResponse(
        http::status status,
        std::string body,
        unsigned version,
        bool keep_alive) {
    http::response<http::string_body> response(status, version);
    response.set(http::field::content_type, "application/json");
    response.body() = std::move(body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

http::response<http::string_body> HandleApiRequest(
        const std::string& target,
        unsigned version,
        bool keep_alive,
        model::Game& game) {

    // GET /api/v1/maps — список всех карт
    if (target == "/api/v1/maps") {
        json::array maps_array;
        for (const auto& map : game.GetMaps()) {
            json::object m;
            m["id"] = *map.GetId();
            m["name"] = map.GetName();
            maps_array.push_back(std::move(m));
        }
        return MakeJsonResponse(http::status::ok,
                                json::serialize(maps_array),
                                version, keep_alive);
    }

    // GET /api/v1/maps/{id} — конкретная карта
    if (target.starts_with("/api/v1/maps/")) {
        std::string map_id = target.substr(std::string("/api/v1/maps/").size());
        const model::Map* map = game.FindMap(model::Map::Id{map_id});

        if (!map) {
            return MakeJsonResponse(http::status::not_found,
                                    MakeErrorBody("mapNotFound", "Map not found"),
                                    version, keep_alive);
        }

        json::object map_obj;
        map_obj["id"] = *map->GetId();
        map_obj["name"] = map->GetName();

        // Дороги
        json::array roads;
        for (const auto& road : map->GetRoads()) {
            json::object r;
            r["x0"] = road.GetStart().x;
            r["y0"] = road.GetStart().y;
            if (road.IsHorizontal()) {
                r["x1"] = road.GetEnd().x;
            } else {
                r["y1"] = road.GetEnd().y;
            }
            roads.push_back(std::move(r));
        }
        map_obj["roads"] = std::move(roads);

        // Здания
        json::array buildings;
        for (const auto& building : map->GetBuildings()) {
            json::object b;
            b["x"] = building.GetBounds().position.x;
            b["y"] = building.GetBounds().position.y;
            b["w"] = building.GetBounds().size.width;
            b["h"] = building.GetBounds().size.height;
            buildings.push_back(std::move(b));
        }
        map_obj["buildings"] = std::move(buildings);

        // Офисы
        json::array offices;
        for (const auto& office : map->GetOffices()) {
            json::object o;
            o["id"] = *office.GetId();
            o["x"] = office.GetPosition().x;
            o["y"] = office.GetPosition().y;
            o["offsetX"] = office.GetOffset().dx;
            o["offsetY"] = office.GetOffset().dy;
            offices.push_back(std::move(o));
        }
        map_obj["offices"] = std::move(offices);

        return MakeJsonResponse(http::status::ok,
                                json::serialize(map_obj),
                                version, keep_alive);
    }

    // Любой другой /api/... запрос
    return MakeJsonResponse(http::status::bad_request,
                            MakeErrorBody("badRequest", "Bad request"),
                            version, keep_alive);
}

http::response<http::string_body> MakeTextResponse(
        http::status status,
        std::string_view text,
        unsigned version,
        bool keep_alive) {
    http::response<http::string_body> response(status, version);
    response.set(http::field::content_type, "text/plain");
    response.body() = std::string(text);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

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
