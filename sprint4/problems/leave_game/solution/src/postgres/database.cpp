#include "database.h"

#include <pqxx/zview.hxx>

namespace postgres {

using pqxx::operator"" _zv;

Database::Database(const std::string& db_url, size_t pool_size) {
    pool_ = std::make_unique<ConnectionPool>(pool_size, [&db_url] {
        return std::make_shared<pqxx::connection>(db_url);
    });

    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    work.exec(R"(
CREATE EXTENSION IF NOT EXISTS "pgcrypto";
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID CONSTRAINT retired_players_id_constraint PRIMARY KEY,
    name varchar(100) NOT NULL,
    score integer NOT NULL,
    play_time_ms bigint NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS retired_players_score_idx
ON retired_players (score DESC, play_time_ms ASC, name ASC);
)"_zv);
    work.commit();
}

}  // namespace postgres
