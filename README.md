# MingC Key Overlay

鸣潮（Wuthering Waves）弹幕式按键提示 overlay。

---

## 项目简介

MingC Key Overlay 是一款面向《鸣潮》的**非注入式**按键提示工具。它通过 Windows 透明覆盖窗口与 Direct2D / DirectComposition 渲染，在屏幕上以弹幕形式显示当前队伍输出循环的按键序列、阶段进度与角色头像，帮助玩家熟悉并复现复杂连招。

程序不注入游戏进程、不使用全局钩子，仅通过 `RegisterHotKey` 全局热键与 `GetAsyncKeyState` 轮询实现输入检测，反作弊风险极低。

---

## 功能特性

- **弹幕式按键序列提示**：当前步骤高亮白色，已完成灰暗，未到达更暗，直观清晰。
- **双模式切换**：自动播放（按预设时间推进）与按键检测（按实际按键推进）。
- **阶段条与进度条**：顶部阶段标签 + 细条整体进度，随时掌握循环所处阶段。
- **角色头像与主题色**：左侧显示当前步骤角色头像，按键块边缘带角色主题色。
- **背景图支持**：每个队伍流程可独立设置背景图，支持 PNG / JPG / BMP。
- **内置可视化编辑器**：基于 ImGui 的 Docking 编辑器，支持流程/角色/阶段/按键全编排。
- **全局热键**：所有功能均可通过可自定义的热键快速操作。
- **配置文件热加载**：外部修改 `default.json` 后自动生效（编辑器打开期间暂停）。
- **撤销 / 重做**：编辑器内最大支持 **50 步** 历史回溯。
- **原子保存**：保存时先写临时文件，再通过 `MoveFileExW` 替换，避免写坏配置。
- **高 DPI 感知**：内置 `app.manifest` 启用 Per-Monitor V2，高分屏不模糊。

---

## 系统要求

- **操作系统**：Windows 10 / Windows 11（64 位）
- **编译器**：Visual Studio 2022（MSVC v143+）
- **构建工具**：CMake 3.21+
- **版本控制**：Git（用于 CMake `FetchContent` 拉取 nlohmann/json 与 ImGui）

---

## 快速开始

### 1. 首次运行

直接运行 `MingCKeyOverlay.exe`：

- 程序会自动在同级目录创建 `profiles/` 与 `assets/`。
- 若 `profiles/default.json` 不存在，程序会写出内置默认配置。
- 默认流程为「常规循环·散秧维」，包含散华、秧秧、维里奈三位角色的示例循环。

### 2. 游戏设置建议

**强烈建议将鸣潮设为「无边框窗口」模式。**

在真·全屏独占（Fullscreen Exclusive）模式下，DWM 不参与合成，非注入式 overlay 可能无法显示或显示异常。无边框窗口模式下 overlay 可正常叠加。

### 3. Overlay 显示逻辑

- **左侧**：当前步骤角色头像（56 px，可缩放）。
- **顶部**：阶段条，显示当前阶段标签（如「攒能量」「满能量放强化重击」）。
- **中间**：按键序列，可见范围为当前步骤前 3 步、后 6 步，共 10 个按键块。
- **底部**：若开启，显示一根细进度条表示整体循环完成度。
- **边框**：移动模式下边框变为黄色，提示已临时移除点击穿透。

### 4. 移动与缩放

- 按 `Ctrl+Shift+M` 进入**移动模式**：窗口边框变黄，临时移除点击穿透。
- **左键拖拽**可移动窗口位置。
- **滚轮**以鼠标位置为锚点缩放（范围 0.5x ~ 2.0x）。
- 再次按 `Ctrl+Shift+M` 或按 `ESC` 退出移动模式，恢复点击穿透。

---

## 默认热键

所有热键均可在编辑器「设置」面板中修改。若某个组合被其他软件占用，`RegisterHotKey` 注册失败，对应项会在编辑器热键设置中显示异常（可更换组合后保存）。

