#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

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

struct RoadBounds {
    double x_min, x_max, y_min, y_max;
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

    RoadBounds GetBounds() const noexcept {
        constexpr double half_width = 0.4;
        if (IsHorizontal()) {
            double x0 = start_.x, x1 = end_.x;
            return RoadBounds{
                std::min(x0, x1) - half_width, std::max(x0, x1) + half_width,
                static_cast<double>(start_.y) - half_width, static_cast<double>(start_.y) + half_width
            };
        } else {
            double y0 = start_.y, y1 = end_.y;
            return RoadBounds{
                static_cast<double>(start_.x) - half_width, static_cast<double>(start_.x) + half_width,
                std::min(y0, y1) - half_width, std::max(y0, y1) + half_width
            };
        }
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

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
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
    double dog_speed_ = 1.0;
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

    double GetDogSpeed() const noexcept {
        return map_->GetDogSpeed();
    }

    Dog& AddDog(const std::string& name) {
        Dog::Id id = next_dog_id_++;
        Position spawn_point;
        if (map_->GetRoads().empty()) {
            spawn_point = Position{0.0, 0.0};
        } else {
            const Road& first_road = map_->GetRoads().front();
            spawn_point = Position{static_cast<double>(first_road.GetStart().x),
                                    static_cast<double>(first_road.GetStart().y)};
        }
        dogs_.push_back(std::make_unique<Dog>(id, name, spawn_point));
        return *dogs_.back();
    }

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

    void Tick(double dt_seconds) {
        for (auto& dog_ptr : dogs_) {
            Speed speed = dog_ptr->GetSpeed();
            if (speed.vx == 0.0 && speed.vy == 0.0) {
                continue;
            }
            Position pos = dog_ptr->GetPosition();
            double new_x = pos.x + speed.vx * dt_seconds;
            double new_y = pos.y + speed.vy * dt_seconds;

            double min_x = pos.x, max_x = pos.x, min_y = pos.y, max_y = pos.y;
            bool on_road = false;
            constexpr double eps = 1e-9;

            for (const auto& road : map_->GetRoads()) {
                auto b = road.GetBounds();
                if (pos.x >= b.x_min - eps && pos.x <= b.x_max + eps &&
                    pos.y >= b.y_min - eps && pos.y <= b.y_max + eps) {
                    on_road = true;
                    min_x = std::min(min_x, b.x_min);
                    max_x = std::max(max_x, b.x_max);
                    min_y = std::min(min_y, b.y_min);
                    max_y = std::max(max_y, b.y_max);
                }
            }

            if (!on_road) {
                continue;
            }

            double clamped_x = std::clamp(new_x, min_x, max_x);
            double clamped_y = std::clamp(new_y, min_y, max_y);
            bool stopped = (clamped_x != new_x) || (clamped_y != new_y);

            dog_ptr->SetPosition(Position{clamped_x, clamped_y});
            if (stopped) {
                dog_ptr->SetSpeed(Speed{0.0, 0.0});
            }
        }
    }

private:
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

    void Tick(double dt_seconds) {
        for (auto& session : sessions_) {
            session->Tick(dt_seconds);
        }
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::vector<std::unique_ptr<GameSession>> sessions_;
};

}  // namespace model
