#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

enum class Direction {
    NORTH, SOUTH, WEST, EAST
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    using Id = std::uint32_t;

    Dog(Id id, std::string name, Position position) noexcept
        : id_{id}
        , name_{std::move(name)}
        , position_{position} {
    }

    Id GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    Position GetPosition() const noexcept {
        return position_;
    }

    Speed GetSpeed() const noexcept {
        return speed_;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetPosition(Position position) noexcept {
        position_ = position;
    }

    void SetSpeed(Speed speed) noexcept {
        speed_ = speed;
    }

    void SetDirection(Direction direction) noexcept {
        direction_ = direction;
    }

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_;
    Direction direction_ = Direction::NORTH;
};

class GameSession {
public:
    explicit GameSession(const Map* map) noexcept
        : map_{map} {
    }

    const Map* GetMap() const noexcept {
        return map_;
    }

    Dog& AddDog(const std::string& name) {
        Dog::Id id = next_dog_id_++;
        Position spawn_point = GenerateRandomSpawnPoint();
        dogs_.push_back(std::make_unique<Dog>(id, name, spawn_point));
        return *dogs_.back();
    }

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

private:

    Position GenerateRandomSpawnPoint() const {
        const auto& roads = map_->GetRoads();
        if (roads.empty()) {
            return Position{0.0, 0.0};
        }
        static thread_local std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
        const Road& road = roads[road_dist(gen)];

        Point start = road.GetStart();
        Point end = road.GetEnd();

        if (road.IsHorizontal()) {
            int min_x = std::min(start.x, end.x);
            int max_x = std::max(start.x, end.x);
            std::uniform_int_distribution<int> x_dist(min_x, max_x);
            return Position{static_cast<double>(x_dist(gen)), static_cast<double>(start.y)};
        } else {
            int min_y = std::min(start.y, end.y);
            int max_y = std::max(start.y, end.y);
            std::uniform_int_distribution<int> y_dist(min_y, max_y);
            return Position{static_cast<double>(start.x), static_cast<double>(y_dist(gen))};
        }
    }

    const Map* map_;
    std::vector<std::unique_ptr<Dog>> dogs_;
    Dog::Id next_dog_id_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession& FindOrCreateSession(const Map::Id& map_id) {
        for (auto& session : sessions_) {
            if (session->GetMap()->GetId() == map_id) {
                return *session;
            }
        }
        const Map* map = FindMap(map_id);
        sessions_.push_back(std::make_unique<GameSession>(map));
        return *sessions_.back();
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::vector<std::unique_ptr<GameSession>> sessions_;
};

}  // namespace model
