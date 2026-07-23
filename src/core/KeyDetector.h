#pragma once

#include "core/TeamRotation.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace overlay::core {

// 将按键名转换为虚拟键码（如 "LButton" -> VK_LBUTTON，"Space" -> VK_SPACE）。
// 无法识别时返回 0。
std::uint32_t KeyNameToVk(const std::string& name);

// 拆分组合键字符串，如 "Shift+1" -> {"Shift", "1"}。
std::vector<std::string> ParseKeyCombination(const std::string& key);

// 判断是否为修饰键名
bool IsModifierName(const std::string& name);

class IKeyDetector {
public:
    virtual ~IKeyDetector() = default;

    // 更新内部状态（事件驱动版本为空操作，轮询版本在此采样）
    virtual void Update() = 0;

    // 消费一次按键按下事件；返回 true 表示期望的 key 被按下。
    virtual bool WasKeyPressed(const std::string& key) = 0;

    // 设置当前允许触发错误红闪的按键集合（主键）。
    virtual void SetValidKeys(const std::unordered_set<std::string>& keys) = 0;

    // 上一轮中是否按下了有效按键但不是期望按键。
    virtual bool IsWrongKeyPressed() const = 0;
};

// 方案 A：事件驱动，通过 WndProc 注入 WM_KEYDOWN/WM_KEYUP 消息。
class EventDrivenKeyDetector : public IKeyDetector {
public:
    EventDrivenKeyDetector();

    void OnKeyDown(std::uint32_t vk);
    void OnKeyUp(std::uint32_t vk);

    void Update() override {}
    bool WasKeyPressed(const std::string& key) override;
    void SetValidKeys(const std::unordered_set<std::string>& keys) override;
    bool IsWrongKeyPressed() const override { return wrong_key_pressed_; }

private:
    bool CheckModifiers(const std::vector<std::string>& modifiers) const;
    bool ConsumePendingKey(std::uint32_t vk);

    std::array<bool, 256> key_state_{};
    std::array<bool, 256> pending_press_{};
    std::unordered_set<std::string> valid_keys_;
    bool wrong_key_pressed_ = false;
};

// 方案 B：独立线程轮询（回退方案）。
class PollingKeyDetector : public IKeyDetector {
public:
    PollingKeyDetector();
    ~PollingKeyDetector() override;

    void Start(int poll_hz);
    void Stop();

    void Update() override;
    bool WasKeyPressed(const std::string& key) override;
    void SetValidKeys(const std::unordered_set<std::string>& keys) override;
    bool IsWrongKeyPressed() const override { return wrong_key_pressed_.load(std::memory_order_acquire); }

private:
    void Run(int poll_hz);
    void PollOnce();

    std::atomic<bool> running_{false};
    std::thread thread_;

    std::array<bool, 256> prev_state_{};
    std::array<bool, 256> current_press_{};
    std::unordered_set<std::string> valid_keys_;
    std::atomic<bool> wrong_key_pressed_{false};

    mutable std::mutex mutex_;
};

// 辅助函数：从队伍流程提取所有需要检测的主键集合。
std::unordered_set<std::string> ExtractValidKeys(const TeamRotation& rotation);

} // namespace overlay::core
