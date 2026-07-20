#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "connection_pool.h"

namespace postgres {

struct RecordEntry {
    std::string name;
    unsigned score;
    double play_time_seconds;
};

class RecordsRepository {
public:
    explicit RecordsRepository(ConnectionPool& pool)
        : pool_{pool} {
    }

    void Save(const std::string& name, unsigned score, double play_time_seconds);

    std::vector<RecordEntry> GetRecords(int start, int max_items);

private:
    ConnectionPool& pool_;
};

}  // namespace postgres
