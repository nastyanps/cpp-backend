#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>

namespace json_loader {

// Константы для ключей JSON
namespace keys {
    constexpr std::string_view X0 = "x0";
    constexpr std::string_view Y0 = "y0";
    constexpr std::string_view X1 = "x1";
    constexpr std::string_view Y1 = "y1";
    constexpr std::string_view X  = "x";
    constexpr std::string_view Y  = "y";
    constexpr std::string_view W  = "w";
    constexpr std::string_view H  = "h";
    constexpr std::string_view ID = "id";
    constexpr std::string_view OFFSET_X = "offsetX";
    constexpr std::string_view OFFSET_Y = "offsetY";
}  // namespace keys

void ParseRoads(const boost::json::array& roads, model::Map& map) {
    for (const auto& road_val : roads) {
        const auto& road_obj = road_val.as_object();
        int x0 = road_obj.at(keys::X0).as_int64();
        int y0 = road_obj.at(keys::Y0).as_int64();
        if (road_obj.contains(keys::X1)) {
            int x1 = road_obj.at(keys::X1).as_int64();
            map.AddRoad(model::Road(model::Road::HORIZONTAL,
                                    model::Point{x0, y0}, x1));
        } else {
            int y1 = road_obj.at(keys::Y1).as_int64();
            map.AddRoad(model::Road(model::Road::VERTICAL,
                                    model::Point{x0, y0}, y1));
        }
    }
}

void ParseBuildings(const boost::json::array& buildings, model::Map& map) {
    for (const auto& building_val : buildings) {
        const auto& b = building_val.as_object();
        int x = b.at(keys::X).as_int64();
        int y = b.at(keys::Y).as_int64();
        int w = b.at(keys::W).as_int64();
        int h = b.at(keys::H).as_int64();
        map.AddBuilding(model::Building(
            model::Rectangle{model::Point{x, y}, model::Size{w, h}}));
    }
}

void ParseOffices(const boost::json::array& offices, model::Map& map) {
    for (const auto& office_val : offices) {
        const auto& o = office_val.as_object();
        std::string oid = std::string(o.at(keys::ID).as_string());
        int x  = o.at(keys::X).as_int64();
        int y  = o.at(keys::Y).as_int64();
        int dx = o.at(keys::OFFSET_X).as_int64();
        int dy = o.at(keys::OFFSET_Y).as_int64();
        map.AddOffice(model::Office(
            model::Office::Id{oid},
            model::Point{x, y},
            model::Offset{dx, dy}));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path, bool randomize_spawn_points,
                      extra_data::LootTypesData& extra_data) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    auto value = boost::json::parse(content);
    const auto& obj  = value.as_object();
    const auto& maps = obj.at("maps").as_array();

    double default_dog_speed = 1.0;
    if (obj.contains("defaultDogSpeed")) {
        default_dog_speed = obj.at("defaultDogSpeed").as_double();
    }

    unsigned default_bag_capacity = 3;
    if (obj.contains("defaultBagCapacity")) {
        default_bag_capacity = static_cast<unsigned>(obj.at("defaultBagCapacity").as_int64());
    }

    double loot_period_seconds = 5.0;
    double loot_probability = 0.5;
    if (obj.contains("lootGeneratorConfig")) {
        const auto& loot_cfg = obj.at("lootGeneratorConfig").as_object();
        loot_period_seconds = loot_cfg.at("period").as_double();
        loot_probability = loot_cfg.at("probability").as_double();
    }
    auto loot_period = std::chrono::milliseconds(
        static_cast<int64_t>(loot_period_seconds * 1000));

    model::Game game(randomize_spawn_points, loot_period, loot_probability);

    for (const auto& map_val : maps) {
        const auto& map_obj = map_val.as_object();

        std::string id   = std::string(map_obj.at(keys::ID).as_string());
        std::string name = std::string(map_obj.at("name").as_string());

        model::Map map(model::Map::Id{id}, name);

        double dog_speed = default_dog_speed;
        if (map_obj.contains("dogSpeed")) {
            dog_speed = map_obj.at("dogSpeed").as_double();
        }
        map.SetDogSpeed(dog_speed);

	unsigned bag_capacity = default_bag_capacity;
	if (map_obj.contains("bagCapacity")) {
	    bag_capacity = static_cast<unsigned>(map_obj.at("bagCapacity").as_int64());
	}
	map.SetBagCapacity(bag_capacity);

        if (map_obj.contains("lootTypes")) {
            const auto& loot_types = map_obj.at("lootTypes").as_array();
            map.SetLootTypeCount(static_cast<unsigned>(loot_types.size()));
            extra_data.AddLootTypes(id, loot_types);

            std::vector<unsigned> loot_values;
            loot_values.reserve(loot_types.size());
            for (const auto& loot_type_val : loot_types) {
                const auto& loot_type_obj = loot_type_val.as_object();
                unsigned item_value = 0;
                if (loot_type_obj.contains("value")) {
                    item_value = static_cast<unsigned>(loot_type_obj.at("value").as_int64());
                }
                loot_values.push_back(item_value);
            }
            map.SetLootValues(std::move(loot_values));
        }
	
        ParseRoads(map_obj.at("roads").as_array(), map);
        ParseBuildings(map_obj.at("buildings").as_array(), map);
        ParseOffices(map_obj.at("offices").as_array(), map);

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
