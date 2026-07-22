#pragma once

namespace overlay::core {

class PlaybackEngine {
public:
    PlaybackEngine() = default;
    void Update(float) {}
    void PlayPause() {}
    void Reset() {}
    size_t CurrentStep() const { return 0; }
};

} // namespace overlay::core
