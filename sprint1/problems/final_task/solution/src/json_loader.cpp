#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    auto value = boost::json::parse(content);
    const auto& obj = value.as_object();
    const auto& maps = obj.at("maps").as_array();

    model::Game game;

    for (const auto& map_val : maps) {
        const auto& map_obj = map_val.as_object();

        std::string id = std::string(map_obj.at("id").as_string());
        std::string name = std::string(map_obj.at("name").as_string());

        model::Map map(model::Map::Id{id}, name);

        // Дороги
        for (const auto& road_val : map_obj.at("roads").as_array()) {
            const auto& road_obj = road_val.as_object();
            int x0 = road_obj.at("x0").as_int64();
            int y0 = road_obj.at("y0").as_int64();

            if (road_obj.contains("x1")) {
                // Горизонтальная дорога
                int x1 = road_obj.at("x1").as_int64();
                map.AddRoad(model::Road(model::Road::HORIZONTAL,
                                        model::Point{x0, y0}, x1));
            } else {
                // Вертикальная дорога
                int y1 = road_obj.at("y1").as_int64();
                map.AddRoad(model::Road(model::Road::VERTICAL,
                                        model::Point{x0, y0}, y1));
            }
        }

        // Здания
        for (const auto& building_val : map_obj.at("buildings").as_array()) {
            const auto& b = building_val.as_object();
            int x = b.at("x").as_int64();
            int y = b.at("y").as_int64();
            int w = b.at("w").as_int64();
            int h = b.at("h").as_int64();
            map.AddBuilding(model::Building(
                model::Rectangle{model::Point{x, y}, model::Size{w, h}}));
        }

        // Офисы
        for (const auto& office_val : map_obj.at("offices").as_array()) {
            const auto& o = office_val.as_object();
            std::string oid = std::string(o.at("id").as_string());
            int x = o.at("x").as_int64();
            int y = o.at("y").as_int64();
            int dx = o.at("offsetX").as_int64();
            int dy = o.at("offsetY").as_int64();
            map.AddOffice(model::Office(
                model::Office::Id{oid},
                model::Point{x, y},
                model::Offset{dx, dy}));
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader