#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

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

}  // namespace model