| 热键 | 功能 | 说明 |
|------|------|------|
| `Ctrl+Shift+M` | 切换移动模式 | 可拖拽 / 滚轮缩放 |
| `Ctrl+Shift+H` | 显示 / 隐藏 | 隐藏时 DWM 不参与合成，零开销 |
| `Ctrl+Shift+Space` | 播放 / 暂停 | 聊天或暂离时暂停，防止误推进 |
| `Ctrl+Shift+P` | 切换模式 | 自动播放 / 按键检测 |
| `Ctrl+Shift+N` | 切换队伍流程 | 循环切换所有已保存流程 |
| `Ctrl+Shift+E` | 打开编辑器 | 再次按下关闭编辑器 |
| `Ctrl+Shift+R` | 重载配置 | 外部修改 JSON 后手动生效 |
| `Ctrl+Shift+Q` | 退出程序 | 优雅关闭所有子系统 |

> 默认全部使用 `Ctrl+Shift` 前缀，以避开鸣潮默认键位，减少热键吞键影响。

---

## 编辑器使用

### 打开方式

- 按 `Ctrl+Shift+E` 呼出编辑器窗口。
- 或运行程序后手动呼出。

### 布局说明

编辑器采用 **ImGui Docking** 布局，主要区域如下：

- **上方三列**：
  - **流程列表**：新建 / 重命名 / 删除队伍流程，设为当前激活流程。
  - **角色段列表**：按角色分段的步骤聚合，支持拖拽排序。
  - **角色视觉信息**：添加 / 删除角色，编辑角色名、头像路径、主题色。
- **下方两列**：
  - **阶段标记**：添加 / 删除 / 编辑阶段名称、起始步骤、颜色覆盖。
  - **按键序列**：表格形式编辑每一步的角色、按键、技能名、图标类型、持续时间；支持添加 / 删除 / 复制 / 拖拽排序。

### 背景图设置

在「背景图设置」窗口中：

1. 点击「选择图片...」打开文件对话框。
2. 支持 PNG / JPG / BMP。
3. 程序会自动将图片复制到 `assets/` 目录，若文件名冲突则自动重命名（如 `bg_01.png` → `bg_01_1.png`）。
4. 复制完成后路径自动写入当前流程，可实时预览。

### 撤销与重做

- **撤销**：`Ctrl+Z`
- **重做**：`Ctrl+Y` 或 `Ctrl+Shift+Z`
- 最大历史步数：**50 步**

> 仅在编辑器窗口处于焦点时生效。

### 保存

- 点击流程列表中的「保存」按钮，或按 `Ctrl+S`。
- 保存采用**原子写入**：先写入 `.tmp` 临时文件，刷盘后通过 `MoveFileExW` 替换原文件，防止写坏配置。
- 保存后 overlay 即时生效（热键、显示参数、流程内容均立即更新）。

---

## JSON 配置说明

配置文件位于 `profiles/default.json`，UTF-8 编码。程序首次运行时会自动写出默认配置。

### 顶层字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `active_rotation` | string | 当前激活的队伍流程名称 |
| `settings` | object | 全局设置 |
| `team_rotations` | array | 所有队伍流程数组 |

### `settings.display` — 显示设置

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `opacity` | `0.85` | 整体不透明度（0.1 ~ 1.0） |
| `scale` | `1.0` | 全局缩放（0.5 ~ 2.0） |
| `position.x` / `position.y` | `100` / `100` | 窗口初始位置 |
| `mode` | `"auto"` | 启动模式：`"auto"` 自动播放 / `"key"` 按键检测 |
| `loop` | `true` | 是否循环播放 |
| `show_avatar` | `true` | 显示角色头像 |
| `show_stage_bar` | `true` | 显示阶段条 |
| `show_progress` | `true` | 显示整体进度条 |
| `show_arrows` | `true` | 显示步骤间箭头 |
| `avatar_size` | `56` | 头像尺寸（像素） |
| `stage_bar_height` | `28` | 阶段条高度（像素） |
| `visible_keys.before` | `3` | 当前步骤前方可见步数 |
| `visible_keys.after` | `6` | 当前步骤后方可见步数 |

