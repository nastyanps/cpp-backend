#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
#include "tagged.h"
#include "collision_detector.h"

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

struct BagItem {
    unsigned id;
    unsigned type;
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

    unsigned GetLootTypeCount() const noexcept {
        return loot_type_count_;
    }

    void SetLootTypeCount(unsigned count) noexcept {
        loot_type_count_ = count;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    unsigned GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(unsigned capacity) noexcept {
        bag_capacity_ = capacity;
    }

    void AddOffice(Office office);

    void SetLootValues(std::vector<unsigned> values) {
        loot_values_ = std::move(values);
    }

    unsigned GetLootValue(unsigned type) const noexcept {
        if (type < loot_values_.size()) {
            return loot_values_[type];
        }
        return 0;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    double dog_speed_ = 1.0;
    unsigned loot_type_count_ = 0;
    unsigned bag_capacity_ = 3;
    std::vector<unsigned> loot_values_;
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

    const std::vector<BagItem>& GetBag() const noexcept {
        return bag_;
    }

    bool TryPutItem(BagItem item, unsigned capacity) {
        if (bag_.size() >= capacity) {
            return false;
        }
        bag_.push_back(item);
        return true;
    }

    void ClearBag() {
        bag_.clear();
    }

    unsigned GetScore() const noexcept {
        return score_;
    }

    void AddScore(unsigned points) {
        score_ += points;
    }

    void UpdateActivity(std::chrono::milliseconds time_delta);

    std::chrono::milliseconds GetIdleTime() const noexcept {
        return idle_time_;
    }

    std::chrono::milliseconds GetTotalTime() const noexcept {
        return total_time_;
    }

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_;
    Direction direction_ = Direction::NORTH;
    std::vector<BagItem> bag_;
    unsigned score_ = 0;
    std::chrono::milliseconds idle_time_{0};
    std::chrono::milliseconds total_time_{0};
};

struct LostObject {
    using Id = std::uint32_t;
    Id id;
    unsigned type;
    Position pos;
};

class GameSession {
public:
    struct RetiredDogInfo {
        std::string name;
        unsigned score;
        double play_time_seconds;
    };

    GameSession(const Map* map, bool randomize_spawn_points,
                loot_gen::LootGenerator::TimeInterval loot_period, double loot_probability) noexcept
        : map_{map}
        , randomize_spawn_points_{randomize_spawn_points}
        , loot_generator_{loot_period, loot_probability} {
    }

    const Map* GetMap() const noexcept {
        return map_;
    }

    double GetDogSpeed() const noexcept {
        return map_->GetDogSpeed();
    }

    Dog& AddDog(const std::string& name);

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

    const std::vector<LostObject>& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    std::vector<RetiredDogInfo> Tick(std::chrono::milliseconds time_delta,
                                      std::chrono::milliseconds retirement_time);

private:
    struct DogMove {
        Dog* dog;
        Position old_pos;
        Position new_pos;
    };

    class CollisionProvider : public collision_detector::ItemGathererProvider {
    public:
        CollisionProvider(const std::vector<LostObject>& lost_objects,
                           const Map::Offices& offices,
                           const std::vector<DogMove>& moves)
            : lost_objects_{lost_objects}
            , offices_{offices}
            , moves_{moves} {
        }

        size_t ItemsCount() const override;
        collision_detector::Item GetItem(size_t idx) const override;
        size_t GatherersCount() const override;
        collision_detector::Gatherer GetGatherer(size_t idx) const override;
        bool IsOffice(size_t idx) const;

    private:
        const std::vector<LostObject>& lost_objects_;
        const Map::Offices& offices_;
        const std::vector<DogMove>& moves_;
    };

    std::vector<DogMove> ComputeMoves(std::chrono::milliseconds time_delta);
    void ProcessCollisions(const std::vector<DogMove>& moves);
    void GenerateLoot(std::chrono::milliseconds time_delta);
    unsigned GenerateRandomLootType() const;
    Position GenerateDefaultSpawnPoint() const;
    Position GenerateRandomPointOnRoad() const;

    const Map* map_;
    bool randomize_spawn_points_;
    std::vector<std::unique_ptr<Dog>> dogs_;
    Dog::Id next_dog_id_ = 0;
    std::vector<LostObject> lost_objects_;
    LostObject::Id next_lost_object_id_ = 0;
    loot_gen::LootGenerator loot_generator_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    Game(bool randomize_spawn_points, loot_gen::LootGenerator::TimeInterval loot_period,
         double loot_probability) noexcept
        : randomize_spawn_points_{randomize_spawn_points}
        , loot_period_{loot_period}
        , loot_probability_{loot_probability} {
    }

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

    GameSession& FindOrCreateSession(const Map::Id& map_id);

    std::vector<GameSession::RetiredDogInfo> Tick(std::chrono::milliseconds time_delta);

    void SetRetirementTime(std::chrono::milliseconds retirement_time) noexcept {
        retirement_time_ = retirement_time;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    bool randomize_spawn_points_;
    loot_gen::LootGenerator::TimeInterval loot_period_;
    double loot_probability_;
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::vector<std::unique_ptr<GameSession>> sessions_;
    std::chrono::milliseconds retirement_time_{60000};  // 1 минута по умолчанию
};

}  // namespace model
