# 弹幕按键提示程序 — 实现计划

## 需求概述

一个类似弹幕的屏幕覆盖层程序，用于游戏中提示队伍流程的按键顺序。

### 核心需求
- **透明覆盖窗口** — 始终最前、点击穿透、不遮挡游戏操作
- **按键序列显示** — 一行排列的按键方框，像长条形小窗循环播放
- **两种推进模式** — (1) **自动播放模式**（计时推进）(2) **按键检测模式**（检测键盘/鼠标输入自动推进）
- **可编辑背景图** — 每个队伍流程可自定义背景图片
- **独立图形化编辑器** — 单独的编辑器窗口，拖拽式编辑按键序列
- **位置/大小可调** — 拖拽移动、滚轮缩放
- **目标游戏**: 鸣潮 (Wuthering Waves)，支持全屏独占
- **运行环境**: Windows 10/11, C++ 实现

### 流程结构
- "队伍流程" 是一条按顺序编排的按键步骤列表，每个步骤带角色标签
- 同一角色可出现多段，且**各段按键可以不同**（如第一次有大招、第二次没有）
- 每个角色每段约 15-20 步，一个队伍流程可能有 3 个角色多段穿插（总步数可达 50+）
- 一个完整循环 = 队伍流程播放一次（循环播放）
- 连招以**鼠标输入为主体**：鸣潮 PC 默认普攻=鼠标左键、重击=长按左键、闪避=右键/Shift，
  按键步骤必须支持鼠标键

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
| 输入检测 | **GetAsyncKeyState** 轮询（键盘 + 鼠标键） | 反作弊安全，见「鸣潮兼容性」一节 |
| 全局快捷键 | **RegisterHotKey** | 反作弊安全，见「鸣潮兼容性」一节 |
| JSON 解析 | **nlohmann/json** (单头文件) | 现代 C++ JSON 库 |
| 构建工具 | **CMake** + MSVC (`/MT` 静态运行时 + `/utf-8`) | 跨 IDE + exe 无 dll 依赖 |

> ⚠️ **渲染方案注意**：不要使用 `WS_EX_LAYERED` + Direct2D —— Layered 窗口下 D2D 只能走
> `ID2D1DCRenderTarget`（GDI 软件拷贝路径），不是真正的硬件加速，每帧都要把位图拷回 GDI。
> 正确做法是 DComp 交换链 + `WS_EX_NOREDIRECTIONBITMAP`（Win8+，Win10/11 完全可用）。

> ⚠️ **交换链效果**：必须使用 `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL`（而非默认的 `FLIP_DISCARD`）。
> `FLIP_DISCARD` 与 Present1 脏矩形不兼容，且 2 buffer 下被 DWM 锁帧。同时须添加
> `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` —— Waitable Object 保证
> 输入采样恰在渲染前一刻进行，对 120Hz 轮询架构至关重要。

> ⚠️ **D3D11 设备标志**：创建共享 D3D11 设备时**必须**指定
> `D3D11_CREATE_DEVICE_BGRA_SUPPORT`，否则 `ID2D1Factory::CreateDxgiSurfaceRenderTarget`
> 返回 `E_INVALIDARG`。D2D 与 ImGui 共用同一设备，此标志不可省略。

**字符编码约定**：全项目统一 UTF-8 —— 源文件 UTF-8 + CMake `add_compile_options(/utf-8)`；
内部字符串一律 UTF-8；调用 DWrite 等宽字符 API 前经 `utils/TextEncoding` 的
`utf8_to_wstring` 转换；文件路径一律用 `std::filesystem::path`（wstring）构造。

