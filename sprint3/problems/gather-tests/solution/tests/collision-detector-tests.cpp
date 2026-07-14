#define _USE_MATH_DEFINES

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <cmath>
#include <sstream>
#include <vector>

#include "../src/collision_detector.h"

using namespace std::literals;

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << ","
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace {

constexpr double EPSILON = 1e-10;

class TestProvider : public collision_detector::ItemGathererProvider {
public:
    TestProvider(std::vector<collision_detector::Item> items,
                 std::vector<collision_detector::Gatherer> gatherers)
        : items_{std::move(items)}
        , gatherers_{std::move(gatherers)} {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

class EventsCloseMatcher : public Catch::Matchers::MatcherGenericBase {
public:
    explicit EventsCloseMatcher(std::vector<collision_detector::GatheringEvent> expected)
        : expected_{std::move(expected)} {
    }

    bool match(const std::vector<collision_detector::GatheringEvent>& actual) const {
        if (actual.size() != expected_.size()) {
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            const auto& a = actual[i];
            const auto& e = expected_[i];
            if (a.gatherer_id != e.gatherer_id || a.item_id != e.item_id) {
                return false;
            }
            if (std::abs(a.sq_distance - e.sq_distance) > EPSILON) {
                return false;
            }
            if (std::abs(a.time - e.time) > EPSILON) {
                return false;
            }
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream out;
        out << "Events close to: [";
        for (const auto& e : expected_) {
            out << Catch::StringMaker<collision_detector::GatheringEvent>::convert(e) << " ";
        }
        out << "]";
        return out.str();
    }

private:
    std::vector<collision_detector::GatheringEvent> expected_;
};

EventsCloseMatcher EventsAreClose(std::vector<collision_detector::GatheringEvent> expected) {
    return EventsCloseMatcher{std::move(expected)};
}

bool IsSortedByTime(const std::vector<collision_detector::GatheringEvent>& events) {
    return std::is_sorted(events.begin(), events.end(),
                           [](const auto& lhs, const auto& rhs) {
                               return lhs.time < rhs.time;
                           });
}

}  // namespace

SCENARIO("Item collection detection", "[collision-detector]") {

    GIVEN("no items and no gatherers") {
        TestProvider provider({}, {});

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("no events are found") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("a single gatherer that stays in place") {
        // Собиратель не переместился - согласно условию, событий быть не должно.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{0.0, 0.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{0.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("no events are found") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("a gatherer moving directly through an item") {
        // Собиратель движется из (0,0) в (10,0), предмет лежит ровно посередине.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{5.0, 0.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("exactly one event is found") {
                REQUIRE(events.size() == 1);
            }

            THEN("the event has correct data") {
                std::vector<collision_detector::GatheringEvent> expected = {
                    {0, 0, 0.0, 0.5}
                };
                REQUIRE_THAT(events, EventsAreClose(expected));
            }
        }
    }

    GIVEN("an item that is out of range of the gatherer's path") {
        // Предмет находится далеко от прямой перемещения - события быть не должно.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{5.0, 100.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("no events are found") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("an item whose projection falls before the start of the path") {
        // Предмет находится "позади" точки старта - проекция < 0, столкновения не будет.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{-5.0, 0.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("no events are found") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("an item whose projection falls after the end of the path") {
        // Предмет находится "впереди" точки финиша - проекция > 1, столкновения не будет.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{15.0, 0.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("no events are found") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("multiple items along the gatherer's path") {
        // Три предмета на пути, должны обнаружиться все в хронологическом порядке.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{8.0, 0.0}, 0.1},  // дальше по пути
            {geom::Point2D{2.0, 0.0}, 0.1},  // ближе к старту
            {geom::Point2D{5.0, 0.0}, 0.1}   // посередине
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("all three events are found") {
                REQUIRE(events.size() == 3);
            }

            THEN("events are sorted by time") {
                REQUIRE(IsSortedByTime(events));
            }

            THEN("the first event corresponds to the closest item") {
                REQUIRE(events.front().item_id == 1);
            }

            THEN("the last event corresponds to the farthest item") {
                REQUIRE(events.back().item_id == 0);
            }
        }
    }

    GIVEN("multiple gatherers passing near the same item") {
        // Оба собирателя проходят точно через предмет (5,0) - должно быть два события.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{5.0, 0.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, 0.5},
            {geom::Point2D{5.0, -10.0}, geom::Point2D{5.0, 10.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("two events are found for the same item") {
                REQUIRE(events.size() == 2);
                for (const auto& event : events) {
                    REQUIRE(event.item_id == 0);
                }
            }

            THEN("events are sorted by time") {
                REQUIRE(IsSortedByTime(events));
            }
        }
    }

    GIVEN("an item exactly at the edge of the collection radius") {
        // Проверка граничного случая: сумма радиусов равна расстоянию.
        double gatherer_width = 1.0;
        double item_width = 1.0;
        double collect_radius = gatherer_width + item_width;

        std::vector<collision_detector::Item> items = {
            {geom::Point2D{5.0, collect_radius}, item_width}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 0.0}, gatherer_width}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("the event is detected (boundary inclusive)") {
                REQUIRE(events.size() == 1);
            }
        }
    }

    GIVEN("a gatherer moving diagonally") {
        // Проверка на непрямолинейное (диагональное) перемещение.
        std::vector<collision_detector::Item> items = {
            {geom::Point2D{5.0, 5.0}, 0.1}
        };
        std::vector<collision_detector::Gatherer> gatherers = {
            {geom::Point2D{0.0, 0.0}, geom::Point2D{10.0, 10.0}, 0.5}
        };
        TestProvider provider(items, gatherers);

        WHEN("we search for gathering events") {
            auto events = collision_detector::FindGatherEvents(provider);

            THEN("the event is found at the midpoint") {
                REQUIRE(events.size() == 1);
                std::vector<collision_detector::GatheringEvent> expected = {
                    {0, 0, 0.0, 0.5}
                };
                REQUIRE_THAT(events, EventsAreClose(expected));
            }
        }
    }
}
