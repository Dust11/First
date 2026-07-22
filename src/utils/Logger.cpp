#include "utils/Logger.h"

#include <windows.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <sstream>
#include <thread>

namespace overlay::utils {

namespace {

std::string CurrentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) %
                    1000;
    std::tm local{};
    localtime_s(&local, &time);
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                       local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                       local.tm_hour, local.tm_min, local.tm_sec, ms.count());
}

std::string ThreadIdToString(const std::thread::id& id) {
    std::ostringstream oss;
    oss << id;
    return oss.str();
}

} // namespace

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::Init(const std::filesystem::path& log_path) {
    std::lock_guard lock(mutex_);
    if (initialized_) return;
    file_.open(log_path, std::ios::out | std::ios::trunc);
    initialized_ = file_.is_open();
}

void Logger::Log(std::string_view level, std::string_view message) {
    std::lock_guard lock(mutex_);
    if (!initialized_) return;
    const auto line = std::format("[{}] [{}] [tid:{}] {}", CurrentTimestamp(),
                                  level, ThreadIdToString(std::this_thread::get_id()),
                                  message);
    file_ << line << "\n";
    file_.flush();
    OutputDebugStringA(line.c_str());
    OutputDebugStringA("\n");
}

} // namespace overlay::utils
