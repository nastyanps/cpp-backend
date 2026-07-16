#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>

#include "model.h"
#include "players.h"
#include "application.h"

namespace model {

template <typename Archive>
void serialize(Archive& ar, Position& pos, [[maybe_unused]] const unsigned version) {
    ar & pos.x;
    ar & pos.y;
}

template <typename Archive>
void serialize(Archive& ar, Speed& speed, [[maybe_unused]] const unsigned version) {
    ar & speed.vx;
    ar & speed.vy;
}

template <typename Archive>
void serialize(Archive& ar, BagItem& item, [[maybe_unused]] const unsigned version) {
    ar & item.id;
    ar & item.type;
}

template <typename Archive>
void serialize(Archive& ar, LostObject& lo, [[maybe_unused]] const unsigned version) {
    ar & lo.id;
    ar & lo.type;
    ar & lo.pos;
}

}  // namespace model

namespace serialization {

// DogRepr - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_(dog.GetBag()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, pos_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_) {
            if (!dog.TryPutItem(item, static_cast<unsigned>(bag_.size()))) {
                throw std::runtime_error("Failed to restore bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & name_;
        ar & pos_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_;
    }

private:
    model::Dog::Id id_ = 0;
    std::string name_;
    model::Position pos_;
    model::Speed speed_;
    model::Direction direction_ = model::Direction::NORTH;
    unsigned score_ = 0;
    std::vector<model::BagItem> bag_;
};

// GameSessionRepr - сериализованное представление GameSession
class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(const model::GameSession& session)
        : map_id_(*session.GetMap()->GetId())
        , lost_objects_(session.GetLostObjects())
        , next_dog_id_(session.GetNextDogId())
        , next_lost_object_id_(session.GetNextLostObjectId()) {
        for (const auto& dog_ptr : session.GetDogs()) {
            dogs_.emplace_back(*dog_ptr);
        }
    }

    const std::string& GetMapId() const noexcept {
        return map_id_;
    }

    void Restore(model::GameSession& session) const {
        for (const auto& dog_repr : dogs_) {
            session.AddRestoredDog(dog_repr.Restore());
        }
        session.SetLostObjects(lost_objects_);
        session.SetNextDogId(next_dog_id_);
        session.SetNextLostObjectId(next_lost_object_id_);
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & map_id_;
        ar & dogs_;
        ar & lost_objects_;
        ar & next_dog_id_;
        ar & next_lost_object_id_;
    }

private:
    std::string map_id_;
    std::vector<DogRepr> dogs_;
    std::vector<model::LostObject> lost_objects_;
    model::Dog::Id next_dog_id_ = 0;
    model::LostObject::Id next_lost_object_id_ = 0;
};

// PlayerRepr - привязка токена к собаке+карте, чтобы восстановить app::Players/PlayerTokens
struct PlayerRepr {
    std::string token;
    model::Dog::Id dog_id;
    std::string map_id;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & token;
        ar & dog_id;
        ar & map_id;
    }
};

// GameStateRepr - полное состояние: все сессии + все токены игроков
class GameStateRepr {
public:
    GameStateRepr() = default;

    void AddSession(const model::GameSession& session) {
        sessions_.emplace_back(session);
    }

    void AddPlayer(PlayerRepr player) {
        players_.push_back(std::move(player));
    }

    const std::vector<GameSessionRepr>& GetSessions() const noexcept {
        return sessions_;
    }

    const std::vector<PlayerRepr>& GetPlayers() const noexcept {
        return players_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & sessions_;
        ar & players_;
    }

private:
    std::vector<GameSessionRepr> sessions_;
    std::vector<PlayerRepr> players_;
};

}  // namespace serialization