**部署形态**：exe + 同级 `profiles/`、`assets/` 目录的绿色包。`/MT` + 静态第三方库使 exe
无 dll 依赖；配置与图片为外置数据。所有相对路径一律以 **exe 所在目录**为基准解析
（`GetModuleFileNameW` 取目录，禁止依赖进程 CWD）。CMake 加 install/post-build 步骤
把 `profiles/` 与 `assets/` 拷到输出目录。

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
│  │  - 移动模式拖拽/缩放   │   │  - 设为当前播放        │ │
│  └──────────┬───────────┘   └───────────┬────────────┘ │
│             │                            │              │
│             └──────────┬─────────────────┘              │
│                        │                                │
│           ┌────────────▼─────────────┐                  │
│           │    Core Engine           │                  │
│           │  - 配置管理(JSON)        │                  │
│           │  - GetAsyncKeyState 检测  │                  │
│           │  - 前台门控              │                  │
│           │  - 自动播放/按键检测推进  │                  │
│           │  - 快捷键系统            │                  │
│           │  - 播放状态机            │                  │
│           └──────────────────────────┘                  │
└────────────────────────────────────────────────────────┘
```

两窗口同进程共享 Core Engine；编辑器与 overlay **共享同一 D3D11 设备**（imgui 后端以
外部设备初始化），背景图 WIC 解码结果一次解码两侧复用。编辑器保存经 Core Engine
**进程内直接生效**并写盘，不经文件热加载。

> ⚠️ **共享设备渲染隔离**：D2D 与 ImGui 共享 D3D11 设备时，`ImGui_ImplDX11` 在
> `Render()` 后仅恢复部分管线状态（光栅化/混合/深度模板子集），输入装配/流输出/
> 计算着色器等未定义。D2D 的 `EndDraw()` 也修改管线状态。为避免状态冲突，两者不
> 直接渲染到同一 backbuffer：overlay 的 D2D 内容渲染到**中间 `ID3D11Texture2D`**
>（`D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`），创建
> `ID2D1Bitmap` 与 `ID3D11ShaderResourceView` 指向同一纹理；ImGui 通过
> `ImGui::Image(/*SRV作为ImTextureID*/)` 将其作为子画面嵌入编辑器窗口。
> 同步用 `D3D11_QUERY_EVENT`：D2D 完成后 `End(pQuery)`，`GetData(...)==S_OK`
> 后 ImGui 再读取纹理——不用 keyed mutex。

**线程安全模型**：编辑器（UI/主线程）保存配置时直接更新内存中的 `ConfigManager` 数据
并写盘；而 `PlaybackEngine` 的输入轮询线程（独立线程）需读取当前步骤序列。两者通过
`std::shared_mutex` 保护配置数据——读取侧获取共享锁，写入侧获取独占锁；
`ConfigManager::GetActiveRotation()` 返回 `std::shared_ptr<const TeamRotation>`（写时
copy-swap），轮询线程仅在步骤边界持有共享锁，不阻塞渲染帧。编辑器保存期间若有步骤正
在检测，等待当前步骤完成后再 swap。编辑器与 overlay 间的通信不依赖文件热加载，直接
通过 Core Engine 接口同步调用。

> ⚠️ **`atomic<shared_ptr>` 方案**：`std::atomic<std::shared_ptr<T>>` 读取比
> `shared_mutex` 快 10-40×，但 MSVC 上可能退化为内部互斥锁（`is_lock_free() == false`），
> 需在目标工具链上 benchmark 确认。选择依据：lock-free → 使用 `atomic<shared_ptr>` 配合
> `memory_order_acquire/release`（不用 `seq_cst`）；非 lock-free → 保留 `shared_mutex` 但
> 改用 two-phase optimistic 模式（共享锁 → 快照 → 释放 → 独享锁 → 验证版本 → swap）。

> ⚠️ **内存序（Memory Ordering）**：跨线程原子变量一律使用 `memory_order_acquire`（读取）
> 与 `memory_order_release`（写入），不默认 `seq_cst`。`seq_cst` 在 x86 上发射 `MFENCE`
> （~30-80 周期），比 acquire/release 慢 2-3×；问题在 x86 上不会显现（x86 保证 acquire 语义），
> 但 ARM 支持（ARM64EC/未来架构）会暴露错误。在注释中注明所选的 memory order。

> ⚠️ **线程局部缓存（Thread-Local Cache）**：每次 `atomic<shared_ptr>::load()` 都会触发
> `InterlockedIncrement` 控制块引用计数，导致所有核心的缓存行失效。在线程局部缓存一份
> 指针，比较控制块地址后复用，可消除 99%+ 的引用计数操作。
> ```cpp
> thread_local std::shared_ptr<const Config> tls_cache;
> auto* current = config_atomic_.load(std::memory_order_acquire).get();
> if (tls_cache.get() != current)
>     tls_cache = config_atomic_.load(std::memory_order_acquire);
> // 使用 tls_cache
> ```

> ⚠️ **False Sharing 防护**：不同线程访问的原子变量（如 `step_index_` 与 `config_ptr_`）
> 若在同一缓存行（64 字节），写入方使读取方的缓存行失效，造成 5-10× 性能退化。
> 每个跨线程原子变量以 `alignas(64)` 对齐到独立缓存行：
> ```cpp
> alignas(64) std::atomic<uint64_t> step_index_{0};
> alignas(64) std::atomic<std::shared_ptr<const Config>> config_ptr_{};
> ```

---

## 数据结构

采用**扁平化单序列**模型：每个按键步骤自带角色标签，角色段只是显示分组。
（若用"角色顺序数组 + 角色名→序列的 map"，同一角色多次上场时各段只能共享一份按键，
无法表达"第二次上场打不同连招"的实际排轴。）

```cpp
// 队伍流程：一条有序的按键步骤列表
struct TeamRotation {
    std::string name;               // "常规循环·散秧维"
    std::string background_image;   // 背景图路径（相对 exe 目录）
    std::vector<KeyStep> steps;     // 完整按键序列（角色段 = 连续相同 character 的分组）
};

