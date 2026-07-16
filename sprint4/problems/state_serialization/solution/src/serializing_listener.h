#pragma once
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "application.h"
#include "application_listener.h"
#include "model_serialization.h"

namespace infrastructure {

class SerializingListener : public app::ApplicationListener {
public:
    SerializingListener(app::Application& app, std::filesystem::path state_file,
                         std::chrono::milliseconds save_period)
        : app_{app}
        , state_file_{std::move(state_file)}
        , save_period_{save_period} {
    }

    void OnTick(std::chrono::milliseconds delta) override {
        if (state_file_.empty()) {
            return;
        }
        time_since_save_ += delta;
        if (save_period_.count() == 0 || time_since_save_ >= save_period_) {
            Save();
            time_since_save_ = std::chrono::milliseconds{0};
        }
    }

    void Save() const {
        if (state_file_.empty()) {
            return;
        }
        serialization::GameStateRepr repr;

        for (const auto& session_ptr : app_.GetGame().GetSessions()) {
            repr.AddSession(*session_ptr);
        }

        app_.GetTokens().ForEachToken([&repr](const app::Token& token, const app::Player& player) {
            serialization::PlayerRepr player_repr;
            player_repr.token = *token;
            player_repr.dog_id = player.GetDog()->GetId();
            player_repr.map_id = *player.GetSession()->GetMap()->GetId();
            repr.AddPlayer(std::move(player_repr));
        });

        auto tmp_path = state_file_;
        tmp_path += ".tmp";

        {
            std::ofstream out(tmp_path);
            if (!out) {
                throw std::runtime_error("Failed to open temp state file for writing");
            }
            boost::archive::text_oarchive archive{out};
            archive << repr;
        }

        std::filesystem::rename(tmp_path, state_file_);
    }

    void Load() {
        if (state_file_.empty() || !std::filesystem::exists(state_file_)) {
            return;
        }

        std::ifstream in(state_file_);
        if (!in) {
            throw std::runtime_error("Failed to open state file for reading");
        }
        boost::archive::text_iarchive archive{in};
        serialization::GameStateRepr repr;
        archive >> repr;

        for (const auto& session_repr : repr.GetSessions()) {
            model::Map::Id map_id{session_repr.GetMapId()};
            model::GameSession& session = app_.GetGame().GetOrCreateSessionByMapId(map_id);
            session_repr.Restore(session);
        }

        for (const auto& player_repr : repr.GetPlayers()) {
            app::Token token{player_repr.token};
            app_.RestorePlayer(token, player_repr.dog_id, player_repr.map_id);
        }
    }

private:
    app::Application& app_;
    std::filesystem::path state_file_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds time_since_save_{0};
};

}  // namespace infrastructure
