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
- "队伍流程" 是一条按顺序编排的按键步骤列表，每个步骤带角色标签
- 同一角色可出现多段，且**各段按键可以不同**（如第一次有大招、第二次没有）
- 每个角色每段约 15-20 步，一个队伍流程可能有 3 个角色多段穿插（总步数可达 50+）
- 一个完整循环 = 队伍流程播放一次（循环播放）

---

## 技术方案

### 技术栈

| 组件 | 选择 | 理由 |
|------|------|-------|
| 窗口管理 | **Win32 API** | 原生支持置顶、点击穿透 |
| 透明合成 | **DirectComposition + DXGI 交换链** (`WS_EX_NOREDIRECTIONBITMAP`) | 真逐像素 alpha、GPU 硬件合成 |
| 2D 渲染 | **Direct2D 1.1** (DeviceContext + 交换链) | 硬件加速 |
| 文字渲染 | **DirectWrite** (DWrite) | 与 D2D 集成 |
| 图像加载 | **WIC** (Windows Imaging Component) | 原生 PNG/JPG 支持 |
| 编辑器 UI | **imgui** (docking branch, Win32+DX11 后端) | 轻量、易集成 |
| 键盘检测 | **GetAsyncKeyState** 轮询 | 无需管理员权限，反作弊安全 |
| 全局快捷键 | **RegisterHotKey** | 系统级注册，非钩子，反作弊安全 |
| JSON 解析 | **nlohmann/json** (单头文件) | 现代 C++ JSON 库 |
| 构建工具 | **CMake** + MSVC (`/MT` 静态运行时) | 跨 IDE + 真单 exe 打包 |

> ⚠️ **渲染方案注意**：不要使用 `WS_EX_LAYERED` + Direct2D —— Layered 窗口下 D2D 只能走
> `ID2D1DCRenderTarget`（GDI 软件拷贝路径），不是真正的硬件加速，每帧都要把位图拷回 GDI。
> 正确做法是 DComp 交换链 + `WS_EX_NOREDIRECTIONBITMAP`（Win8+，Win10/11 完全可用）。

### 架构概览

```
┌────────────────────────────────────────────────────────┐
│                    key_overlay.exe                      │
│                                                        │
│  ┌──────────────────────┐   ┌────────────────────────┐ │
│  │    Overlay Window     │   │   Editor Window (imgui) │ │
│  │  (D2D + DComp 交换链) │   │  - 队伍流程管理         │ │
│  │  - 按键方块渲染       │   │  - 角色段编排(拖拽)     │ │
│  │  - 背景图渲染         │   │  - 按键编辑(拖拽排序)   │ │
│  │  - 高亮/状态显示       │   │  - 背景图选择/预览     │ │
│  │  - 点击穿透           │   │  - 模式/快捷键设置      │ │
│  │  - 移动模式拖拽/缩放   │   │                        │ │
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

采用**扁平化单序列**模型：每个按键步骤自带角色标签，角色段只是显示分组。
（若用"角色顺序数组 + 角色名→序列的 map"，同一角色多次上场时各段只能共享一份按键，
无法表达"第二次上场打不同连招"的实际排轴。）

```cpp
// 队伍流程：一条有序的按键步骤列表
struct TeamRotation {
    std::string name;               // "常规循环·散秧维"
    std::string background_image;   // 背景图路径
    std::vector<KeyStep> steps;     // 完整按键序列（角色段 = 连续相同 character 的分组）
};

