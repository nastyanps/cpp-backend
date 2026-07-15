#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <iterator>

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

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_;
    Direction direction_ = Direction::NORTH;
    std::vector<BagItem> bag_;
    unsigned score_ = 0;
};

struct LostObject {
    using Id = std::uint32_t;
    Id id;
    unsigned type;
    Position pos;
};

class GameSession {
public:
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

    Dog& AddDog(const std::string& name) {
        Dog::Id id = next_dog_id_++;
        Position spawn_point = randomize_spawn_points_
            ? GenerateRandomPointOnRoad()
            : GenerateDefaultSpawnPoint();
        dogs_.push_back(std::make_unique<Dog>(id, name, spawn_point));
        return *dogs_.back();
    }

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

    const std::vector<LostObject>& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    void Tick(std::chrono::milliseconds time_delta) {
        auto moves = ComputeMoves(time_delta);
        GenerateLoot(time_delta);
        ProcessCollisions(moves);
    }

private:
    struct DogMove {
        Dog* dog;
        Position old_pos;
        Position new_pos;
    };

    std::vector<DogMove> ComputeMoves(std::chrono::milliseconds time_delta);

    class CollisionProvider : public collision_detector::ItemGathererProvider {
    public:
        CollisionProvider(const std::vector<LostObject>& lost_objects,
                           const Map::Offices& offices,
                           const std::vector<DogMove>& moves)
            : lost_objects_{lost_objects}
            , offices_{offices}
            , moves_{moves} {
        }

        size_t ItemsCount() const override {
            return lost_objects_.size() + offices_.size();
        }

        collision_detector::Item GetItem(size_t idx) const override {
            if (idx < lost_objects_.size()) {
                const auto& lo = lost_objects_[idx];
                return collision_detector::Item{geom::Point2D{lo.pos.x, lo.pos.y}, 0.0};
            }
            const auto& office = offices_[idx - lost_objects_.size()];
            return collision_detector::Item{
                geom::Point2D{static_cast<double>(office.GetPosition().x),
                              static_cast<double>(office.GetPosition().y)},
                0.25};
        }

        size_t GatherersCount() const override {
            return moves_.size();
        }

        collision_detector::Gatherer GetGatherer(size_t idx) const override {
            const auto& m = moves_[idx];
            return collision_detector::Gatherer{
                geom::Point2D{m.old_pos.x, m.old_pos.y},
                geom::Point2D{m.new_pos.x, m.new_pos.y},
                0.3};
        }

        bool IsOffice(size_t idx) const {
            return idx >= lost_objects_.size();
        }

    private:
        const std::vector<LostObject>& lost_objects_;
        const Map::Offices& offices_;
        const std::vector<DogMove>& moves_;
    };

    void ProcessCollisions(const std::vector<DogMove>& moves) {
        // Собиратель без перемещения не участвует в столкновениях.
        std::vector<DogMove> active_moves;
        std::copy_if(moves.begin(), moves.end(), std::back_inserter(active_moves),
                      [](const DogMove& m) {
                          return m.old_pos.x != m.new_pos.x || m.old_pos.y != m.new_pos.y;
                      });
        if (active_moves.empty()) {
            return;
        }

        CollisionProvider provider(lost_objects_, map_->GetOffices(), active_moves);
        auto events = collision_detector::FindGatherEvents(provider);

        std::vector<bool> item_collected(lost_objects_.size(), false);
        unsigned bag_capacity = map_->GetBagCapacity();

        for (const auto& event : events) {
            Dog* dog = active_moves[event.gatherer_id].dog;

            if (provider.IsOffice(event.item_id)) {
                unsigned total_points = 0;
                for (const auto& bag_item : dog->GetBag()) {
                    total_points += map_->GetLootValue(bag_item.type);
                }
                dog->AddScore(total_points);
                dog->ClearBag();
            } else {
                if (item_collected[event.item_id]) {
                    continue;
                }
                const auto& lo = lost_objects_[event.item_id];
                if (dog->TryPutItem(BagItem{lo.id, lo.type}, bag_capacity)) {
                    item_collected[event.item_id] = true;
                }
            }
        }

        if (std::find(item_collected.begin(), item_collected.end(), true) != item_collected.end()) {
            std::vector<LostObject> remaining;
            size_t idx = 0;
            std::copy_if(lost_objects_.begin(), lost_objects_.end(), std::back_inserter(remaining),
                          [&item_collected, &idx](const LostObject&) {
                              return !item_collected[idx++];
                          });
            lost_objects_ = std::move(remaining);
        }
    }

    void GenerateLoot(std::chrono::milliseconds time_delta) {
        unsigned looter_count = static_cast<unsigned>(dogs_.size());
        unsigned loot_count = static_cast<unsigned>(lost_objects_.size());
        unsigned new_loot = loot_generator_.Generate(time_delta, loot_count, looter_count);

        for (unsigned i = 0; i < new_loot; ++i) {
            LostObject::Id id = next_lost_object_id_++;
            unsigned type = GenerateRandomLootType();
            Position pos = GenerateRandomPointOnRoad();
            lost_objects_.push_back(LostObject{id, type, pos});
        }
    }

    unsigned GenerateRandomLootType() const {
        static thread_local std::mt19937 gen{std::random_device{}()};
        unsigned max_type = map_->GetLootTypeCount() > 0 ? map_->GetLootTypeCount() - 1 : 0;
        std::uniform_int_distribution<unsigned> dist(0, max_type);
        return dist(gen);
    }

    Position GenerateDefaultSpawnPoint() const {
        if (map_->GetRoads().empty()) {
            return Position{0.0, 0.0};
        }
        const Road& first_road = map_->GetRoads().front();
        return Position{static_cast<double>(first_road.GetStart().x),
                         static_cast<double>(first_road.GetStart().y)};
    }

    Position GenerateRandomPointOnRoad() const {
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

    GameSession& FindOrCreateSession(const Map::Id& map_id) {
        for (auto& session : sessions_) {
            if (session->GetMap()->GetId() == map_id) {
                return *session;
            }
        }
        const Map* map = FindMap(map_id);
        sessions_.push_back(std::make_unique<GameSession>(map, randomize_spawn_points_,
                                                            loot_period_, loot_probability_));
        return *sessions_.back();
    }

    void Tick(std::chrono::milliseconds time_delta) {
        for (auto& session : sessions_) {
            session->Tick(time_delta);
        }
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
};

}  // namespace model
