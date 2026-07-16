#pragma once
#include <chrono>

namespace app {

class ApplicationListener {
public:
    virtual void OnTick(std::chrono::milliseconds delta) = 0;

protected:
    ~ApplicationListener() = default;
};

}  // namespace app
