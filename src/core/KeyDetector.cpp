#include "core/KeyDetector.h"

#include "utils/Logger.h"

#include <cctype>
#include <cmath>
#include <format>
#include <map>
#include <sstream>

namespace overlay::core {

namespace {

// 按键名 -> VK 映射表（大小写不敏感）
const std::map<std::string, std::uint32_t, std::less<>> kKeyNameMap = {
    // 鼠标键
    {"LBUTTON", VK_LBUTTON},
    {"RBUTTON", VK_RBUTTON},
    {"MBUTTON", VK_MBUTTON},
    {"XBUTTON1", VK_XBUTTON1},
    {"XBUTTON2", VK_XBUTTON2},
    // 常用键盘键
    {"BACK", VK_BACK},
    {"TAB", VK_TAB},
    {"RETURN", VK_RETURN},
    {"ENTER", VK_RETURN},
    {"SHIFT", VK_SHIFT},
    {"CTRL", VK_CONTROL},
    {"CONTROL", VK_CONTROL},
    {"ALT", VK_MENU},
    {"MENU", VK_MENU},
    {"PAUSE", VK_PAUSE},
    {"CAPITAL", VK_CAPITAL},
    {"CAPSLOCK", VK_CAPITAL},
    {"ESC", VK_ESCAPE},
    {"ESCAPE", VK_ESCAPE},
    {"SPACE", VK_SPACE},
    {"PRIOR", VK_PRIOR},
    {"PAGEUP", VK_PRIOR},
    {"NEXT", VK_NEXT},
    {"PAGEDOWN", VK_NEXT},
    {"END", VK_END},
    {"HOME", VK_HOME},
    {"LEFT", VK_LEFT},
    {"UP", VK_UP},
    {"RIGHT", VK_RIGHT},
    {"DOWN", VK_DOWN},
    {"PRINT", VK_PRINT},
    {"SNAPSHOT", VK_SNAPSHOT},
    {"PRINTSCREEN", VK_SNAPSHOT},
    {"INSERT", VK_INSERT},
    {"DELETE", VK_DELETE},
    {"DEL", VK_DELETE},
    {"HELP", VK_HELP},
    {"LWIN", VK_LWIN},
    {"RWIN", VK_RWIN},
    {"APPS", VK_APPS},
    {"NUMPAD0", VK_NUMPAD0},
    {"NUMPAD1", VK_NUMPAD1},
    {"NUMPAD2", VK_NUMPAD2},
    {"NUMPAD3", VK_NUMPAD3},
    {"NUMPAD4", VK_NUMPAD4},
    {"NUMPAD5", VK_NUMPAD5},
    {"NUMPAD6", VK_NUMPAD6},
    {"NUMPAD7", VK_NUMPAD7},
    {"NUMPAD8", VK_NUMPAD8},
    {"NUMPAD9", VK_NUMPAD9},
    {"MULTIPLY", VK_MULTIPLY},
    {"ADD", VK_ADD},
    {"SEPARATOR", VK_SEPARATOR},
    {"SUBTRACT", VK_SUBTRACT},
    {"DECIMAL", VK_DECIMAL},
    {"DIVIDE", VK_DIVIDE},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},
    {"NUMLOCK", VK_NUMLOCK},
    {"SCROLL", VK_SCROLL},
    {"LSHIFT", VK_LSHIFT},
    {"RSHIFT", VK_RSHIFT},
    {"LCONTROL", VK_LCONTROL},
    {"RCONTROL", VK_RCONTROL},
    {"LMENU", VK_LMENU},
    {"RMENU", VK_RMENU},
};

std::string ToUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

std::uint32_t KeyNameToVk(const std::string& name) {
    std::string upper = ToUpper(name);

    // 单字符 A-Z, 0-9
    if (upper.size() == 1) {
        char c = upper[0];
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }

    auto it = kKeyNameMap.find(upper);
    if (it != kKeyNameMap.end()) return it->second;

    // 特殊处理 LButton / RButton / MButton（混合大小写已在上一步统一为大写）
    return 0;
}

std::vector<std::string> ParseKeyCombination(const std::string& key) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : key) {
        if (c == '+') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

bool IsModifierName(const std::string& name) {
    std::string upper = ToUpper(name);
    return upper == "SHIFT" || upper == "CTRL" || upper == "CONTROL" || upper == "ALT";
}

std::unordered_set<std::string> ExtractValidKeys(const TeamRotation& rotation) {
    std::unordered_set<std::string> keys;
    for (const auto& step : rotation.steps) {
        auto parts = ParseKeyCombination(step.key);
        if (!parts.empty()) {
            // 组合键的主键是最后一段；单键就是本身
            keys.insert(parts.back());
        }
    }
    return keys;
}

// ============================================================================
// EventDrivenKeyDetector
// ============================================================================

EventDrivenKeyDetector::EventDrivenKeyDetector() {
    key_state_.fill(false);
    pending_press_.fill(false);
}

void EventDrivenKeyDetector::OnKeyDown(std::uint32_t vk) {
    if (vk >= key_state_.size()) return;
    if (!key_state_[vk]) {
        pending_press_[vk] = true;
    }
    key_state_[vk] = true;
}

void EventDrivenKeyDetector::OnKeyUp(std::uint32_t vk) {
    if (vk >= key_state_.size()) return;
    key_state_[vk] = false;
}

bool EventDrivenKeyDetector::CheckModifiers(const std::vector<std::string>& modifiers) const {
    for (const auto& mod : modifiers) {
        std::uint32_t vk = KeyNameToVk(mod);
        if (vk == VK_SHIFT && !(GetKeyState(VK_SHIFT) & 0x8000)) return false;
        if (vk == VK_CONTROL && !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
        if (vk == VK_MENU && !(GetKeyState(VK_MENU) & 0x8000)) return false;
    }
    return true;
}

bool EventDrivenKeyDetector::ConsumePendingKey(std::uint32_t vk) {
    if (vk >= pending_press_.size()) return false;
    if (pending_press_[vk]) {
        pending_press_[vk] = false;
        return true;
    }
    return false;
}

bool EventDrivenKeyDetector::WasKeyPressed(const std::string& key) {
    wrong_key_pressed_ = false;

    auto parts = ParseKeyCombination(key);
    if (parts.empty()) return false;

    std::string main_key = parts.back();
    std::vector<std::string> modifiers(parts.begin(), parts.end() - 1);

    std::uint32_t main_vk = KeyNameToVk(main_key);
    if (main_vk == 0) return false;

    bool consumed = ConsumePendingKey(main_vk);
    if (!consumed) return false;

    if (!CheckModifiers(modifiers)) {
        // 修饰键不满足：若主键属于有效集合则记为错键
        if (valid_keys_.find(main_key) != valid_keys_.end()) {
            wrong_key_pressed_ = true;
        }
        return false;
    }

    return true;
}

void EventDrivenKeyDetector::SetValidKeys(const std::unordered_set<std::string>& keys) {
    valid_keys_ = keys;
}

// ============================================================================
// PollingKeyDetector
// ============================================================================

PollingKeyDetector::PollingKeyDetector() = default;

PollingKeyDetector::~PollingKeyDetector() {
    Stop();
}

void PollingKeyDetector::Start(int poll_hz) {
    Stop();
    running_.store(true, std::memory_order_release);
    prev_state_.fill(false);
    current_press_.fill(false);
    thread_ = std::thread(&PollingKeyDetector::Run, this, std::max(1, poll_hz));
}

void PollingKeyDetector::Stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void PollingKeyDetector::Run(int poll_hz) {
    using clock = std::chrono::steady_clock;
    const float interval_ms = 1000.0f / std::max(1.0f, static_cast<float>(poll_hz));
    auto next_time = clock::now();

    while (running_.load(std::memory_order_acquire)) {
        PollOnce();

        next_time += std::chrono::duration<std::uint64_t, std::micro>(
            static_cast<std::uint64_t>(interval_ms * 1000.0f));
        std::this_thread::sleep_until(next_time);
    }
}

void PollingKeyDetector::PollOnce() {
    // 只跟踪 valid_keys_ 中涉及的主键和常用修饰键，避免全量 256 键轮询。
    std::vector<std::uint32_t> vks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& key : valid_keys_) {
            std::uint32_t vk = KeyNameToVk(key);
            if (vk != 0) vks.push_back(vk);
        }
    }
    // 始终跟踪 Shift/Ctrl/Alt 以便组合键判断
    vks.push_back(VK_SHIFT);
    vks.push_back(VK_CONTROL);
    vks.push_back(VK_MENU);

    std::array<bool, 256> current_state{};
    for (std::uint32_t vk : vks) {
        if (vk >= current_state.size()) continue;
        current_state[vk] = (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
    }

    // 检测上升沿并记录到 current_press_
    std::array<bool, 256> new_press{};
    for (std::uint32_t vk : vks) {
        if (vk >= current_state.size()) continue;
        if (current_state[vk] && !prev_state_[vk]) {
            new_press[vk] = true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_press_ = new_press;
    }

    prev_state_ = current_state;
}

void PollingKeyDetector::Update() {
    // 轮询版本在后台线程更新，此处无需操作。
}

bool PollingKeyDetector::WasKeyPressed(const std::string& key) {
    auto parts = ParseKeyCombination(key);
    if (parts.empty()) return false;

    std::string main_key = parts.back();
    std::vector<std::string> modifiers(parts.begin(), parts.end() - 1);
    std::uint32_t main_vk = KeyNameToVk(main_key);
    if (main_vk == 0 || main_vk >= 256) return false;

    bool pressed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pressed = current_press_[main_vk];
        current_press_[main_vk] = false;
    }

    if (!pressed) return false;

    // 检查修饰键
    bool modifiers_ok = true;
    for (const auto& mod : modifiers) {
        std::uint32_t vk = KeyNameToVk(mod);
        if (vk == 0) { modifiers_ok = false; break; }
        if ((GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) == 0) {
            modifiers_ok = false;
            break;
        }
    }

    if (!modifiers_ok) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (valid_keys_.find(main_key) != valid_keys_.end()) {
            wrong_key_pressed_.store(true, std::memory_order_release);
        }
        return false;
    }

    return true;
}

void PollingKeyDetector::SetValidKeys(const std::unordered_set<std::string>& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    valid_keys_ = keys;
}

} // namespace overlay::core
