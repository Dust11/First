# 弹幕按键提示程序 — 实现计划

## 需求概述

一个类似弹幕的屏幕覆盖层程序，用于游戏中提示队伍流程的按键顺序。

### 核心需求
- **透明覆盖窗口** — 始终最前、点击穿透、不遮挡游戏操作
- **按键序列显示** — 一行排列的按键方框，像长条形小窗循环播放
- **两种推进模式** — (1) 自动弹幕播放 (2) 检测键盘输入自动推进
- **可编辑背景图** — 每个队伍流程可自定义背景图片
- **独立图形化编辑器** — 单独的编辑器窗口，拖拽式编辑按键序列
- **位置/大小可调** — 拖拽移动、滚轮缩放
- **目标游戏**: 鸣潮 (Wuthering Waves), 支持全屏独占
- **运行环境**: Windows 10/11, C++ 实现

### 流程结构
- "队伍流程" 包含多个角色的按键序列段，按自定义顺序编排
- 每个角色约 15-20 步，每个队伍流程可能有 3 个角色多段穿插
- 一个完整循环 = 队伍流程播放一次（循环播放）

---

## 技术方案

### 技术栈

| 组件 | 选择 | 理由 |
|------|------|-------|
| 窗口管理 | **Win32 API** | WS_EX_LAYERED + WS_EX_TRANSPARENT 原生支持 |
| 2D 渲染 | **Direct2D** (D2D1) | 硬件加速透明渲染 |
| 文字渲染 | **DirectWrite** (DWrite) | 与 D2D 集成 |
| 图像加载 | **WIC** (Windows Imaging Component) | 原生 PNG/JPG 支持 |
| 编辑器 UI | **imgui** (docking branch) | 轻量、易集成、跨 Win32 |
| 键盘检测 | **GetAsyncKeyState** 轮询 | 无需管理员权限，反作弊安全 |
| JSON 解析 | **nlohmann/json** (单头文件) | 现代 C++ JSON 库 |
| 构建工具 | **CMake** | 跨编译器 + IDE 支持 |

### 架构概览

```
┌────────────────────────────────────────────────────────┐
│                    key_overlay.exe                      │
│                                                        │
│  ┌──────────────────────┐   ┌────────────────────────┐ │
│  │    Overlay Window     │   │   Editor Window (imgui) │ │
│  │  (D2D Rendering)      │   │  - 队伍流程管理         │ │
│  │  - 按键方块渲染       │   │  - 角色顺序编排(拖拽)   │ │
│  │  - 背景图渲染         │   │  - 按键编辑(拖拽排序)   │ │
│  │  - 高亮/状态显示       │   │  - 背景图选择/预览     │ │
│  │  - 点击穿透           │   │  - 模式/快捷键设置      │ │
│  │  - 右键拖拽/缩放      │   │                        │ │
│  └──────────┬───────────┘   └───────────┬────────────┘ │
│             │                            │              │
│             └──────────┬─────────────────┘              │
│                        │                                │
│           ┌────────────▼─────────────┐                  │
│           │    Core Engine           │                  │
│           │  - 配置管理(JSON)        │                  │
│           │  - GetAsyncKeyState 检测  │                  │
│           │  - 自动/手动推进逻辑     │                  │
│           │  - 快捷键系统            │                  │
│           │  - 播放状态机            │                  │
│           └──────────────────────────┘                  │
└────────────────────────────────────────────────────────┘
```

---

## 数据结构

```cpp
// 队伍流程：多个角色的按键序列组合
struct TeamRotation {
    std::string name;                    // "深境螺旋·常规循环"
    std::string background_image;        // 背景图路径
    std::vector<std::string> character_order; // 角色顺序 ["散华","秧秧","维里奈","散华"]
    std::map<std::string, KeySequence> character_sequences; // 角色名 → 按键序列
};

// 按键序列：一个角色的全部按键
struct KeySequence {
    std::string character_name;           // "散华"
    std::vector<KeyStep> steps;           // 按键步骤
};

// 单个按键步骤
struct KeyStep {
    std::string key;                      // "Q", "E", "Shift+1", "Space"
    std::string skill_name;               // "冰棱散射", "闪避"
    int duration_ms;                      // 自动模式停留时间(ms)
};
```

