#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <windows.h>

namespace overlay::utils {

// 单个热键条目
struct HotkeyEntry {
    std::string name;         // 动作名，如 "toggle_visibility"
    std::string combination;  // 原始组合字符串，如 "Ctrl+Shift+H"
    int id = 0;               // RegisterHotKey ID
    uint32_t modifiers = 0;   // MOD_CONTROL | MOD_SHIFT | MOD_ALT
    uint32_t vk = 0;          // 主键 VK 码
    bool registered = false;  // RegisterHotKey 是否成功
    std::chrono::steady_clock::time_point last_fallback_trigger;
};

class HotkeyManager {
public:
    using ActionCallback = std::function<void(const std::string& action)>;

    HotkeyManager();
    ~HotkeyManager();

    // 禁止拷贝
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    // 从配置 JSON 对象（hotkeys 节）解析并注册热键。
    // hwnd: 接收 WM_HOTKEY 的窗口句柄。
    // base_id: RegisterHotKey 起始 ID。
    bool RegisterFromConfig(const nlohmann::json& hotkeys, HWND hwnd, int base_id = 1000);

    // 注销所有已注册热键
    void UnregisterAll();

    // 窗口收到 WM_HOTKEY 时调用
    void OnHotkeyMessage(WPARAM wParam);

    // 兜底检测：低频率（≤10Hz）调用，防止游戏全屏吞没 WM_HOTKEY
    void PollFallback();

    // 设置动作回调
    void SetActionCallback(ActionCallback cb) { callback_ = std::move(cb); }

    const std::vector<HotkeyEntry>& GetEntries() const { return entries_; }

private:
    bool ParseCombination(const std::string& combination, uint32_t& modifiers, uint32_t& vk);
    void TriggerAction(HotkeyEntry& entry);

    HWND hwnd_ = nullptr;
    std::vector<HotkeyEntry> entries_;
    ActionCallback callback_;
};

} // namespace overlay::utils
