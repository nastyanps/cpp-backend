#pragma once
#include "model.h"
#include "players.h"
#include "application_listener.h"

#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <chrono>

namespace app {

struct JoinGameResult {
    Token token;
    model::Dog::Id player_id;
};

enum class JoinGameErrorReason {
    InvalidName,
    InvalidMap
};

class JoinGameError : public std::runtime_error {
public:
    explicit JoinGameError(JoinGameErrorReason reason)
        : std::runtime_error("Join game error")
        , reason_{reason} {
    }

    JoinGameErrorReason GetReason() const noexcept {
        return reason_;
    }

private:
    JoinGameErrorReason reason_;
};

class Application {
public:
    explicit Application(model::Game& game)
        : game_{game} {
    }

    const model::Game::Maps& ListMaps() const noexcept {
        return game_.GetMaps();
    }

    const model::Map* FindMap(const model::Map::Id& id) const noexcept {
        return game_.FindMap(id);
    }

    JoinGameResult JoinGame(const std::string& map_id_str, const std::string& user_name) {
        if (user_name.empty()) {
            throw JoinGameError(JoinGameErrorReason::InvalidName);
        }

        model::Map::Id map_id{map_id_str};
        const model::Map* map = game_.FindMap(map_id);
        if (!map) {
            throw JoinGameError(JoinGameErrorReason::InvalidMap);
        }

        model::GameSession& session = game_.FindOrCreateSession(map_id);
        model::Dog& dog = session.AddDog(user_name);
        Player& player = players_.Add(&dog, &session);
        Token token = tokens_.AddPlayer(player);

        return JoinGameResult{token, dog.GetId()};
    }

    Player* FindPlayerByToken(const Token& token) {
        return tokens_.FindPlayerByToken(token);
    }

    void SetPlayerDirection(app::Player& player, std::string_view move) {
        model::Dog* dog = player.GetDog();
        double speed = player.GetSession()->GetDogSpeed();

        if (move == "L") {
            dog->SetSpeed(model::Speed{-speed, 0.0});
            dog->SetDirection(model::Direction::WEST);
        } else if (move == "R") {
            dog->SetSpeed(model::Speed{speed, 0.0});
            dog->SetDirection(model::Direction::EAST);
        } else if (move == "U") {
            dog->SetSpeed(model::Speed{0.0, -speed});
            dog->SetDirection(model::Direction::NORTH);
        } else if (move == "D") {
            dog->SetSpeed(model::Speed{0.0, speed});
            dog->SetDirection(model::Direction::SOUTH);
        } else if (move.empty()) {
            dog->SetSpeed(model::Speed{0.0, 0.0});
        } else {
            throw std::invalid_argument("Invalid move value");
        }
    }

    void Tick(std::chrono::milliseconds time_delta) {
        auto retired = game_.Tick(time_delta);
        if (retired.empty()) {
            return;
        }

        std::vector<RetiredPlayerInfo> retired_infos;
        for (const auto& info : retired) {
            retired_infos.push_back(RetiredPlayerInfo{info.name, info.score, info.play_time_seconds});

            model::GameSession* session = game_.FindSessionByDogId(info.id);
            if (session) {
                std::string map_id = *session->GetMap()->GetId();
                if (Player* player = players_.FindByDogIdAndMapId(info.id, map_id)) {
                    tokens_.RemoveByPlayer(*player);
                    players_.Remove(info.id, map_id);
                }
                session->RemoveDog(info.id);
            }
        }

        if (listener_) {
            try {
                listener_->OnPlayersRetired(retired_infos);
            } catch (const std::exception&) {
                // Сбой сохранения в БД не должен прерывать игровой цикл.
            }
        }
    }

    void SetManualTickAllowed(bool allowed) noexcept {
        manual_tick_allowed_ = allowed;
    }

    bool IsManualTickAllowed() const noexcept {
        return manual_tick_allowed_;
    }

    void SetListener(ApplicationListener* listener) noexcept {
        listener_ = listener;
    }

private:
    model::Game& game_;
    Players players_;
    PlayerTokens tokens_;
    bool manual_tick_allowed_ = true;
    ApplicationListener* listener_ = nullptr;
};

}  // namespace app