// 单个按键步骤
struct KeyStep {
    std::string character;    // "散华" — 角色标签，用于分段显示
    std::string key;          // "Q", "E", "Shift+1", "Space"
    std::string skill_name;   // "冰棱散射", "闪避"
    int duration_ms;          // 自动模式停留时间(ms)
};
```

编辑器中按连续相同 `character` 折叠显示为段：
`[散华 ×18] → [秧秧 ×15] → [维里奈 ×20] → [散华 ×10]`，段可整体拖拽排序。

---

## 详细设计

### 1. Overlay 窗口 — OverlayWindow

**窗口样式**:
- `WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`
- 渲染目标：DirectComposition visual 绑定 DXGI 交换链（premultiplied alpha），
  D2D1.1 DeviceContext 直接画到交换链 backbuffer
- **DPI 感知**：manifest 声明 Per-Monitor V2，避免高缩放屏上坐标/字号模糊

**交互方式**:

> ⚠️ 穿透模式（`WS_EX_TRANSPARENT`）下窗口**收不到任何鼠标消息**，
> 因此拖拽/滚轮必须先进入"移动模式"才能生效。

| 操作 | 功能 |
|------|------|
| **Ctrl+Shift+M** | 切换移动模式（临时移除 `WS_EX_TRANSPARENT`，窗口显示边框提示） |
| **移动模式下左键拖拽** | 移动窗口位置 |
| **移动模式下滚轮** | 缩放窗口大小 |
| **Ctrl+Shift+H** | 切换显示/隐藏 |
| **Ctrl+Shift+P** | 切换推进模式（自动/按键检测） |
| **Ctrl+Shift+E** | 打开编辑器窗口 |
| **Ctrl+Shift+R** | 重载配置 |

所有快捷键用 `RegisterHotKey` 注册（系统级、非钩子、反作弊安全），
配置字符串（如 `"Ctrl+Shift+H"`）由工具函数解析为修饰键 + VK 码。

**渲染 (Direct2D)**:
- 背景图渲染（全窗口背景）
- **可视窗口滚动**：总步数可能 50+，一行显示不下。以当前步骤为中心显示
  `[-3, +6]` 共 10 个键位，随推进平滑滚动
- 按键方块渲染（圆角矩形 + 阴影）
- 按键文字渲染（键名 + 技能名 + 角色分段色标）
- 状态指示（当前按键高亮发光，已完成变暗，未到灰色）
- 进度条/计数器显示当前进度
- 限帧 30fps 避免占用 GPU

### 2. 编辑器窗口 — EditorWindow (imgui)

**a) 队伍流程管理**
- 新建/打开/保存队伍流程
- 流程列表（所有保存的队伍流程）
- 重命名/删除

**b) 队伍流程编排**
- 角色段列表（连续相同角色的折叠分组，可整体拖拽排序）
  ```
  [散华 18步] → [秧秧 15步] → [维里奈 20步] → [散华 10步]
  ```
- 添加/删除角色段；同角色可添加多段，各段按键独立编辑

**c) 按键序列编辑**
- 按键列表（可拖拽排序，可跨段移动）
- 每项编辑：角色标签、按键名、技能标签、持续时间
- 添加/删除/复制按键步骤
- 支持特殊键与组合键（Ctrl, Shift, Space, F1-F12, Shift+1 等）

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
- `GetAsyncKeyState` 轮询，频率约 **120Hz**
- **边沿检测**：只在按键"按下瞬间"（上一帧抬起、当前帧按下）推进，
  避免按住一个键连跳多步
- **组合键处理**：`Shift+1` 需同时检测修饰键与主键都处于按下状态
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
      "name": "常规循环·散秧维",
      "background": "assets/bg_01.png",
      "steps": [
        { "character": "散华", "key": "E", "skill_name": "冰棱散射", "duration_ms": 2000 },
        { "character": "散华", "key": "R", "skill_name": "极寒领域", "duration_ms": 1500 },
        { "character": "秧秧", "key": "Q", "skill_name": "...", "duration_ms": 1800 }
      ]
    }
  ],
  "settings": {
    "display": {
      "opacity": 0.85,
      "scale": 1.0,
      "position": { "x": 100, "y": 100 },
      "window_width": 700,
      "window_height": 100,
      "visible_keys": { "before": 3, "after": 6 },
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
      "move_mode": "Ctrl+Shift+M",
      "reload_config": "Ctrl+Shift+R"
    },
    "input": {
      "poll_hz": 120,
      "wrong_key_flash": true,
      "timeout_skip_ms": 0
    }
  }
}
```

---

## 鸣潮兼容性注意事项

1. **全屏独占**: 鸣潮 (UE4) 真独占全屏下 DWM 不参与合成，任何 overlay 都无法显示，
   `SetWindowPos` 定期置顶也无效。**唯一可靠方案：无边框窗口模式**，在 README/首次运行提示中说明。
2. **反作弊安全**（鸣潮使用 ACE 反作弊）:
   - ✅ `GetAsyncKeyState` 安全（只读按键状态）
   - ✅ `RegisterHotKey` 安全（系统注册，非钩子）
   - ❌ **不要用** `SetWindowsHookEx(WH_KEYBOARD_LL)`（全局钩子可能触发反作弊）
   - ❌ **不要** 做任何内存操作
