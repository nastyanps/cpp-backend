#include "api_handler.h"
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <chrono>

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

namespace {
std::string DirectionToString(model::Direction dir) {
    switch (dir) {
        case model::Direction::NORTH: return "U";
        case model::Direction::SOUTH: return "D";
        case model::Direction::WEST: return "L";
        case model::Direction::EAST: return "R";
    }
    assert(false);
    return "U";
}
}  // namespace

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
    if (std::any_of(token_str.begin(), token_str.end(), [](char c) {
               return !std::isxdigit(static_cast<unsigned char>(c));
        })) {
        return std::nullopt;
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
    if (target == "/api/v1/game/state") {
        return HandleStateRequest(req);
    }
    if (target == "/api/v1/game/player/action") {
        return HandleActionRequest(req);
    }
    if (target.starts_with("/api/v1/maps")) {
        return HandleMapsRequest(req, target);
    }
    if (target == "/api/v1/game/tick") {
        return HandleTickRequest(req);
    }

    return MakeJsonResponse(http::status::bad_request,
                             MakeErrorBody("badRequest", "Bad request"), version, keep_alive);
}

StringResponse ApiHandler::HandleMapsRequest(const StringRequest& req, std::string_view target) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (target == "/api/v1/maps") {
        json::array maps_array;
        for (const auto& map : application_.ListMaps()) {
            json::object m;
            m["id"] = *map.GetId();
            m["name"] = map.GetName();
            maps_array.push_back(std::move(m));
        }
        return MakeJsonResponse(http::status::ok, json::serialize(maps_array), version, keep_alive);
    }

    if (target.starts_with("/api/v1/maps/")) {
        std::string map_id = std::string(target.substr(std::string("/api/v1/maps/").size()));
        const model::Map* map = application_.FindMap(model::Map::Id{map_id});
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
	
	if (const auto* loot_types = extra_data_.FindLootTypes(map_id)) {
    	    map_obj["lootTypes"] = *loot_types;
	}
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

    try {
        auto result = application_.JoinGame(map_id_str, user_name);

        json::object body;
        body["authToken"] = *result.token;
        body["playerId"] = result.player_id;

        return MakeJsonResponse(http::status::ok, json::serialize(body), version, keep_alive);
    } catch (const app::JoinGameError& e) {
        if (e.GetReason() == app::JoinGameErrorReason::InvalidName) {
            return MakeJsonResponse(http::status::bad_request,
                                     MakeErrorBody("invalidArgument", "Invalid name"),
                                     version, keep_alive);
        }
        return MakeJsonResponse(http::status::not_found,
                                 MakeErrorBody("mapNotFound", "Map not found"),
                                 version, keep_alive);
    }
}

StringResponse ApiHandler::HandlePlayersRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Invalid method"),
                                 version, keep_alive, "GET, HEAD"sv);
    }

    return ExecuteAuthorized(req, [&](app::Player& player) {
        model::GameSession* session = player.GetSession();
        json::object result;
        for (const auto& dog_ptr : session->GetDogs()) {
            json::object entry;
            entry["name"] = dog_ptr->GetName();
            result[std::to_string(dog_ptr->GetId())] = std::move(entry);
        }
        return MakeJsonResponse(http::status::ok, json::serialize(result), version, keep_alive);
    });
}

StringResponse ApiHandler::HandleStateRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Invalid method"),
                                 version, keep_alive, "GET, HEAD"sv);
    }

    return ExecuteAuthorized(req, [&](app::Player& player) {
        model::GameSession* session = player.GetSession();
        json::object players_obj;
        for (const auto& dog_ptr : session->GetDogs()) {
            json::object entry;

            json::array pos;
            pos.push_back(dog_ptr->GetPosition().x);
            pos.push_back(dog_ptr->GetPosition().y);
            entry["pos"] = std::move(pos);

            json::array speed;
            speed.push_back(dog_ptr->GetSpeed().vx);
            speed.push_back(dog_ptr->GetSpeed().vy);
            entry["speed"] = std::move(speed);

            entry["dir"] = DirectionToString(dog_ptr->GetDirection());

            players_obj[std::to_string(dog_ptr->GetId())] = std::move(entry);
        }

	json::object lost_objects_obj;
	for (const auto& lo : session->GetLostObjects()) {
    	    json::object entry;
    	    entry["type"] = lo.type;
    	    json::array pos;
    	    pos.push_back(lo.pos.x);
    	    pos.push_back(lo.pos.y);
    	    entry["pos"] = std::move(pos);
    	    lost_objects_obj[std::to_string(lo.id)] = std::move(entry);
	}
        json::object result;
        result["players"] = std::move(players_obj);
	result["lostObjects"] = std::move(lost_objects_obj);
        return MakeJsonResponse(http::status::ok, json::serialize(result), version, keep_alive);
    });
}

StringResponse ApiHandler::HandleActionRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::post) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Invalid method"),
                                 version, keep_alive, "POST"sv);
    }

    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() || content_type_it->value() != "application/json"sv) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Invalid content type"),
                                 version, keep_alive);
    }

    std::string move;
    try {
        auto value = json::parse(req.body());
        const auto& obj = value.as_object();
        move = std::string(obj.at("move").as_string());
        if (move != "L" && move != "R" && move != "U" && move != "D" && !move.empty()) {
            throw std::invalid_argument("bad move");
        }
    } catch (...) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Failed to parse action"),
                                 version, keep_alive);
    }

    return ExecuteAuthorized(req, [&](app::Player& player) {
        application_.SetPlayerDirection(player, move);
        return MakeJsonResponse(http::status::ok, "{}", version, keep_alive);
    });
}

StringResponse ApiHandler::HandleTickRequest(const StringRequest& req) {
    unsigned version = req.version();
    bool keep_alive = req.keep_alive();

    if (!application_.IsManualTickAllowed()) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("badRequest", "Invalid endpoint"),
                                 version, keep_alive);
    }

    if (req.method() != http::verb::post) {
        return MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorBody("invalidMethod", "Invalid method"),
                                 version, keep_alive, "POST"sv);
    }

    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() || content_type_it->value() != "application/json"sv) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Failed to parse tick request JSON"),
                                 version, keep_alive);
    }

    int64_t time_delta_ms = 0;
    try {
        auto value = json::parse(req.body());
        const auto& obj = value.as_object();
        time_delta_ms = obj.at("timeDelta").as_int64();
    } catch (...) {
        return MakeJsonResponse(http::status::bad_request,
                                 MakeErrorBody("invalidArgument", "Failed to parse tick request JSON"),
                                 version, keep_alive);
    }

   
    application_.Tick(std::chrono::milliseconds(time_delta_ms));

    return MakeJsonResponse(http::status::ok, "{}", version, keep_alive);
}

}  // namespace http_handler
