#include "model.h"

#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

// Map

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }
    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

// Dog

void Dog::UpdateActivity(std::chrono::milliseconds time_delta, bool was_moving) {
    if (was_moving) {
        idle_time_ = std::chrono::milliseconds{0};
    } else {
        idle_time_ += time_delta;
    }
    total_time_ += time_delta;
}

// GameSession::CollisionProvider

size_t GameSession::CollisionProvider::ItemsCount() const {
    return lost_objects_.size() + offices_.size();
}

collision_detector::Item GameSession::CollisionProvider::GetItem(size_t idx) const {
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

size_t GameSession::CollisionProvider::GatherersCount() const {
    return moves_.size();
}

collision_detector::Gatherer GameSession::CollisionProvider::GetGatherer(size_t idx) const {
    const auto& m = moves_[idx];
    return collision_detector::Gatherer{
        geom::Point2D{m.old_pos.x, m.old_pos.y},
        geom::Point2D{m.new_pos.x, m.new_pos.y},
        0.3};
}

bool GameSession::CollisionProvider::IsOffice(size_t idx) const {
    return idx >= lost_objects_.size();
}

// GameSession

Dog& GameSession::AddDog(const std::string& name) {
    Dog::Id id = next_dog_id_++;
    Position spawn_point = randomize_spawn_points_
        ? GenerateRandomPointOnRoad()
        : GenerateDefaultSpawnPoint();
    dogs_.push_back(std::make_unique<Dog>(id, name, spawn_point));
    return *dogs_.back();
}

std::vector<GameSession::RetiredDogInfo> GameSession::Tick(
    std::chrono::milliseconds time_delta, std::chrono::milliseconds retirement_time) {
    
    std::vector<bool> was_moving;
    was_moving.reserve(dogs_.size());
    for (const auto& dog_ptr : dogs_) {
        Speed s = dog_ptr->GetSpeed();
        was_moving.push_back(s.vx != 0.0 || s.vy != 0.0);
    }

    auto moves = ComputeMoves(time_delta);
    GenerateLoot(time_delta);
    ProcessCollisions(moves);

    for (size_t i = 0; i < dogs_.size(); ++i) {
        dogs_[i]->UpdateActivity(time_delta, was_moving[i]);
    }

    std::vector<RetiredDogInfo> retired;
    for (const auto& dog_ptr : dogs_) {
        if (dog_ptr->GetIdleTime() >= retirement_time) {
            double play_time_seconds =
                std::chrono::duration<double>(dog_ptr->GetTotalTime()).count();
            retired.push_back(RetiredDogInfo{
                dog_ptr->GetId(), *map_->GetId(), dog_ptr->GetName(), dog_ptr->GetScore(),
                play_time_seconds});
        }
    }
    return retired;
}

std::vector<GameSession::DogMove> GameSession::ComputeMoves(std::chrono::milliseconds time_delta) {
    std::vector<DogMove> moves;
    const double dt_seconds = std::chrono::duration<double>(time_delta).count();

    for (auto& dog_ptr : dogs_) {
        Position old_pos = dog_ptr->GetPosition();
        Speed speed = dog_ptr->GetSpeed();

        if (speed.vx == 0.0 && speed.vy == 0.0) {
            moves.push_back(DogMove{dog_ptr.get(), old_pos, old_pos});
            continue;
        }

        double new_x = old_pos.x + speed.vx * dt_seconds;
        double new_y = old_pos.y + speed.vy * dt_seconds;

        double min_x = old_pos.x, max_x = old_pos.x, min_y = old_pos.y, max_y = old_pos.y;
        bool on_road = false;
        constexpr double eps = 1e-9;

        for (const auto& road : map_->GetRoads()) {
            auto b = road.GetBounds();
            if (old_pos.x >= b.x_min - eps && old_pos.x <= b.x_max + eps &&
                old_pos.y >= b.y_min - eps && old_pos.y <= b.y_max + eps) {
                on_road = true;
                min_x = std::min(min_x, b.x_min);
                max_x = std::max(max_x, b.x_max);
                min_y = std::min(min_y, b.y_min);
                max_y = std::max(max_y, b.y_max);
            }
        }

        Position new_pos = old_pos;
        if (on_road) {
            double clamped_x = std::clamp(new_x, min_x, max_x);
            double clamped_y = std::clamp(new_y, min_y, max_y);
            bool stopped = (clamped_x != new_x) || (clamped_y != new_y);

            new_pos = Position{clamped_x, clamped_y};
            dog_ptr->SetPosition(new_pos);
            if (stopped) {
                dog_ptr->SetSpeed(Speed{0.0, 0.0});
            }
        }

        moves.push_back(DogMove{dog_ptr.get(), old_pos, new_pos});
    }
    return moves;
}

void GameSession::ProcessCollisions(const std::vector<DogMove>& moves) {
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

void GameSession::GenerateLoot(std::chrono::milliseconds time_delta) {
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

unsigned GameSession::GenerateRandomLootType() const {
    static thread_local std::mt19937 gen{std::random_device{}()};
    unsigned max_type = map_->GetLootTypeCount() > 0 ? map_->GetLootTypeCount() - 1 : 0;
    std::uniform_int_distribution<unsigned> dist(0, max_type);
    return dist(gen);
}

Position GameSession::GenerateDefaultSpawnPoint() const {
    if (map_->GetRoads().empty()) {
        return Position{0.0, 0.0};
    }
    const Road& first_road = map_->GetRoads().front();
    return Position{static_cast<double>(first_road.GetStart().x),
                     static_cast<double>(first_road.GetStart().y)};
}

Position GameSession::GenerateRandomPointOnRoad() const {
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

// Game

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

GameSession& Game::FindOrCreateSession(const Map::Id& map_id) {
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

std::vector<GameSession::RetiredDogInfo> Game::Tick(std::chrono::milliseconds time_delta) {
    std::vector<GameSession::RetiredDogInfo> all_retired;
    for (auto& session : sessions_) {
        auto retired = session->Tick(time_delta, retirement_time_);
        all_retired.insert(all_retired.end(), retired.begin(), retired.end());
    }
    return all_retired;
}

}  // namespace model
