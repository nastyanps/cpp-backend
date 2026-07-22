#pragma once
#include <boost/json.hpp>
#include <string>
#include <unordered_map>

namespace extra_data {

class LootTypesData {
public:
    void AddLootTypes(const std::string& map_id, boost::json::array loot_types) {
        data_[map_id] = std::move(loot_types);
    }

    const boost::json::array* FindLootTypes(const std::string& map_id) const {
        auto it = data_.find(map_id);
        if (it == data_.end()) {
            return nullptr;
        }
        return &it->second;
    }

private:
    std::unordered_map<std::string, boost::json::array> data_;
};

}  // namespace extra_data