---

## 详细设计

### 1. Overlay 窗口 — OverlayWindow

**窗口样式**:
- `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`
- `SetLayeredWindowAttributes` 或 `UpdateLayeredWindow` 实现透明

**交互方式**:
| 操作 | 功能 |
|------|------|
| **右键拖拽** | 移动窗口位置（拖拽时临时移除 WS_EX_TRANSPARENT） |
| **鼠标滚轮** | 缩放窗口大小 |
| **Ctrl+Shift+H** | 切换显示/隐藏 |
| **Ctrl+Shift+P** | 切换推进模式（自动/按键检测） |
| **Ctrl+Shift+E** | 打开编辑器窗口 |
| **Ctrl+Shift+R** | 重载配置 |

**渲染 (Direct2D)**:
- 背景图渲染（全窗口背景）
- 按键方块渲染（圆角矩形 + 阴影）
- 按键文字渲染（键名 + 技能名）
- 状态指示（当前按键高亮发光，已完成变暗，未到灰色）
- 进度条/计数器显示当前进度
- 限帧 30fps 避免占用 GPU

### 2. 编辑器窗口 — EditorWindow (imgui)

**a) 队伍流程管理**
- 新建/打开/保存队伍流程
- 流程列表（所有保存的队伍流程）
- 重命名/删除

**b) 队伍流程编排**
- 角色顺序列表（可拖拽排序）
  ```
  [散华 18步] → [秧秧 15步] → [维里奈 20步] → [散华 10步]
  ```
- 添加/删除角色段
- 每段对应一个角色的按键序列

**c) 按键序列编辑**
- 按键列表（可拖拽排序）
- 每项编辑：按键名、技能标签、持续时间
- 添加/删除/复制按键步骤
- 支持特殊键（Ctrl, Shift, Space, F1-F12 等）

**d) 背景图管理**
- 背景图预览
- 从本地选择图片（PNG/JPG）
- 每个队伍流程独立绑定背景图

**e) 设置**
- 窗口默认位置/大小
- 默认推进模式
- 快捷键配置
- 按键方块样式（颜色、圆角大小、间距等）

### 3. 播放引擎 — PlaybackEngine

**模式A: 自动弹幕播放**
- 每个按键步骤有 `duration_ms`
- 计时器到时自动推进
- 显示进度条/倒计时

**模式B: 按键输入检测**
- 使用 `GetAsyncKeyState` 轮询当前步骤的按键
- 检测到正确按键后自动推进到下一步
- 按错键无反应（或红色闪烁反馈）
- 可配置超时自动跳过

**状态机**:
```
IDLE → PLAYING → PAUSED → PLAYING → FINISHED → IDLE
         ↓
       EDITING (打开编辑器时暂停)
```

### 4. 配置文件 (JSON)

```json
{
  "team_rotations": [
    {
      "name": "深境螺旋·常规循环",
      "background": "assets/deep_abyss.png",
      "character_order": ["散华", "秧秧", "维里奈", "散华"],
      "character_sequences": {
        "散华": {
          "character_name": "散华",
          "steps": [
            { "key": "E", "skill_name": "冰棱散射", "duration_ms": 2000 },
            { "key": "R", "skill_name": "极寒领域", "duration_ms": 1500 }
          ]
        },
        "秧秧": { ... },
        "维里奈": { ... }
      }
    }
  ],
  "settings": {
    "display": {
      "opacity": 0.85,
      "scale": 1.0,
      "position": { "x": 100, "y": 100 },
      "window_width": 700,
      "window_height": 100,
      "key_style": {
        "key_size": 50,
        "spacing": 10,
        "border_radius": 8,
        "active_color": "#FFD700",
        "font_size": 16
      },
      "mode": "auto",
      "show_progress": true
    },
    "hotkeys": {
      "toggle_visibility": "Ctrl+Shift+H",
      "toggle_mode": "Ctrl+Shift+P",
      "open_editor": "Ctrl+Shift+E",
      "play_pause": "Ctrl+Shift+Space",
      "reload_config": "Ctrl+Shift+R"
    }
  }
}
```

