#pragma once
#include <memory>
#include <string>

#include "connection_pool.h"
#include "records_repository.h"

namespace postgres {

class Database {
public:
    explicit Database(const std::string& db_url, size_t pool_size);

    RecordsRepository& GetRecords() & {
        return records_;
    }

private:
    std::unique_ptr<ConnectionPool> pool_;
    RecordsRepository records_{*pool_};
};

}  // namespace postgres
