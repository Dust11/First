#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <windows.h>

namespace overlay::utils {

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    // Watches the directory containing file_path and posts msg_id to notify_hwnd
    // when that specific filename is modified.
    bool Start(const std::filesystem::path& file_path, HWND notify_hwnd, UINT msg_id);
    void Stop();

    void Pause() { paused_.store(true); }
    void Resume() { paused_.store(false); }
    bool IsPaused() const { return paused_.load(); }

    // Poll-based check: returns true if a change was detected since last poll.
    // Includes built-in debounce (300 ms).
    bool Poll();

private:
    void WorkerThread();

    std::filesystem::path watch_dir_;
    std::wstring filter_name_;
    HWND notify_hwnd_ = nullptr;
    UINT msg_id_ = 0;

    HANDLE hDir_ = INVALID_HANDLE_VALUE;
    HANDLE hThread_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> changed_{false};

    std::chrono::steady_clock::time_point last_change_time_;
};

} // namespace overlay::utils
