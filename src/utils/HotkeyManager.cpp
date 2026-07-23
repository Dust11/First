#include "utils/HotkeyManager.h"

#include "core/KeyDetector.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <string>

namespace overlay::utils {

namespace {

std::string ToUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

HotkeyManager::HotkeyManager() = default;

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
}

bool HotkeyManager::ParseCombination(const std::string& combination,
                                     uint32_t& modifiers, uint32_t& vk) {
    modifiers = 0;
    vk = 0;

    auto parts = ::overlay::core::ParseKeyCombination(combination);
    if (parts.empty()) return false;

    // 最后一段是主键
    vk = ::overlay::core::KeyNameToVk(parts.back());
    if (vk == 0) {
        LOG_ERROR(std::format("Unknown hotkey main key: {}", parts.back()));
        return false;
    }

    // 前面是修饰键
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        std::string upper = ToUpper(parts[i]);
        if (upper == "CTRL" || upper == "CONTROL") {
            modifiers |= MOD_CONTROL;
        } else if (upper == "SHIFT") {
            modifiers |= MOD_SHIFT;
        } else if (upper == "ALT") {
            modifiers |= MOD_ALT;
        } else {
            LOG_ERROR(std::format("Unknown modifier in hotkey: {}", parts[i]));
            return false;
        }
    }

    return true;
}

bool HotkeyManager::RegisterFromConfig(const nlohmann::json& hotkeys,
                                       HWND hwnd, int base_id) {
    UnregisterAll();
    hwnd_ = hwnd;

    if (!hotkeys.is_object()) {
        LOG_ERROR("Hotkeys config is not an object.");
        return false;
    }

    int id = base_id;
    for (const auto& [name, value] : hotkeys.items()) {
        if (!value.is_string()) {
            LOG_ERROR(std::format("Hotkey value for '{}' is not a string.", name));
            continue;
        }

        HotkeyEntry entry;
        entry.name = name;
        entry.combination = value.get<std::string>();
        entry.id = id++;

        if (!ParseCombination(entry.combination, entry.modifiers, entry.vk)) {
            LOG_ERROR(std::format("Failed to parse hotkey '{}': {}", name, entry.combination));
            entries_.push_back(entry);
            continue;
        }

        if (hwnd_ != nullptr) {
            BOOL ok = RegisterHotKey(hwnd_, entry.id, entry.modifiers, entry.vk);
            if (!ok) {
                LOG_ERROR(std::format(
                    "RegisterHotKey failed for '{}': {} (error={})",
                    name, entry.combination, GetLastError()));
            } else {
                entry.registered = true;
                LOG_INFO(std::format("Registered hotkey '{}': {}", name, entry.combination));
            }
        }

        entries_.push_back(entry);
    }

    // 只要关键热键（quit）注册成功即认为基本可用
    bool has_quit = false;
    for (const auto& e : entries_) {
        if (e.name == "quit" && e.registered) has_quit = true;
    }
    return has_quit;
}

void HotkeyManager::UnregisterAll() {
    if (hwnd_ != nullptr) {
        for (const auto& entry : entries_) {
            if (entry.registered) {
                UnregisterHotKey(hwnd_, entry.id);
            }
        }
    }
    entries_.clear();
    hwnd_ = nullptr;
}

void HotkeyManager::OnHotkeyMessage(WPARAM wParam) {
    int id = static_cast<int>(wParam);
    for (auto& entry : entries_) {
        if (entry.id == id) {
            TriggerAction(entry);
            break;
        }
    }
}

void HotkeyManager::PollFallback() {
    // 兜底频率由调用方控制（建议 ≤10Hz）。
    // 检查每个已注册热键：修饰键 + 主键同时按下，并做 300ms 防抖。
    for (auto& entry : entries_) {
        if (!entry.registered) continue;

        bool ctrl_ok = (entry.modifiers & MOD_CONTROL) == 0 ||
                       (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift_ok = (entry.modifiers & MOD_SHIFT) == 0 ||
                        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt_ok = (entry.modifiers & MOD_ALT) == 0 ||
                      (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool main_ok = (GetAsyncKeyState(static_cast<int>(entry.vk)) & 0x8000) != 0;

        if (ctrl_ok && shift_ok && alt_ok && main_ok) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - entry.last_fallback_trigger);
            if (elapsed.count() >= 300) {
                entry.last_fallback_trigger = now;
                TriggerAction(entry);
            }
        }
    }
}

void HotkeyManager::TriggerAction(HotkeyEntry& entry) {
    LOG_INFO(std::format("Hotkey triggered: {}", entry.name));
    if (callback_) {
        callback_(entry.name);
    }
}

} // namespace overlay::utils
