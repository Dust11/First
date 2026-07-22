#pragma once

#include <string>
#include <format>
#include <fstream>
#include <filesystem>
#include <mutex>

namespace overlay::utils {

class Logger {
public:
    static Logger& Instance();

    void Init(const std::filesystem::path& log_path);
    void Log(std::string_view level, std::string_view message);

private:
    Logger() = default;
    std::mutex mutex_;
    std::ofstream file_;
    bool initialized_ = false;
};

} // namespace overlay::utils

#define LOG_INFO(msg)  overlay::utils::Logger::Instance().Log("INFO", (msg))
#define LOG_WARN(msg)  overlay::utils::Logger::Instance().Log("WARN", (msg))
#define LOG_ERROR(msg) overlay::utils::Logger::Instance().Log("ERROR", (msg))
