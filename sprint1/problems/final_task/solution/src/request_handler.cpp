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

}  // namespace http_handler
