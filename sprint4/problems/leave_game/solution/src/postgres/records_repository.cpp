#include "records_repository.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void RecordsRepository::Save(const std::string& name, unsigned score, double play_time_seconds) {
    auto conn = pool_.GetConnection();
    pqxx::work work{*conn};
    work.exec_params(
        "INSERT INTO retired_players (id, name, score, play_time_ms) "
        "VALUES (gen_random_uuid(), $1, $2, $3);"_zv,
        name, score, static_cast<int64_t>(play_time_seconds * 1000));
    work.commit();
}

std::vector<RecordEntry> RecordsRepository::GetRecords(int start, int max_items) {
    auto conn = pool_.GetConnection();
    pqxx::read_transaction r{*conn};

    std::vector<RecordEntry> records;
    auto result = r.exec_params(
        "SELECT name, score, play_time_ms FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "LIMIT $1 OFFSET $2;"_zv,
        max_items, start);

    for (const auto& row : result) {
        records.push_back(RecordEntry{
            row["name"].as<std::string>(),
            row["score"].as<unsigned>(),
            row["play_time_ms"].as<int64_t>() / 1000.0});
    }
    return records;
}

}  // namespace postgres
