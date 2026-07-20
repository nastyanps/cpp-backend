#pragma once
#include <cctype>
#include <deque>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "model.h"
#include "tagged.h"

namespace app {

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class Player {
public:
    Player(model::GameSession* session, model::Dog* dog)
        : session_{session}
        , dog_{dog} {
    }

    model::GameSession* GetSession() const noexcept {
        return session_;
    }

    model::Dog* GetDog() const noexcept {
        return dog_;
    }

private:
    model::GameSession* session_;
    model::Dog* dog_;
};

class Players {
public:
    Player& Add(model::Dog* dog, model::GameSession* session) {
        players_.emplace_back(session, dog);
        Player& player = players_.back();
        DogMapKey key{dog->GetId(), *session->GetMap()->GetId()};
        dog_map_id_to_player_[key] = &player;
        return player;
    }

    Player* FindByDogIdAndMapId(model::Dog::Id dog_id, const std::string& map_id) {
        DogMapKey key{dog_id, map_id};
        if (auto it = dog_map_id_to_player_.find(key); it != dog_map_id_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    using DogMapKey = std::pair<model::Dog::Id, std::string>;
    struct DogMapKeyHasher {
        size_t operator()(const DogMapKey& k) const {
            return std::hash<model::Dog::Id>{}(k.first) ^ (std::hash<std::string>{}(k.second) << 1);
        }
    };

    std::deque<Player> players_;
    std::unordered_map<DogMapKey, Player*, DogMapKeyHasher> dog_map_id_to_player_;
};

class PlayerTokens {
public:
    Token AddPlayer(Player& player) {
        Token token{GenerateToken()};
        token_to_player_[token] = &player;
        return token;
    }

    Player* FindPlayerByToken(const Token& token) {
        if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void RemoveByDogName(const std::string& dog_name) {
        for (auto it = token_to_player_.begin(); it != token_to_player_.end(); ++it) {
            if (it->second->GetDog()->GetName() == dog_name) {
                token_to_player_.erase(it);
                return;
            }
        }
    }

private:
    static uint64_t GenRandom(std::mt19937_64& gen) {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(gen);
    }

    std::string GenerateToken() {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(16) << GenRandom(generator1_)
            << std::setw(16) << GenRandom(generator2_);
        return oss.str();
    }

    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
};

}  // namespace app
