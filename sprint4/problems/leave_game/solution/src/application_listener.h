#pragma once
#include <string>
#include <vector>

namespace app {

struct RetiredPlayerInfo {
    std::string name;
    unsigned score;
    double play_time_seconds;
};

class ApplicationListener {
public:
    virtual void OnPlayersRetired(const std::vector<RetiredPlayerInfo>& retired) = 0;

protected:
    ~ApplicationListener() = default;
};

}  // namespace app
