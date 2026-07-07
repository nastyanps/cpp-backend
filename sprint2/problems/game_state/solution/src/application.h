#pragma once
#include "model.h"
#include "players.h"
#include <optional>
#include <string>

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

private:
    model::Game& game_;
    Players players_;
    PlayerTokens tokens_;
};

}  // namespace app
