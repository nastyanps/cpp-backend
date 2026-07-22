#include <catch2/catch_test_macros.hpp>
#include "../src/model.h"

using namespace std::literals;

SCENARIO("Loot generation on GameSession") {
    GIVEN("a map with a road and loot types") {
        model::Map map(model::Map::Id{"map1"s}, "Map 1"s);
        map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 40));
        map.SetLootTypeCount(2);

        model::GameSession session(&map, false, std::chrono::milliseconds(1000), 1.0);

        WHEN("a dog joins and enough time passes") {
            session.AddDog("Rex"s);
            session.Tick(std::chrono::milliseconds(2000), std::chrono::milliseconds(60000));

            THEN("loot count does not exceed looter count") {
                REQUIRE(session.GetLostObjects().size() <= session.GetDogs().size());
            }

            THEN("loot type is within valid range") {
                for (const auto& lo : session.GetLostObjects()) {
                    REQUIRE(lo.type < map.GetLootTypeCount());
                }
            }

            THEN("loot position is a valid double pair") {
                for (const auto& lo : session.GetLostObjects()) {
                    REQUIRE(lo.pos.x >= 0.0);
                    REQUIRE(lo.pos.y >= 0.0);
                }
            }
        }

        WHEN("no dogs are present on the map") {
            session.Tick(std::chrono::milliseconds(2000), std::chrono::milliseconds(60000));

            THEN("no loot is generated") {
                REQUIRE(session.GetLostObjects().empty());
            }
        }

        WHEN("multiple dogs join and a lot of time passes") {
            session.AddDog("Rex"s);
            session.AddDog("Bob"s);
            session.AddDog("Max"s);
            session.Tick(std::chrono::milliseconds(10000), std::chrono::milliseconds(60000));

            THEN("loot count never exceeds dog count") {
                REQUIRE(session.GetLostObjects().size() <= session.GetDogs().size());
            }
        }
    }

    GIVEN("a map with zero probability loot generator") {
        model::Map map(model::Map::Id{"map2"s}, "Map 2"s);
        map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 40));
        map.SetLootTypeCount(1);

        model::GameSession session(&map, false, std::chrono::milliseconds(1000), 0.0);

        WHEN("a dog joins and time passes") {
            session.AddDog("Rex"s);
            session.Tick(std::chrono::milliseconds(5000), std::chrono::milliseconds(60000));

            THEN("no loot is ever generated") {
                REQUIRE(session.GetLostObjects().empty());
            }
        }
    }
}
