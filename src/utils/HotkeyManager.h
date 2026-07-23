#pragma once

namespace overlay::utils {

class HotkeyManager {
public:
    HotkeyManager() = default;
    bool RegisterDefaults() { return true; }
    void UnregisterAll() {}
    void PollFallback() {}
};

} // namespace overlay::utils
