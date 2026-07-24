# MingC 实现进度记录

> 记录时间：2026-07-24
> 当前分支：`feat/implementation`
> 记录原因：用户暂时终止会话，保存当前进度供下次继续。

## 已完成任务

### Task 15：实现编辑器队伍流程与角色段编排
- 状态：已完成并已推送（commit `c5069ed`，已推送到 `feat/implementation`）。
- 主要内容：
  - 新增 `EditorComponents` 类，实现三列编辑器：流程列表、角色段列表、角色视觉信息。
  - 支持流程新建/重命名/删除/设为当前/保存。
  - 支持角色段的添加/删除与拖拽排序（`SEGMENT_DND`）。
  - 支持角色头像路径、主题色编辑。
  - 修复了编辑器未初始化时调用 ImGui backend shutdown 导致的退出挂起。
  - 修复了 COM 资源在 `CoUninitialize()` 之后释放导致的段错误。

### Task 16：实现编辑器阶段标记与按键序列编辑
- 状态：已完成并已推送（commit `b2dee9e`，已推送到 `feat/implementation`）。
- 主要内容：
  - 在编辑器主窗口下方新增两列面板：阶段标记 + 按键序列。
  - **阶段标记面板**：
    - 添加/删除阶段。
    - 编辑阶段名称、起始步骤（自动排序并保证首个阶段从 0 开始）。
    - 颜色可选覆盖或自动使用角色主题色。
  - **按键序列面板**：
    - 使用 `ImGuiListClipper` 优化大列表（50+ 步）。
    - 可编辑每步的角色、按键、技能名、图标类型、自定义图标路径、持续时间。
    - 支持拖拽排序（`STEP_DND` payload）。
    - 支持添加/删除/复制步骤。
  - 数据模型：
    - `KeyStep` 新增 `custom_icon` 字段。
    - JSON 序列化宏改用 `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`，兼容缺少 `custom_icon` 的旧配置。
- 验证：
  - 使用 MSBuild 编译 `build/debug/MingCKeyOverlay.sln` 成功（0 错误）。
  - 默认运行（5 秒自动关闭）退出码 `exit=0`。
  - 临时开启编辑器自动显示的运行退出码 `exit=0`（已回退该临时改动）。

### Task 17：实现编辑器背景图、设置与原子保存
- 状态：已完成并已推送（commit `2df96a3`，已推送到 `feat/implementation`）。
- 主要内容：
  - **背景图设置面板**：
    - 可手动编辑 `TeamRotation::background_image` 相对路径。
    - 通过 Win32 `GetOpenFileNameW` 选择本地 PNG/JPG/BMP。
    - 选择后自动复制到 `assets/`（重名时自动加序号），JSON 中保存相对路径。
    - 使用 `ResourceLoader` + D3D11 SRV 在编辑器中实时预览背景图。
  - **设置面板**（独立可停靠窗口）：
    - `display`：不透明度、缩放、窗口位置、循环播放、显示头像/阶段条/进度条/箭头、头像大小、阶段条高度。
    - `key_style`：按键宽度/高度/间距/圆角，当前/已完成/未到达背景色、文字色、箭头色。
    - `hotkeys`：所有动作的热键字符串编辑。
    - `input`：轮询频率、仅前台检测、目标进程名、错键红闪、超时自动跳过。
  - **原子保存**：编辑器继续调用已有的 `ConfigManager::Save()`（tmp + FlushFileBuffers + MoveFileExW）。
  - 代码改动：
    - `src/core/ConfigManager.h`：新增 `GetExeDirectory()`。
    - `src/editor/EditorComponents.h/.cpp`：新增 `DrawBackgroundImage`、`DrawSettings`、`UpdateBackgroundPreview`、`CreatePreviewTexture`。
    - `src/editor/EditorWindow.cpp`：渲染时把 D3D11 设备传给 `EditorComponents::Draw`。
- 验证：
  - MSBuild Debug x64 编译 0 错误。
  - 默认 5 秒运行退出码 `exit=0`。
  - 临时开启编辑器自动显示运行 5 秒退出码 `exit=0`（已回退）。

### Task 18：实现撤销重做与 `EditHistory`
- 状态：已完成并已推送（commit `937429e`，已推送到 `feat/implementation`）。
- 主要内容：
  - 新增 `src/editor/EditHistory.h`：基于 `AppConfig` 快照的撤销重做栈（最大 50 步）。
  - 在 `EditorComponents` 中集成 `EditHistory`：
    - 首次打开时自动记录初始快照（此时不可撤销）。
    - 流程、角色段、角色、阶段、按键序列、背景图、设置等所有有意义编辑后调用 `history_.Push(cfg)`。
    - 文本/数值输入使用 `IsItemDeactivatedAfterEdit()` 避免逐字符快照。
  - 快捷键：编辑器主窗口焦点下 `Ctrl+Z` 撤销、`Ctrl+Y`/`Ctrl+Shift+Z` 重做。
  - 撤销/重做时恢复整个 `AppConfig`、调用 `apply_callback` 使 overlay 同步更新，并清空延迟操作/重命名等瞬态状态。
  - 编辑器关闭（`EditorWindow::Hide`/`Shutdown`）时调用 `OnEditorHidden()` 清空历史，不跨会话持久化。
- 代码改动：
  - `src/editor/EditHistory.h`：新增。
  - `src/editor/EditorComponents.h/.cpp`：新增 `OnEditorHidden`、`ClearTransientState`，所有绘制函数增加 `history_.Push` 调用点。
  - `src/editor/EditorWindow.cpp`：`Hide()`/`Shutdown()` 调用 `components_.OnEditorHidden()`。
- 验证：
  - MSBuild Debug x64 编译 0 错误。
  - 默认 5 秒运行退出码 `exit=0`。
  - 临时开启编辑器自动显示运行 5 秒退出码 `exit=0`（已回退）。

## 未推送提交

无。

## 待办任务

- [ ] **Task 19**：完善背景图、进度条与热加载
- [ ] **Task 20**：测试、打包与 README 完善

## 关键命令备忘

- 构建（Debug x64）：
  ```bash
  "/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" \
    "$(pwd -W)/build/debug/MingCKeyOverlay.sln" -p:Configuration=Debug -p:Platform=x64 -m
  ```
- 运行测试：
  ```bash
  ./build/debug/bin/MingCKeyOverlay.exe
  ```
- 推送：
  ```bash
  git push
  ```

## 注意事项

- 当前 `main.cpp` 中保留了一个 5 秒自动关闭计时器，仅用于阶段验证，最终产品应移除。
- 编辑器自动显示测试改动已回退，当前 `main.cpp` 为干净状态。
