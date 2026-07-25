# MingC 实现进度记录

> 记录时间：2026-07-25
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

### Task 19：完善背景图、进度条与热加载
- 状态：已完成并已推送（commit `b562fc4`，已推送到 `feat/implementation`）。
- 主要内容：
  - **背景图 cover 渲染**：
    - `VisualRenderer::DrawBackground()` 保持等比 cover 裁剪计算，绘制时改用 `Direct2DRenderer::DrawBitmapHighQuality()`。
    - `Direct2DRenderer` 新增 `DrawBitmapHighQuality()`，使用 `ID2D1Effect`（`CLSID_D2D1Scale`） cubic 插值，`CreateEffect` 失败时回退普通 `DrawBitmap`。
  - **进度条**：
    - `RenderState` 新增 `overall_progress`（0..1）与 `show_progress`。
    - 新增 `VisualRenderer::DrawProgressBar()`：阶段条下方 4px（缩放后）细条，半透明轨道 + 主题色填充。
    - `main.cpp` 渲染循环根据 `display.show_progress` 填充开关，并按 `(current_step + step_progress) / total_steps` 计算总进度。
  - **配置热加载**：
    - 新增 `src/utils/FileWatcher.h/.cpp`：使用 `ReadDirectoryChangesW`（overlapped）监听 `profiles/default.json` 所在目录，文件修改时 `PostMessageW` 通知主窗口。
    - `main.cpp` 通过 `SetWindowSubclass` 处理 `WM_USER_CONFIG_CHANGED`，400ms 防抖定时器后调用 `ReloadConfig`。
    - 编辑器打开/保存期间 `Pause()`/`Resume()` 文件监视，避免保存触发热加载竞争。
    - `AppContext` 新增 `std::mutex config_mutex`，`ReloadConfig` 与渲染循环共享状态访问均加锁，避免热加载与渲染线程数据竞争。
  - `CMakeLists.txt` 加入 `FileWatcher` 源文件与 `comctl32` 库。
- 代码改动：
  - 新增 `src/utils/FileWatcher.h/.cpp`。
  - 修改 `src/overlay/IRenderer.h`、`Direct2DRenderer.h/.cpp`、`VisualRenderer.h/.cpp`。
  - 修改 `src/main.cpp`、`CMakeLists.txt`。
- 验证：
  - MSBuild Debug x64 编译 0 错误、0 警告。
  - 默认 5 秒运行退出码 `exit=0`。
  - 临时开启编辑器自动显示运行 5 秒退出码 `exit=0`（已回退）。

### Task 20：测试、打包与 README 完善
- 状态：已完成并已推送（commit `5ccf44b`，已推送到 `feat/implementation`）。
- 主要内容：
  1. 移除 `src/main.cpp` 中无条件 5s 自动关闭计时器；新增 `--smoke-test` CLI 参数，2 秒自动退出用于 CI/本地验证。
  2. `CMakeLists.txt` 追加 CPack ZIP 配置；`cpack -G ZIP -C Release` 生成绿色包 `MingCKeyOverlay-0.1.0-win64.zip`。
  3. 新增 `scripts/build-and-test.bat`（Debug + Release 双配置构建 + 冒烟测试）与 `scripts/package-release.bat`（Release 构建 + CPack）。
  4. `README.md` 扩展为完整中文用户指南，覆盖项目简介、功能特性、系统要求、快速开始、热键表、编辑器用法、JSON 配置说明、鸣潮兼容性、构建打包、故障排查。
- 验证：
  - `scripts/build-and-test.bat` 通过：Debug/Release 均 0 错误，`--smoke-test` 退出码 0。
  - `scripts/package-release.bat` 通过，`build/release/MingCKeyOverlay-0.1.0-win64.zip` 生成。
- 代码改动：
  - 修改 `src/main.cpp`、`CMakeLists.txt`、`README.md`。
  - 新增 `scripts/build-and-test.bat`、`scripts/package-release.bat`。

### 单实例检测修复（Task 20 后续）
- 状态：已完成并已推送（commit `4622489`，已推送到 `feat/implementation`）。
- 问题：重复启动 `MingCKeyOverlay.exe` 时，第二个实例因全局热键已被占用而注册失败（`RegisterHotKey error=1409`），导致看起来像“程序没出来”。
- 修复：
  - `src/main.cpp` 在初始化前通过命名互斥量 `MingCKeyOverlay_SingleInstance_Mutex` 检测是否已有实例运行。
  - 若已有实例，通过 `FindWindowW(OverlayWindow::ClassName())` 找到已有窗口并激活到最前，随后退出。
  - 非 `--smoke-test` 模式下弹出提示“程序已在运行，已切换到已有窗口。”
  - `src/overlay/OverlayWindow` 新增静态 `ClassName()`，供外部查找窗口使用。
- 验证：
  - `scripts/build-and-test.bat` 通过。
  - 手动测试：先启动一个实例，再启动第二个实例，第二个实例退出码 0，且仅保留一个进程。
- 代码改动：
  - 修改 `src/main.cpp`、`src/overlay/OverlayWindow.h/.cpp`。

### D2D 工厂线程模式修复（Task 20 后续）
- 状态：已完成并已推送（commit `cd8b22b`，已推送到 `feat/implementation`）。
- 问题：渲染线程与 D2D 资源创建线程不是同一个线程，但 `Direct2DRenderer` 使用 `D2D1_FACTORY_TYPE_SINGLE_THREADED`，跨线程访问 D2D 设备上下文可能导致首帧/所有内容无法显示。
- 修复：将 `D2D1CreateFactory` 的工厂类型改为 `D2D1_FACTORY_TYPE_MULTI_THREADED`。
- 验证：
  - `scripts/build-and-test.bat` 通过。
  - `scripts/package-release.bat` 通过，ZIP 已重新生成。
- 代码改动：
  - 修改 `src/overlay/Direct2DRenderer.cpp`。

## 进行中任务

无。

## 未推送提交

无。

## 待办任务

无。

## 关键命令备忘

- 构建（Debug x64）：
  ```bash
  "/c/Program Files/CMake/bin/cmake.exe" --preset debug
  "/c/Program Files/CMake/bin/cmake.exe" --build build/debug --config Debug
  ```
- 构建并测试：
  ```bash
  export PATH="/c/Program Files/CMake/bin:$PATH"
  scripts/build-and-test.bat
  ```
- 打包：
  ```bash
  scripts/package-release.bat
  ```
- 推送：
  ```bash
  git push
  ```

## 注意事项

- `scripts/*.bat` 依赖 `cmake` 在 PATH 中；若本地未加入 PATH，可先执行 `set PATH=C:\Program Files\CMake\bin;%PATH%` 再运行脚本。
- 编辑器自动显示测试改动已回退，当前 `main.cpp` 为干净状态。
- `--smoke-test` 仅用于 CI/本地验证，正常模式下 overlay 会长期运行直到用户按 `Ctrl+Shift+Q` 退出。