### `settings.display.key_style` — 按键样式

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `key_width` | `90` | 按键块宽度 |
| `key_height` | `44` | 按键块高度 |
| `spacing` | `10` | 按键块间距 |
| `border_radius` | `8` | 圆角半径 |
| `font_size` | `16` | 按键文字字号 |
| `skill_name_font_size` | `14` | 技能名字号 |
| `icon_size` | `20` | 图标尺寸 |
| `active_color` | `"#F3F4F6"` | 当前步骤背景色 |
| `active_text_color` | `"#111827"` | 当前步骤文字色 |
| `done_color` | `"#4B5563"` | 已完成步骤背景色 |
| `pending_color` | `"#1F2937"` | 未到达步骤背景色 |
| `text_color` | `"#F3F4F6"` | 普通文字色 |
| `arrow_color` | `"#9CA3AF"` | 箭头颜色 |

### `settings.hotkeys` — 热键映射

| 字段 | 默认值 | 对应功能 |
|------|--------|----------|
| `move_mode` | `Ctrl+Shift+M` | 切换移动模式 |
| `toggle_visibility` | `Ctrl+Shift+H` | 显示/隐藏 |
| `play_pause` | `Ctrl+Shift+Space` | 播放/暂停 |
| `toggle_mode` | `Ctrl+Shift+P` | 切换自动/按键模式 |
| `next_rotation` | `Ctrl+Shift+N` | 下一个队伍流程 |
| `open_editor` | `Ctrl+Shift+E` | 打开编辑器 |
| `reload_config` | `Ctrl+Shift+R` | 重载配置 |
| `quit` | `Ctrl+Shift+Q` | 退出程序 |

### `settings.input` — 输入检测

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `foreground_only` | `true` | 仅当目标进程处于前台时检测按键 |
| `poll_hz` | `60` | 轮询频率（30 ~ 120 Hz） |
| `target_process` | `"Client-Win64-Shipping.exe"` | 目标进程名（前台判定用） |
| `timeout_skip_ms` | `0` | 超时自动跳过（0 = 禁用） |
| `wrong_key_flash` | `true` | 按错键时屏幕红闪提示 |

### `team_rotations[]` — 队伍流程

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 流程名称（唯一标识） |
| `background_image` | string | 背景图相对路径（如 `assets/bg_01.png`） |
| `characters` | array | 角色列表：`name`、`avatar_image`、`theme_color` |
| `stages` | array | 阶段标记：`label`、`color`、`start_step` |
| `steps` | array | 按键步骤：`character`、`key`、`skill_name`、`key_icon`、`custom_icon`、`duration_ms` |

> `key_icon` 可选值：`""`（自动推断）、`"mouse_left"`、`"mouse_left_hold"`、`"mouse_right"`、`"mouse_middle"`、`"mouse_x1"`、`"mouse_x2"`、`"keyboard"`、`"custom"`。

---

## 全屏 / 反作弊兼容性

### 不注入、不 Hook

- 仅使用 `RegisterHotKey` 注册系统全局热键。
- 按键检测使用 `GetAsyncKeyState` 轮询或本窗口 `WM_KEYDOWN` / `WM_KEYUP` 事件。
- 不读写游戏内存、不注入 DLL、不安装全局钩子。

### 反作弊风险

风险极低，但最终兼容性需以实测为准。若遇到热键被拦截或 overlay 被隐藏，可尝试：

1. 切换为**无边框窗口**模式。
2. 使用副屏方案（将 overlay 放在第二显示器）。

### 全屏独占问题

真·全屏独占（Fullscreen Exclusive）下 DWM 不参与合成，非注入式 overlay 可能无法显示或闪烁。**强烈建议始终使用无边框窗口模式。**

### 热键吞键

`RegisterHotKey` 命中后，该组合键被系统消费，游戏收不到此次按键。因此默认热键全部使用 `Ctrl+Shift` 前缀，避开鸣潮默认 WASD / 技能键位，降低对实际操作的干扰。

### MPO 渲染损坏（Windows 11 24H2+）

