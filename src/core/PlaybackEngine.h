#pragma once

#include "core/TeamRotation.h"

namespace overlay::core {

enum class PlaybackState {
    Idle,
    Playing,
    Paused,
    Finished,
};

class PlaybackEngine {
public:
    PlaybackEngine();

    void SetRotation(const TeamRotation* rotation);

    void Play();
    void Pause();
    void PlayPause();
    void Reset();

    // 以毫秒为单位的帧时间推进播放进度（自动播放模式）
    void Update(float dt_ms);

    // 按键检测模式：手动前进一步
    void NextStep();

    PlaybackState GetState() const { return state_; }
    size_t CurrentStep() const { return current_step_; }

    // 当前步骤在 duration 中的进度（0..1），用于进度条
    float CurrentStepProgress() const;

    bool IsLoopEnabled() const { return loop_enabled_; }
    void SetLoopEnabled(bool enabled) { loop_enabled_ = enabled; }

private:
    const TeamRotation* rotation_ = nullptr;
    PlaybackState state_ = PlaybackState::Idle;
    size_t current_step_ = 0;
    float elapsed_ms_ = 0.0f;
    bool loop_enabled_ = true;
};

} // namespace overlay::core