---

## 鸣潮兼容性注意事项

1. **全屏独占**: 鸣潮 (UE4) 全屏独占可能遮挡 overlay。解决方案:
   - 提示用户使用无边框窗口模式
   - 或使用 `SetWindowPos` 定期置顶
2. **反作弊安全**:
   - ✅ `GetAsyncKeyState` 安全（只读按键状态）
   - ❌ **不要用** `SetWindowsHookEx(WH_KEYBOARD_LL)`（全局钩子可能触发反作弊）
   - ❌ **不要** 做任何内存操作
3. **性能**: D2D 渲染开销极低，限帧 30fps 不影响游戏

---

## 文件结构

```
project\mc\key_overlay/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                    // 入口
│   ├── overlay/
│   │   ├── OverlayWindow.h/.cpp    // 透明覆盖窗口
│   │   └── Direct2DRenderer.h/.cpp // D2D 渲染器
│   ├── editor/
│   │   ├── EditorWindow.h/.cpp     // ImGui 编辑器
│   │   └── EditorComponents.h/.cpp // 编辑器子组件
│   ├── core/
│   │   ├── PlaybackEngine.h/.cpp   // 播放引擎
│   │   ├── KeyDetector.h/.cpp      // 键盘输入检测
│   │   ├── ConfigManager.h/.cpp    // 配置读写
│   │   └── TeamRotation.h          // 数据结构
│   └── utils/
│       ├── HotkeyManager.h/.cpp    // 快捷键
│       └── ResourceLoader.h/.cpp   // 图片加载
├── profiles/default.json
├── assets/
└── third_party/
    ├── imgui/                      // imgui docking branch
    └── nlohmann/                   // json.hpp
```

---

## 开发阶段

### 阶段 1 — 基础骨架
- 创建 CMake 项目，配置第三方依赖
- OverlayWindow：透明、置顶、点击穿透
- D2D 渲染循环
- **验证**: 显示半透明窗口，可拖拽

### 阶段 2 — 按键渲染
- TeamRotation / KeySequence 数据结构
- 按键方块渲染（圆角矩形 + 文字 + 状态颜色）
- 自动推进逻辑
- **验证**: 显示一行按键方块，自动循环

### 阶段 3 — 键盘检测
- KeyDetector (GetAsyncKeyState)
- 两种推进模式切换
- 快捷键系统
- **验证**: 按键模式下按下正确键推进

### 阶段 4 — 编辑器
- 集成 imgui
- 队伍流程管理 + 按键编辑 + 背景图
- 配置保存/加载
- **验证**: 完整编辑并保存配置

### 阶段 5 — 完善
- 背景图渲染（WIC → D2D Bitmap）
- 窗口缩放/透明度/样式设置
- 进度条显示
- 热加载配置

### 阶段 6 — 测试打包
- Win10/Win11 测试
- 与鸣潮同时运行测试
- 打包单 exe

---

## 验证流程

1. **编译**: `cmake --build build` 零错误
2. **启动**: 显示透明 overlay 窗口在屏幕角落
3. **默认配置**: 显示示例按键序列
4. **自动模式**: 按键自动推进 → 循环
5. **按键模式**: 切换后按正确键推进
6. **编辑器**: Ctrl+Shift+E 打开，编辑 → 保存 → 生效
7. **背景图**: 编辑器选择图片 → 窗口更新
8. **游戏测试**: 与鸣潮同时运行