部分 Win11 24H2 设备在多平面叠加（MPO）场景下可能出现 overlay 残影或冻结。可尝试注册表调整：

```
HKLM\Software\Microsoft\Windows\Dwm\OverlayMinFPS = 0
```

> 修改注册表前请备份，风险自负。

---

## 构建

### 环境准备

1. 安装 **Visual Studio 2022**（勾选「使用 C++ 的桌面开发」工作负载）。
2. 安装 **CMake 3.21+**。
3. 确保 `git` 在 PATH 中（用于 `FetchContent` 自动下载依赖）。

### Debug 构建

```bash
cmake --preset debug
cmake --build build/debug --config Debug
```

输出：`build/debug/bin/MingCKeyOverlay.exe`

### Release 构建

```bash
cmake --preset release
cmake --build build/release --config Release
```

输出：`build/release/bin/MingCKeyOverlay.exe`

> 项目使用 `/MT` 静态运行时，Release 构建无额外 MSVC 运行时 DLL 依赖。

---

## 打包发布

### 一键打包脚本

```bash
scripts/package-release.bat
```

执行流程：

1. `cmake --preset release` 配置 Release。
2. `cmake --build build/release --config Release` 构建。
3. `cpack -G ZIP -C Release` 生成绿色 ZIP 包。

输出：`build/release/MingCKeyOverlay-0.1.0-win64.zip`

ZIP 包根目录直接包含可执行文件、`profiles/`、`assets/`、`README.md`，解压即用，无需安装。

### 冒烟测试（CI / 本地验证）

```bash
scripts/build-and-test.bat
```

执行 Debug + Release 双配置构建，并以 `--smoke-test` 运行程序：窗口创建、D2D/DComp/ImGui 初始化、首帧渲染验证通过后，2 秒自动退出，返回码 `0` 表示通过。

---

## 目录结构

```
MingC/
├── src/                    # 源码
│   ├── overlay/            # 透明覆盖窗口与 D2D/DComp 渲染
│   ├── editor/             # ImGui 编辑器窗口与组件
│   ├── core/               # 配置管理、播放引擎、输入检测、数据模型
│   └── utils/              # 日志、编码、资源加载、热键、文件监视
├── profiles/               # JSON 配置（运行时自动生成 default.json）
│   └── schema.json         # 配置 JSON Schema
├── assets/                 # 图片资源（背景图、头像，运行时自动填充）
├── scripts/                # 构建与打包脚本
│   ├── build-and-test.bat
│   └── package-release.bat
├── app.manifest            # 高 DPI 感知清单
├── CMakeLists.txt          # 主构建脚本（含 CPack ZIP 配置）
├── CMakePresets.json       # CMake Preset（debug / release）
└── README.md               # 本文档
```

---

## 常见问题

| 现象 | 可能原因 | 解决 |
|------|---------|------|
| Overlay 不显示 | 游戏为全屏独占模式 | 切换为**无边框窗口** |
| Overlay 显示但按键不推进 | 当前为自动播放模式，或前台门控生效 | 按 `Ctrl+Shift+P` 切换为按键检测；确认游戏前台 |
| 热键无效 | 组合被其他软件占用 | 打开编辑器修改热键，或关闭冲突软件 |
| 编辑器中文显示方框 | 系统缺少 `msyh.ttc` | 安装微软雅黑字体；程序会依次尝试 `msyh.ttc` → `msyhbd.ttc` → `simhei.ttf`，均失败则回退默认英文 |
| 配置文件损坏无法启动 | JSON 语法错误 | 删除 `profiles/default.json`，程序会重新写出内置默认配置 |
| 高 DPI 下模糊 | 未启用 Per-Monitor V2 | 确保 `app.manifest` 存在（已内置），或注销重新登录 |
| 保存配置后未生效 | 编辑器保存期间热加载被暂停 | 正常现象，关闭编辑器后自动恢复；或按 `Ctrl+Shift+R` 手动重载 |
| 运行时提示缺少 DLL | 使用了 Debug 运行时 | 对外分发请使用 Release 构建（`/MT` 静态链接） |