3. **尽早实测**: 叠加层一般被放行，但存在不确定性。**阶段 1 的空壳 overlay
   就要与鸣潮同时运行实测**，尽早暴露兼容性风险，而不是留到最后。
4. **性能**: D2D + DComp 渲染开销极低，限帧 30fps 不影响游戏

---

## 文件结构

```
project\mc\key_overlay/
├── CMakeLists.txt
├── app.manifest                  // Per-Monitor V2 DPI 感知
├── src/
│   ├── main.cpp                    // 入口
│   ├── overlay/
│   │   ├── OverlayWindow.h/.cpp    // 透明覆盖窗口
│   │   └── Direct2DRenderer.h/.cpp // D2D + DComp 交换链渲染器
│   ├── editor/
│   │   ├── EditorWindow.h/.cpp     // ImGui 编辑器 (Win32+DX11 后端)
│   │   └── EditorComponents.h/.cpp // 编辑器子组件
│   ├── core/
│   │   ├── PlaybackEngine.h/.cpp   // 播放引擎
│   │   ├── KeyDetector.h/.cpp      // 键盘输入检测（边沿检测/组合键）
│   │   ├── ConfigManager.h/.cpp    // 配置读写
│   │   └── TeamRotation.h          // 数据结构
│   └── utils/
│       ├── HotkeyManager.h/.cpp    // RegisterHotKey 封装 + 快捷键字符串解析
│       └── ResourceLoader.h/.cpp   // 图片加载
├── profiles/default.json
├── assets/
└── third_party/
    ├── imgui/                      // imgui docking branch
    └── nlohmann/                   // json.hpp
```

---

## 开发阶段

### 阶段 1 — 基础骨架 + 游戏兼容性实测
- 创建 CMake 项目，配置第三方依赖，添加 Per-Monitor V2 manifest
- OverlayWindow：DComp 交换链透明渲染、置顶、点击穿透
- D2D 渲染循环
- **验证**: 显示半透明窗口；**与鸣潮同时运行实测**（无边框模式），确认不被反作弊拦截

### 阶段 2 — 按键渲染
- TeamRotation / KeyStep 扁平数据结构
- 按键方块渲染（圆角矩形 + 文字 + 状态颜色）
- 可视窗口滚动（当前步骤居中，[-3, +6]）
- 自动推进逻辑
- **验证**: 显示按键方块条，自动循环并平滑滚动

### 阶段 3 — 键盘检测
- KeyDetector（GetAsyncKeyState，120Hz 轮询 + 边沿检测 + 组合键）
- 两种推进模式切换
- RegisterHotKey 快捷键系统（含移动模式 Ctrl+Shift+M + 拖拽/缩放）
- **验证**: 按键模式下按下正确键推进一次；移动模式下可拖拽缩放

### 阶段 4 — 编辑器
- 集成 imgui（Win32+DX11 后端）
- 队伍流程管理 + 角色段编排 + 按键编辑 + 背景图
- 配置保存/加载
- **验证**: 完整编辑并保存配置（含同角色多段不同按键）

### 阶段 5 — 完善
- 背景图渲染（WIC → D2D Bitmap）
- 窗口缩放/透明度/样式设置
- 进度条显示
- 热加载配置（`ReadDirectoryChangesW` 文件监视）

### 阶段 6 — 测试打包
- Win10/Win11 测试、高 DPI/多显示器测试
- 与鸣潮完整流程联测
- `/MT` 静态运行时打包单 exe

---

## 验证流程

1. **编译**: `cmake --build build` 零错误
2. **启动**: 显示透明 overlay 窗口在屏幕角落（高 DPI 下无模糊）
3. **默认配置**: 显示示例按键序列，可视窗口正确滚动
4. **自动模式**: 按键自动推进 → 循环
5. **按键模式**: 切换后按正确键推进；按住不放不连跳；组合键可识别
6. **移动模式**: Ctrl+Shift+M 后可拖拽移动、滚轮缩放，退出后恢复穿透
7. **编辑器**: Ctrl+Shift+E 打开，编辑（含同角色多段）→ 保存 → 生效
8. **背景图**: 编辑器选择图片 → 窗口更新
9. **游戏测试**: 与鸣潮同时运行（无边框模式，尽早实测）