// 单个按键步骤
struct KeyStep {
    std::string character;    // "散华" — 角色标签，用于分段显示
    std::string key;          // "Q", "E", "Shift+1", "Space", "LButton", "RButton"
    std::string skill_name;   // "冰棱散射", "普攻·三段", "重击·长按"
    int duration_ms;          // 自动播放模式停留时间(ms)
};
```

**key 命名约定**：键盘键用 VK 名（`Q`、`Space`、`F1`），组合键用 `+` 连接
（`Shift+1`），鼠标键用 `LButton` / `RButton` / `MButton`。`GetAsyncKeyState` 对鼠标
VK 码与键盘同构，KeyDetector 边沿检测逻辑原样复用，无需额外 device 字段。
**修饰键 VK 常量**：`Shift` → `VK_SHIFT`（不区分左右）、`Ctrl` → `VK_CONTROL`、
`Alt` → `VK_MENU`。组合键解析时从字符串提取修饰键名并映射到对应 VK。
**长按不建 schema 字段**：本程序是提示器而非验证器，按下沿即推进，tap/hold 语义用
`skill_name` 文本表达（如 "重击·长按"）。

编辑器中按连续相同 `character` 折叠显示为段：
`[散华 ×18] → [秧秧 ×15] → [维里奈 ×20] → [散华 ×10]`，段可整体拖拽排序。

---

## 详细设计

### 1. Overlay 窗口 — OverlayWindow

**窗口样式**:
- 主样式 `WS_POPUP`；扩展样式
  `WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`
  （`NOACTIVATE` **始终保留**，移动模式下点击也不抢游戏焦点；`TRANSPARENT` 在移动模式下
  临时移除，退出后恢复——两者互不冲突，各管各的行为）
- 渲染目标：DirectComposition visual 绑定 DXGI 交换链（premultiplied alpha），
  D2D1.1 DeviceContext 直接画到交换链 backbuffer
- **DPI 感知**：manifest 声明 Per-Monitor V2，避免高缩放屏上坐标/字号模糊。
  处理 `WM_DPICHANGED`：按建议矩形 `SetWindowPos` + `ID2D1DeviceContext::SetDpi` +
  重建 DWrite 文本格式。配置坐标采用**虚拟屏幕坐标**（多显示器可为负值），
  启动时校验窗口落在任一显示器工作区内，否则回退默认位置
  > ⚠️ **预缓存字体图集**：在 `WM_DPICHANGED` 中同步重建 DWrite 文本格式耗时可接受，
  > 但 ImGui 字体图集重建（含 CJK 字形）会造成多帧卡顿（100ms+）。
  > 方案：启动时按 100%/125%/150%/200% 预构建多份字体图集缓存（以 `dpi_scale * 100` 为 key），
  > DPI 变更时直接切换——`io.Fonts = cached_atlas[new_dpi]` + 重建 ImGui 纹理，延迟 <1ms。
  > ImGui 样式缩放：缓存 100% 基准样式，每次变更时 `style.ScaleAllSizes(dpiScale)` 重新应用，
  > 防止累积舍入误差。
  > **多显示器追踪**：窗口跨显示器移动时 `MonitorFromWindow` + 缓存每 `HMONITOR` 的 DPI，
  > `WM_MOVING`/`WM_MOVE` 中重新评估当前显示器的 DPI 变化。
- **尺寸变更**：`OnSize` 时按三步骤释放-重建序列处理——
  (1) `ID2D1DeviceContext::SetTarget(nullptr)` 释放目标位图；
  (2) `IDXGISwapChain1::ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)`；
  (3) `GetBuffer(0)` 取新 backbuffer → `CreateBitmapFromDxgiSurface` 重建 D2D 目标位图 →
  `SetTarget`。**绝不缓存 `ID3D11RenderTargetView` 指针**（ResizeBuffers 后失效）。
  `ResizeBuffers` 返回 `DXGI_ERROR_DEVICE_REMOVED/RESET` 时走设备丢失恢复路径

  > ⚠️ **设备丢失恢复（必须实现）**：长时间运行（游戏数小时会话）+ 驱动超时/重置下，
  > D3D 设备丢失是常态。DComp 设备丢失后**无法**关联新 DXGI 设备，必须全链重建。
  > 在 `WM_PAINT` 中调用 `IDCompositionDevice::CheckDeviceState` 检测丢失信号。
  > 恢复流程：释放所有 D2D/D3D/DComp 资源 → 重建 D3D11 设备（保留原始标志）→ 重建
  > DXGI 交换链 → 重建 D2D 工厂/设备/上下文/目标位图 → 重建 DComp 设备/visual →
  > 设置 visual 内容并 Commit → 重放 visual 树参数（offset、clip）。另需：
  > `ImGui_ImplDX11_InvalidateDeviceObjects()` + `CreateDeviceObjects()` 重置 ImGui。
  > 建议封装为 RAII 工厂或状态结构体以便原子回放。

**交互方式**:

> ⚠️ 穿透模式（`WS_EX_TRANSPARENT`）下窗口**收不到任何鼠标消息**，
> 因此拖拽/滚轮必须先进入"移动模式"才能生效。

| 操作 | 功能 |
|------|------|
| **Ctrl+Shift+M** | 切换移动模式（临时移除 `WS_EX_TRANSPARENT`，由 D2D 自绘高亮边框提示） |
| **移动模式下左键拖拽** | 移动窗口位置 |
| **移动模式下滚轮** | 缩放（改写 `display.scale`，整体等比缩放） |
| **ESC**（移动模式下） | 退出移动模式（防呆；ESC 为 `WM_KEYDOWN` 窗口消息处理，非 `RegisterHotKey`） |
| **Ctrl+Shift+H** | 切换显示/隐藏 |
| **Ctrl+Shift+Space** | 播放/暂停（聊天、暂离时先暂停，防止误推进） |
| **Ctrl+Shift+P** | 切换推进模式（自动播放/按键检测） |
| **Ctrl+Shift+N** | 切换当前播放的队伍流程（循环切换） |
| **Ctrl+Shift+E** | 打开编辑器窗口 |
| **Ctrl+Shift+R** | 重载配置 |
| **Ctrl+Shift+Q** | 退出程序 |

所有快捷键用 `RegisterHotKey` 注册，配置字符串由工具函数解析为修饰键 + VK 码；
**完整快捷键以配置文件 `hotkeys` 节为准**。注意 RegisterHotKey 命中后该按键组合被系统
消费、游戏收不到此次按键——因此默认全部使用 `Ctrl+Shift` 前缀（鸣潮默认键位不用 Ctrl），
且不应绑定游戏常用键。注册失败（组合被其他程序占用）时逐个记录日志并在编辑器设置页
标红提示改键，其余热键不受影响。

**布局与缩放**：窗口尺寸为派生值，不写死——
`width = (before + after + 1) × (key_size + spacing) × scale`，
`height = key_size × scale + 进度条与边距`。滚轮只改 `display.scale`，
key_size 等基准值不随缩放变化。

> ⚠️ **缩放锚定**：滚轮缩放时以**当前鼠标位置**为锚点固定内容不偏移——缩放后重算窗口
> 左上角位置使鼠标所在图形位置不变（`new_pos = mouse_pos - (mouse_pos - old_pos) ×
> (new_scale / old_scale)`）。此策略在 `WM_MOUSEWHEEL` 处理中实现，与拖拽移动正交。

**渲染 (Direct2D)**:
- 背景图渲染（全窗口背景；等比 **cover 裁剪** + `HIGH_QUALITY_CUBIC` 插值，
  resize 时仅调整绘制矩形、不重新解码）
- 无背景图时的默认外观：**半透明深色圆角底板**，保证按键在任何游戏画面上可读；
  `opacity` 作用于背景底板 alpha，按键本体始终不透明
- **可视窗口滚动**：总步数可能 50+，一行显示不下。以当前步骤为基准显示
  `[-3, +6]` 共 10 个键位（当前步骤位于可视窗口第 4 位，偏向预览后续按键），随推进平滑滚动
- 按键方块渲染（圆角矩形 + 阴影）
- 按键文字渲染（键名 + 技能名 + 角色分段色标）
- 状态指示（当前按键高亮发光，已完成变暗，未到灰色）
- 错误输入反馈（当前键红色闪烁，可关）
- 进度条/计数器显示当前进度
- **按需渲染**：脏标记驱动——状态变化（推进、拖拽、滚动动画进行中）才渲染并 Present，
  静止时 0 Present，比固定 30fps 更省 GPU 且动画更顺滑；自动播放模式的进度条为连续动画，
  播放期间照常逐帧渲染

> ⚠️ **DComp Commit 批处理**：`IDCompositionDevice::Commit()` 是异步调用，由 DWM
> VSync 时钟门控。每帧 Commit 一次且 visual 树未变动时，Commit 仍会触发 DWM 序列化，
> 浪费约 0.03W 功耗（Mozilla profiles 显示 Commit 在高频下成为瓶颈）。
> 跟踪 `bool m_visualTreeDirty` —— 只在 SetOffset/SetClip/AddVisual/RemoveVisual
> 后 Commit；无变化时不 Commit。

> ⚠️ **隐藏时 RemoveVisual，不 SetOpacity(0)**：`SetOpacity(0)` 或零尺寸 clip 的
> visual 仍由 DWM 处理（合成阶段跳过但处理开销仍在）。正确做法是
> `RemoveVisual`/`RemoveAllVisuals`——DWM 完全不参与。恢复时在同一 Commit 批次中
> `AddVisual`，实现原子性切换。优先使用单一根 visual + swap chain 内容；多层 overlay
> 通过 D2D 渲染合并到同一 swap chain，避免 DComp 多 visual 树的开销。

> ⚠️ **脏矩形渲染 + PushAxisAlignedClip**：`BeginDraw()` 后调用
> `PushAxisAlignedClip(&dirtyRect, D2D1_ANTIALIAS_MODE_ALIASED)` 将渲染限制在变化
> 区域，只清空/重绘 clip 范围内的内容（而非全窗口）。clip 矩形需与 Present1 的
> `DXGI_PRESENT_PARAMETERS` 脏矩形匹配，DWM 据此做 Panel Self Refresh (PSR) 优化。
> `PopAxisAlignedClip()` 在 `EndDraw()` 前调用。

### 2. 编辑器窗口 — EditorWindow (imgui)

**中文字体**：imgui 默认字体仅 ASCII，集成时用 `AddFontFromFileTTF` 加载系统
`C:\Windows\Fonts\msyh.ttc`（TTC 需指定 face index：`ImFontConfig.FontNo`）。

> 🔴 **关键：不要用 `GetGlyphRangesChineseFull()`**——该函数栅格化约 60,000+ CJK 字形，
> 超出 GPU 最大纹理尺寸（16384），消耗 >200MB VRAM，纹理创建静默失败导致字形缺失。
> 
> 正确做法——`ImFontGlyphRangesBuilder` 只收集 UI 实际用到的字符：
> ```cpp
> ImFontGlyphRangesBuilder builder;
> builder.AddText(ui_strings);    // 只添加实际 UI 文本中的字符
> builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
> builder.BuildRanges(&custom_ranges);
> io.Fonts->AddFontFromFileTTF("msyh.ttc", font_size, &config, custom_ranges.Data);
> ```
> 附加优化：`OversampleH = OversampleV = 1`（纹理尺寸缩小 4 倍）；
> `io.Fonts->TexDesiredWidth = 2048` + `ImFontAtlasFlags_NoPowerOfTwoHeight`。
> 始终检查返回的 `ImFont*` 非空。

> 若以单 exe 分发，可将常用中文字体子集按此 builder 方式嵌入 `.rc` 资源，
> 用 `AddFontFromMemoryTTF` 加载，避免依赖系统 msyh.ttc。

**编辑器窗口位置**：首次打开时在 overlay 窗口正上方偏移居中显示（避免完全重叠）；
之后记住上次关闭时的窗口位置（通过 `imgui.ini` 的 `SetNextWindowPos`/`SaveIniSettingsToDisk`）；
多显示器下确保创建在 overlay 所在的显示器工作区内。

**a) 队伍流程管理**
- 新建/载入/保存队伍流程（存储模型为单文件多流程，见「配置文件」；
  `profiles/` 目录当前仅 `default.json` 单文件，目录为预留）
- 流程列表（所有保存的队伍流程），**「设为当前播放」**（写回 `active_rotation`）
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
- 支持特殊键与组合键（Ctrl, Shift, Space, F1-F12, Shift+1 等）与
  **鼠标键**（按键录入支持「捕获下一次键盘按键或鼠标点击」）
  > ⚠️ **捕获实现**：键盘捕获通过编辑器窗口 `WM_KEYDOWN` 消息处理实现（编辑器有焦点时）
  > 结合修饰键状态（`GetKeyState`）组合成完整键名字符串。**鼠标点击捕获**因反作弊约束
  > 不使用全局钩子——编辑器提供鼠标键下拉选择（`LButton`/`RButton`/`MButton`/`XButton1`/
  > `XButton2`）替代实时捕获，用户点击"录入"按钮后手工选择或点按编辑器窗口内的按钮触发。
  > 组合键中的 `+` 号两侧修饰键在键盘捕获时自动检测 `Ctrl`/`Shift`/`Alt` 状态并填入。
- **撤销/重做**（MVP 后增强）：`EditHistory` 栈记录每次编辑操作（添加/删除/拖拽排序/改值），
  `Ctrl+Z` 撤销、`Ctrl+Y` 重做；编辑器关闭时清空历史，不跨会话持久化
- **大列表优化**：50+ 步的按键列表配合 `ImGuiListClipper` 使用——
  ```cpp
  clipper.Begin(steps.size(), ImGui::GetTextLineHeightWithSpacing());
  while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
          ImGui::PushID(i);  // 用整数 PushID 避免字符串操作
          // 渲染第 i 行
          ImGui::PopID();
      }
  }
  ```
  不带 clipper 时 50 行 ImGuiTable 即降帧至 <30 FPS。
  `PushID(i)` 使用整数快速哈希路径，比 `PushID(nameStr)` 的 `strlen`+ 哈希更快。
- **拖拽排序实现模式**：使用非 ImGui 保留前缀的 payload type（如 `"STEP_DND"`），
  内含 `sizeof(size_t)` 源索引。在 `AcceptDragDropPayload()->IsDelivery()` 中**立即**
  用 `std::rotate` 重排容器（不推迟到帧尾，否则 clipper 的 display index 偏移导致越界）：
  ```cpp
  auto item = std::move(vec[src]);
  if (src < dst)
      std::move(vec.begin()+src+1, vec.begin()+dst+1, vec.begin()+src);
  else
      std::move(vec.begin()+dst, vec.begin()+src, vec.begin()+dst+1);
  vec[dst] = std::move(item);
  ```
  跨段拖拽（角色段间移动步骤）需先调整源/目标段容器，再做 `std::move`。

**d) 背景图管理**
- 背景图预览（WIC 解码到 CPU 位图即可预览，与 overlay 共享解码结果）
- 从本地选择图片（PNG/JPG）；选图后**复制到 exe 目录 `assets/` 下**，
  JSON 中只存相对路径
- 每个队伍流程独立绑定背景图

**e) 设置**
- 窗口默认位置/缩放
- 默认推进模式
- 快捷键配置（注册失败的项标红提示）
- 按键方块样式（颜色、圆角大小、间距等）

### 3. 播放引擎 — PlaybackEngine

**线程模型**：输入轮询运行在**独立线程**（`timeBeginPeriod(1)` + 高分辨率可等待
定时器，或 `sleep_until` 循环），与渲染解耦；轮询结果经原子变量交给 PlaybackEngine。
注意默认 `WM_TIMER` 约 15.6ms 粒度，达不到 120Hz，不可用。

> ⚠️ **定时器实现**：纯 `sleep_until` 退化为 `Sleep()` ~1ms 精度 + 累积漂移，8.33ms
> 间隔下抖动可达 ±1-15ms。**Win10 1803+**：使用 `CreateWaitableTimerExW` 带标志
> `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`（值 0x2）+ `SetWaitableTimer` 绝对到期时间，
> `WaitForSingleObject` 等待——线程完全挂起，零 CPU 开销。**旧版回退**：混合模式——
> `Sleep(1)` 等待大部分间隔（>2ms 剩余时），最后 1-2ms 用 `QueryPerformanceCounter` spin
>（`_mm_pause()` 在 spin 循环内提示 CPU 降低功耗）。采用**绝对时间参考**防止漂移：
> `target_time = start + tick * interval`，不使用相对 `sleep_for` 累积误差。

> ⚠️ **线程优先级与定时器合并**：默认 `THREAD_PRIORITY_NORMAL` 下调度器可能合并定时器中断，
> 造成 5-15ms 过睡。轮询线程设为 `THREAD_PRIORITY_TIME_CRITICAL`（15）+ 禁用定时器合并：
> ```cpp
> SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
> DWORD no_coalescing = 2; // TIMER_SET_NO_COALESCING
> SetThreadInformation(hThread, ThreadTimerMode, &no_coalescing, sizeof(DWORD));
> ```
> Spin 限制在 1-2ms（120Hz 下约 18% 单核），不纯 spin 全间隔。
> `timeBeginPeriod(1)` / `timeEndPeriod(1)` 用 RAII 类在 WinMain 入口/出口配对。

> ⚠️ **`atomic::wait()` 空转优化**：overlay 隐藏或无游戏前台时，轮询线程不需 120Hz 运行。
> 用 C++20 `atomic::wait()` 让线程完全挂起——Windows 上映射为 `WaitOnAddress`，零 CPU：
> ```cpp
> // 写入方（前台检测/显隐切换处）
> ready_.store(true, std::memory_order_release);
> ready_.notify_one();
> 
> // 轮询线程
> while (!ready_.load(std::memory_order_acquire))
>     ready_.wait(false, std::memory_order_acquire);
> ```
> 实现三级空闲策略：无游戏前台 = 完全挂起（原子等待）；可见 + 空闲 = 定时驱动 120Hz；
> 可见 + 活跃 = 完整帧同步。

**模式A: 自动播放模式**
- 每个按键步骤有 `duration_ms`
- 计时器到时自动推进
- 显示进度条/倒计时

**模式B: 按键检测模式**
- `GetKeyboardState` 批量轮询（一次 syscall 读取全部 256 字节键盘状态表），
  替代逐个调用 `GetAsyncKeyState`（254 次 syscall/帧），频率约 **120Hz**
- **边沿检测**：维护 `bool prevKeyState[256]` 数组记录上一帧状态，每帧比较当前
  与上一帧的按下/抬起状态做上升沿/下降沿检测。**不依赖 `GetAsyncKeyState` 返回值的低位**
  （bit 0，MSDN 已标明不可靠）。对于组合键步骤，主键上升沿 + 修饰键当前按下。
  ```cpp
  BYTE keys[256];
  GetKeyboardState(keys);
  for (每个跟踪的键) {
      bool down = (keys[vk] & 0x80) != 0;
      if (down && !prevKeys[vk]) /* 上升沿 */;
      prevKeys[vk] = down;
  }
  ```
- **前台门控**：轮询前用 `GetForegroundWindow` 比对目标进程（配置
  `input.foreground_only` / `target_process`），游戏非前台时暂停检测，
  避免 Alt-Tab 打字误推进；游戏内聊天框场景前台仍是游戏、窗口判定无法覆盖，
  用 **Ctrl+Shift+Space 暂停**兜底
  > ⚠️ **前台检测优化**：不在每轮 120Hz 循环中调用 `GetForegroundWindow`
  >（不必要的内核态切换）。窗口句柄缓存 + 仅在检测到输入事件时刷新，或降频至
  > 每 10 轮（~12 次/秒）。另注册 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 回调，
  > 前台切换瞬间触发更新（事件驱动 + 轮询备胎，取更快者）。
- **边沿检测**：只在按键"按下瞬间"（上一帧抬起、当前帧按下）推进，
  避免按住一个键连跳多步
- **组合键处理**：组合键步骤 = 主键按下沿 + 指定修饰键均处于按下；
  **普通键步骤忽略无关修饰键**（战斗中常按住 Shift/WASD，不因此误判）
- **「按错键」集合过滤**：加载流程时预计算**所有单键**（组合键拆出主键）的并集作为
  `valid_keys` 集合。对于组合键步骤（如 `Shift+1`），集合中包含 `1` 而非 `Shift+1` 整体。
  检测逻辑：按下 `1` → `1` ∈ `valid_keys` → 期望键是 `Shift+1` 且 `Shift` 未按下 →
  触发错误红闪（`wrong_key_flash`）。普通移动键（`W`/`A`/`S`/`D` 等）不在集合中则静默忽略。
  红色闪烁反馈（`wrong_key_flash`）；集合外按键（移动、视角等）静默忽略，不会全程误闪
- 检测到正确按键后自动推进到下一步
- 可配置超时自动跳过（`timeout_skip_ms`：**0 = 禁用**，>0 = 超时毫秒数，解析时非负校验）

**状态机**（转换触发源：热键 / 计时器 / 编辑器）:
```
IDLE → PLAYING ⇄ PAUSED（Ctrl+Shift+Space）
PLAYING ──播完末步──┬─ loop=true  → 回到第 1 步继续 PLAYING（循环播放）
                    └─ loop=false → FINISHED → IDLE
