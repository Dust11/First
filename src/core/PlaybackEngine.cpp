#include "core/PlaybackEngine.h"

#include "utils/Logger.h"

#include <format>

namespace overlay::core {

PlaybackEngine::PlaybackEngine() = default;

void PlaybackEngine::SetRotation(const TeamRotation* rotation) {
    rotation_ = rotation;
    Reset();
}

void PlaybackEngine::Play() {
    if (!rotation_ || rotation_->steps.empty()) {
        LOG_WARN("Cannot play: no rotation or empty steps.");
        return;
    }
    if (state_ == PlaybackState::Finished) {
        current_step_ = 0;
        elapsed_ms_ = 0.0f;
    }
    state_ = PlaybackState::Playing;
}

void PlaybackEngine::Pause() {
    if (state_ == PlaybackState::Playing) {
        state_ = PlaybackState::Paused;
    }
}

void PlaybackEngine::PlayPause() {
    if (state_ == PlaybackState::Playing) {
        Pause();
    } else {
        Play();
    }
}

void PlaybackEngine::Reset() {
    current_step_ = 0;
    elapsed_ms_ = 0.0f;
    state_ = rotation_ && !rotation_->steps.empty() ? PlaybackState::Idle : PlaybackState::Finished;
}

void PlaybackEngine::Update(float dt_ms) {
    if (state_ != PlaybackState::Playing || !rotation_) return;
    if (rotation_->steps.empty()) {
        state_ = PlaybackState::Finished;
        return;
    }
    if (dt_ms < 0.0f) dt_ms = 0.0f;

    elapsed_ms_ += dt_ms;

    const int duration_ms = rotation_->steps[current_step_].duration_ms;
    if (duration_ms <= 0) {
        LOG_WARN(std::format("Step {} has invalid duration {}; clamping to 100ms.",
                             current_step_, duration_ms));
    }
    const float step_duration = std::max(100.0f, static_cast<float>(duration_ms));

    while (elapsed_ms_ >= step_duration) {
        elapsed_ms_ -= step_duration;
        current_step_++;

        if (current_step_ >= rotation_->steps.size()) {
            if (loop_enabled_) {
                current_step_ = 0;
                LOG_INFO("Rotation looped.");
            } else {
                current_step_ = rotation_->steps.size() - 1;
                state_ = PlaybackState::Finished;
                elapsed_ms_ = 0.0f;
                LOG_INFO("Rotation finished.");
                break;
            }
        }
    }
}

void PlaybackEngine::NextStep() {
    if (!rotation_ || rotation_->steps.empty()) return;

    current_step_++;
    elapsed_ms_ = 0.0f;

    if (current_step_ >= rotation_->steps.size()) {
        if (loop_enabled_) {
            current_step_ = 0;
            LOG_INFO("Rotation looped (key advance).");
        } else {
            current_step_ = rotation_->steps.size() - 1;
            state_ = PlaybackState::Finished;
            LOG_INFO("Rotation finished (key advance).");
        }
    }
}

float PlaybackEngine::CurrentStepProgress() const {
    if (!rotation_ || rotation_->steps.empty()) return 0.0f;
    const int duration_ms = rotation_->steps[current_step_].duration_ms;
    const float step_duration = std::max(100.0f, static_cast<float>(duration_ms));
    return std::min(1.0f, elapsed_ms_ / step_duration);
}

} // namespace overlay::core
