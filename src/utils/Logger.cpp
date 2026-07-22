#include "utils/Logger.h"

#include <windows.h>
#include <filesystem>

namespace overlay::utils {

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
    file_ << "[" << level << "] " << message << "\n";
    file_.flush();
    OutputDebugStringA(message.data());
    OutputDebugStringA("\n");
}

} // namespace overlay::utils