PLAYING → EDITING（Ctrl+Shift+E 打开编辑器时暂停）→ 关闭编辑器回到 PLAYING
```
进入 EDITING 时**注销所有 overlay 全局热键**（防止编辑器内操作与 `RegisterHotKey` 冲突）；
关闭编辑器后重新注册。编辑器窗口自身的快捷键（`Ctrl+S` 保存等）在 imgui 层处理，不依赖
系统热键。
模式B 播完末步同样按 `loop` 规则回卷。重载配置后按 `active_rotation` 恢复当前播放流程。

### 4. 配置文件 (JSON)

存储模型：**单文件多流程** —— 全部流程在 `team_rotations` 数组，`settings` 全局一份；
`active_rotation` 指向当前播放流程。文件按 UTF-8 读写。

```json
{
  "active_rotation": "常规循环·散秧维",
  "team_rotations": [
    {
      "name": "常规循环·散秧维",
      "background": "assets/bg_01.png",
      "steps": [
        { "character": "散华", "key": "LButton", "skill_name": "普攻·三段", "duration_ms": 1200 },
        { "character": "散华", "key": "LButton", "skill_name": "重击·长按", "duration_ms": 1500 },
        { "character": "散华", "key": "E", "skill_name": "冰棱散射", "duration_ms": 2000 },
        { "character": "秧秧", "key": "Q", "skill_name": "...", "duration_ms": 1800 }
      ]
    }
  ],
  "settings": {
    "display": {
      "opacity": 0.85,
      "scale": 1.0,
      "position": { "x": 100, "y": 100 },
      "visible_keys": { "before": 3, "after": 6 },
      "key_style": {
        "key_size": 50,
        "spacing": 10,
        "border_radius": 8,
        "active_color": "#FFD700",
        "font_size": 16
      },
      "mode": "auto",
      "loop": true,
      "show_progress": true
    },
    "hotkeys": {
      "toggle_visibility": "Ctrl+Shift+H",
      "play_pause": "Ctrl+Shift+Space",
      "toggle_mode": "Ctrl+Shift+P",
      "next_rotation": "Ctrl+Shift+N",
      "open_editor": "Ctrl+Shift+E",
      "move_mode": "Ctrl+Shift+M",
      "reload_config": "Ctrl+Shift+R",
      "quit": "Ctrl+Shift+Q"
    },
    "input": {
      "poll_hz": 120,
      "foreground_only": true,
      "target_process": "Client-Win64-Shipping.exe",
      "wrong_key_flash": true,
      "timeout_skip_ms": 0
    }
  }
}
```

> ⚠️ **原子写入模式**：配置写盘使用临时文件 + 重命名模式防止断电/崩溃损坏 JSON：
> (1) 在**同目录**（同卷）创建临时文件 `default.json.tmp`；
> (2) 序列化写入 tmp 文件 → `FlushFileBuffers` 刷新到磁盘；
> (3) `MoveFileExW(tmpPath, configPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`。
> 不要用 `ReplaceFileW`（持有目标文件句柄时不可用）。不直接写原文件，避免写入中
> 崩溃留下半截 JSON 触发热加载。

> ⚠️ **字段访问约定**：统一使用 `json::at(key)` 而非 `operator[]` —— `operator[]`
> 对缺失键静默插入 `null`，掩盖配置错误。`.at()` 抛出 `nlohmann::json::out_of_range`
> / `type_error` / `parse_error`，需分类捕获并记录具体字段名。
> 热路径可用预分配的 `static const json::json_pointer` 跳过每轮字符串构造：
> ```cpp
> static const json::json_pointer JP_POLL_HZ = "/input/poll_hz";
> auto hz = config.at(JP_POLL_HZ).get<int>();
> ```
> 可选字段用 `.value(key, default)` 代替 `.at()`+try-catch，避免异常开销。

### 5. 错误处理与日志

- **`duration_ms` 校验**：配置加载时对每个 `KeyStep` 的 `duration_ms` 做 clamp，
  最小值为 100ms（防止 0 值导致自动播放模式卡死/无限循环），最大值不限（长时间停留合理）
- **`profiles/` 目录创建**：程序启动时确保 `profiles/` 和 `assets/` 目录存在
  （`std::filesystem::create_directories`），默认配置写出前先创建目录
- **JSON 解析失败** → 保留内存中上一份有效配置（首启则为内置默认），日志记录并提示；
  热加载场景尤其重要，损坏的写盘不会冲掉正在播放的配置
- **图片缺失/解码失败** → 回退默认深色底板，日志记录
- **热键注册失败** → 逐个报错（日志 + 编辑器设置页标红），其余热键不受影响
- **日志**：`utils/Logger` 写 exe 同级 `overlay.log`（同时 OutputDebugString），至少记录
  启动路径解析、配置加载结果、热键注册成功/失败、图片加载结果、DComp/D2D 设备创建结果。
  阶段 1 实测时凭日志即可判断 overlay 是渲染失败还是被系统/反作弊拦截

---

## 鸣潮兼容性注意事项

1. **全屏独占**: 真独占（未开全屏优化）下 DWM 不参与合成，**非注入式** overlay
   无法合成显示（注入式 overlay 如 Steam/Discord 除外，但注入有反作弊风险，不采用）。
   是否真独占取决于游戏的全屏优化状态，**以实测为准**。**推荐方案：无边框窗口模式**，
   在 README/首次运行提示中说明。
2. **反作弊安全**（鸣潮使用 ACE 反作弊）:
   - ✅ `GetAsyncKeyState` 安全（只读按键状态）
   - ✅ `RegisterHotKey` 安全（系统注册，非钩子）
   - ❌ **不要用** `SetWindowsHookEx(WH_KEYBOARD_LL)`（全局钩子可能触发反作弊）
   - ❌ **不要** 做任何内存操作
3. **尽早实测（go/no-go 决策点）**: 叠加层一般被放行，但存在不确定性。**阶段 1 的
   空壳 overlay 就要与鸣潮同时运行实测**，尽早暴露兼容性风险。这是项目的 go/no-go
   关口：**若 overlay 被拦截，转向副屏/第二设备显示方案或终止项目，不再投入后续阶段**。
4. **热键吞键**: RegisterHotKey 命中后该组合被系统消费、游戏收不到；默认热键全部
   `Ctrl+Shift` 前缀以避开鸣潮默认键位，README 中需注明此行为。
5. **性能**: 按需渲染静止时零 GPU 占用，不影响游戏

---

## 文件结构

代码位于本仓库（MingC/）根目录：

```
MingC/
├── CMakeLists.txt                  // 含 /utf-8、/MT；install 拷贝 profiles/assets
│                                   // （第三方库须统一 /MT —— imgui 源码编译/FetchContent
│                                   // 无问题；若 nlohmann/json 是单头文件则不受运行时影响）
├── README.md                       // 无边框模式说明、热键吞键说明、submodule 初始化命令
├── app.manifest                    // Per-Monitor V2 DPI 感知
├── src/
│   ├── main.cpp                    // 入口
│   ├── overlay/
│   │   ├── OverlayWindow.h/.cpp    // 透明覆盖窗口
│   │   └── Direct2DRenderer.h/.cpp // D2D + DComp 交换链渲染器
│   ├── editor/
│   │   ├── EditorWindow.h/.cpp     // ImGui 编辑器 (Win32+DX11 后端， 共享 D3D11 设备)
│   │   └── EditorComponents.h/.cpp // 编辑器子组件
│   ├── core/
│   │   ├── PlaybackEngine.h/.cpp   // 播放引擎（自动播放/按键检测 + 前台门控）
│   │   ├── KeyDetector.h/.cpp      // 输入检测（独立线程轮询；边沿/组合键/鼠标键）
│   │   ├── ConfigManager.h/.cpp    // 配置读写（UTF-8；active_rotation）
│   │   └── TeamRotation.h          // 数据结构
│   └── utils/
│       ├── HotkeyManager.h/.cpp    // RegisterHotKey 封装 + 字符串解析 + 注册失败报错
│       ├── ResourceLoader.h/.cpp   // 图片加载（WIC，CPU 侧副本供重建）
│       ├── TextEncoding.h/.cpp     // UTF-8 ↔ UTF-16 转换
│       └── Logger.h/.cpp           // overlay.log + OutputDebugString
├── profiles/default.json           // 单文件多流程；目录为预留
├── assets/
└── third_party/                    // git submodule 或 FetchContent，锁定版本
    ├── imgui/                      // imgui docking branch（锁定 tag，如 v1.91.x-docking）
    └── nlohmann/                   // json.hpp（v3.x）
```

---

## 开发阶段

> **MVP 基线**：MVP = 阶段 1-3 + 手写 JSON 配置 + Ctrl+Shift+R 手动重载。
> 编辑器（核心需求，可延后不可砍）、热加载、进度条为 **MVP 后增强**；
> 工期紧张时先砍热加载与进度条。

### 阶段 1 — 基础骨架 + 游戏兼容性实测（go/no-go）
- 创建 CMake 项目，配置第三方依赖（锁定版本），添加 Per-Monitor V2 manifest
- OverlayWindow：DComp 交换链透明渲染、置顶、点击穿透、`WS_EX_NOACTIVATE`；
  **D3D11 设备创建须带 `D3D11_CREATE_DEVICE_BGRA_SUPPORT`**；
  交换链用 **`DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` + `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`**；
  三步骤 ResizeBuffers + **设备丢失恢复骨架**（`CheckDeviceState` 检测 +
  全链重建路径预留接口 + `ImGui_ImplDX11_InvalidateDeviceObjects`/`CreateDeviceObjects` 占位）
- D2D 渲染循环 + Logger（设备创建结果落日志）
- README 先行：无边框模式说明
- **验证**: 显示半透明窗口；**与鸣潮同时运行实测**（无边框模式），确认不被反作弊拦截
  —— **go/no-go 决策点**：被拦截则转向副屏方案或终止

### 阶段 2 — 按键渲染
- TeamRotation / KeyStep 扁平数据结构（含鼠标键命名约定）
- 按键方块渲染（圆角矩形 + 文字 + 状态颜色 + 错误红闪）
- 可视窗口滚动（当前步骤位于第 4 位，[-3, +6]）
- 自动播放推进逻辑
- 内置默认示例配置（默认 JSON 通过 CMake `file(READ ...)` 或 `.rc` 资源文件嵌入 exe，
  首启自动写出到 `profiles/`；`profiles/` 目录不存在则先 `create_directories`）
- **验证**: 显示按键方块条，自动循环并平滑滚动

### 阶段 3 — 输入检测
- KeyDetector（独立线程 120Hz 轮询——**`GetKeyboardState` 批量读取** + `bool prevKeyState[256]`
  边沿检测；组合键；**鼠标键**；**前台门控** + `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`）
- 定时器实现：`CreateWaitableTimerExW(HIGH_RESOLUTION)` + 混合 sleep/spin（或旧版回退）；
  `THREAD_PRIORITY_TIME_CRITICAL` + 禁用定时器合并；`atomic::wait()` 空闲挂起
- 两种推进模式切换
- RegisterHotKey 快捷键系统（含移动模式 + 拖拽/缩放、播放暂停、切换流程、退出；
  注册失败报错）+ ESC 防呆退出移动模式
- **验证**: 按键检测模式下按正确键（含鼠标键）推进一次；游戏非前台不推进；
  移动模式可拖拽缩放、不抢游戏焦点；ESC 退出移动模式

### 阶段 4 — 编辑器
- 集成 imgui（Win32+DX11 后端，共享 D3D11 设备；**中文字体用 `ImFontGlyphRangesBuilder`
  收集实际 UI 文本字符，禁用 `GetGlyphRangesChineseFull`**）
- 队伍流程管理（含设为当前播放）+ 角色段编排 + 按键编辑（拖拽排序用非保留 prefix payload
  `"STEP_DND"` + `std::rotate` 即时重排；50+ 列表用 `ImGuiListClipper` + `PushID(i)`）+ 背景图
  （选择/复制入 assets/WIC 加载/预览）
- 配置保存/加载（原子写入模式：tmp 文件 + `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`）；
  字段访问用 `.at()` 而非 `operator[]`
- **验证**: 完整编辑并保存配置（含同角色多段不同按键），overlay 即时生效

### 阶段 5 — 完善
- 背景图 D2D 渲染（cover 裁剪）与默认底板外观
- 窗口缩放/透明度/样式设置
- 进度条显示
- 热加载配置（`ReadDirectoryChangesW` 文件监视，面向外部手改 JSON）
  > ⚠️ 实现方式：`CreateFileW(FILE_LIST_DIRECTORY | FILE_FLAG_OVERLAPPED)` + 关联
  > `CreateIoCompletionPort` (IOCP) + 64KB 通知缓冲区（对齐 4KB）。每次
  > `GetQueuedCompletionStatus` 后立即重新投递 overlapped 请求。防抖机制：每文件
  > 缓存 (path, size, lastWriteTime)，收到通知后重置 200-500ms 定时器，静默期满后才处理。
  > 写入完成检测：`CreateFileW(GENERIC_READ | FILE_SHARE_READ)` 重试最多 200ms
  >（`ERROR_SHARING_VIOLATION` 时 `Sleep(10)` 回退），确保文件写入完毕。
  > 编辑器保存写盘期间**临时关闭文件监视**，写完后重新开启，防止保存动作触发热加载
  > 竞争条件（编辑器已进程内直接生效，无需再走热加载路径）；监视恢复后通过生成计数器
  > 忽略自己写盘触发的 `FILE_NOTIFY_CHANGE_LAST_WRITE` 事件。

### 阶段 6 — 测试打包
- Win10/Win11 测试、高 DPI/多显示器测试（多级缩放间切换验证预缓存图集切换无卡顿）
- 与鸣潮完整流程联测
- `/MT` 静态运行时打包：exe + 同级 `profiles/`、`assets/`、README 的绿色包

---

## 验证流程

以下为各阶段验证项的**汇总终验清单**：

1. **编译**: `cmake --build build` 零错误
2. **启动**: 显示透明 overlay 窗口在屏幕角落（高 DPI 下无模糊）
3. **默认配置**: 显示示例按键序列，可视窗口正确滚动
4. **自动播放模式**: 按键自动推进 → 循环
5. **按键检测模式**: 切换后按正确键推进（含鼠标键步骤）；按住不放不连跳；
   组合键可识别；普通键忽略残留修饰键；集合内错键红色闪烁且不推进；
   超时跳过生效（配置 >0 时）
6. **前台门控**: 游戏非前台（Alt-Tab 打字）不推进；聊天框场景先 Ctrl+Shift+Space 暂停
7. **移动模式**: Ctrl+Shift+M 后可拖拽移动、滚轮缩放（写回 `scale`），点击不抢游戏焦点，
   ESC 或再次 Ctrl+Shift+M 退出后恢复穿透
8. **交互热键**: Ctrl+Shift+Space 暂停/恢复；Ctrl+Shift+N 切换流程；Ctrl+Shift+H
   显示/隐藏；Ctrl+Shift+Q 退出（热加载后按 `active_rotation` 恢复为阶段 5 验证项）
9. **编辑器**: Ctrl+Shift+E 打开，编辑（含同角色多段）→ 保存 → 即时生效
10. **背景图**: 编辑器选择图片 → 复制入 assets → 窗口更新；图片缺失时回退默认底板
11. **热加载**: 外部修改 JSON → 自动生效；写入损坏 JSON → 保留旧配置并提示
12. **异常用例**: 损坏 JSON 启动回退内置默认；热键被占用时日志报错 + 设置页标红
13. **性能**: 静止时 CPU/GPU 占用≈0；连续运行 2 小时无内存/句柄增长；
    游戏 FPS 开启 overlay 前后对比下降 <5%
14. **游戏测试（签收 checklist）**: 与鸣潮同跑 ≥30 分钟，覆盖登录/战斗/切场景；
    overlay 全程可见置顶；按键检测模式下游戏内按键可推进；无 ACE 警告/踢出。
    **每次鸣潮大版本或 ACE 更新后需重跑本项**
