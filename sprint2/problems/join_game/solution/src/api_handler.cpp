#include "api_handler.h"
#include <cctype>

namespace http_handler {

std::string MakeErrorBody(std::string_view code, std::string_view message) {
    json::object obj;
    obj["code"] = code;
    obj["message"] = message;
    return json::serialize(obj);
}

StringResponse MakeJsonResponse(http::status status, std::string body, unsigned version, bool keep_alive,
                                  std::string_view allow) {
    StringResponse response(status, version);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    if (!allow.empty()) {
        response.set(http::field::allow, allow);
    }
    response.body() = std::move(body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

std::optional<app::Token> ApiHandler::TryExtractToken(const StringRequest& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return std::nullopt;
    }
    std::string value(it->value());
    const std::string prefix = "Bearer "s;
    if (value.size() <= prefix.size() || value.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    std::string token_str = value.substr(prefix.size());
    if (token_str.size() != 32) {
        return std::nullopt;
    }
    for (char c : token_str) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return std::nullopt;
        }
    }
    return app::Token{token_str};
}

StringResponse ApiHandler::HandleApiRequest(const StringRequest& req) {
    std::string target(req.target());
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (target == "/api/v1/game/join") {
        return HandleJoinRequest(req);
    }
    if (target == "/api/v1/game/players") {
        return HandlePlayersRequest(req);
    }
    if (target.starts_with("/api/v1/maps")) {
        return HandleMapsRequest(req, target);
    }

    return MakeJsonResponse(http::status::bad_request,
                             MakeErrorBody("badRequest", "Bad request"), version, keep_alive);
}

StringResponse ApiHandler::HandleMapsRequest(const StringRequest& req, std::string_view target) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (target == "/api/v1/maps") {
        json::array maps_array;
        for (const auto& map : game_.GetMaps()) {
            json::object m;
            m["id"] = *map.GetId();
            m["name"] = map.GetName();
            maps_array.push_back(std::move(m));
        }
        return MakeJsonResponse(http::status::ok, json::serialize(maps_array), version, keep_alive);
    }

    if (target.starts_with("/api/v1/maps/")) {
        std::string map_id = std::string(target.substr(std::string("/api/v1/maps/").size()));
        const model::Map* map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            return MakeJsonResponse(http::status::not_found,
                                     MakeErrorBody("mapNotFound", "Map not found"), version, keep_alive);
        }
        json::object map_obj;
        map_obj["id"] = *map->GetId();
        map_obj["name"] = map->GetName();

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

        return MakeJsonResponse(http::status::ok, json::serialize(map_obj), version, keep_alive);
    }

    return MakeJsonResponse(http::status::bad_request,
                             MakeErrorBody("badRequest", "Bad request"), version, keep_alive);
}

StringResponse ApiHandler::HandleJoinRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::post) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Only POST method is expected"),
                                 version, keep_alive, "POST"sv);
    }

    std::string user_name;
    std::string map_id_str;
    try {
        auto value = json::parse(req.body());
        const auto& obj = value.as_object();
        user_name = std::string(obj.at("userName").as_string());
        map_id_str = std::string(obj.at("mapId").as_string());
    } catch (...) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Join game request parse error"),
                                 version, keep_alive);
    }

    if (user_name.empty()) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Invalid name"),
                                 version, keep_alive);
    }

    model::Map::Id map_id{map_id_str};
    const model::Map* map = game_.FindMap(map_id);
    if (!map) {
        return MakeJsonResponse(http::status::not_found,
                                 MakeErrorBody("mapNotFound", "Map not found"),
                                 version, keep_alive);
    }

    model::GameSession& session = game_.FindOrCreateSession(map_id);
    model::Dog& dog = session.AddDog(user_name);
    app::Player& player = players_.Add(&dog, &session);
    app::Token token = tokens_.AddPlayer(player);

    json::object body;
    body["authToken"] = *token;
    body["playerId"] = dog.GetId();

    return MakeJsonResponse(http::status::ok, json::serialize(body), version, keep_alive);
}

StringResponse ApiHandler::HandlePlayersRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Invalid method"),
                                 version, keep_alive, "GET, HEAD"sv);
    }

    auto token_opt = TryExtractToken(req);
    if (!token_opt) {
        return MakeJsonResponse(http::status::unauthorized,
                                 MakeErrorBody("invalidToken", "Authorization header is missing"),
                                 version, keep_alive);
    }

    app::Player* player = tokens_.FindPlayerByToken(*token_opt);
    if (!player) {
        return MakeJsonResponse(http::status::unauthorized,
                                 MakeErrorBody("unknownToken", "Player token has not been found"),
                                 version, keep_alive);
    }

    model::GameSession* session = player->GetSession();
    json::object result;
    for (const auto& dog_ptr : session->GetDogs()) {
        json::object entry;
        entry["name"] = dog_ptr->GetName();
        result[std::to_string(dog_ptr->GetId())] = std::move(entry);
    }

    return MakeJsonResponse(http::status::ok, json::serialize(result), version, keep_alive);
}

}  // namespace http_handler
