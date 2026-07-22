#pragma once

#include <string>

namespace overlay::core {

class IKeyDetector {
public:
    virtual ~IKeyDetector() = default;
    virtual void Update() = 0;
    virtual bool WasKeyPressed(const std::string& key) const = 0;
};

} // namespace overlay::core
