# MingC Key Overlay

鸣潮（Wuthering Waves）弹幕式按键提示 overlay。

## 使用说明

- **推荐游戏模式**：无边框窗口（全屏独占下 DWM 不参与合成，非注入 overlay 可能无法显示）。
- **快捷键**（默认均带 `Ctrl+Shift` 前缀，可在配置中修改）：
  - `Ctrl+Shift+M`：切换移动模式（临时移除点击穿透，可拖拽/缩放）。
  - `Ctrl+Shift+H`：显示/隐藏 overlay。
  - `Ctrl+Shift+Space`：播放/暂停。
  - `Ctrl+Shift+P`：切换自动播放/按键检测模式。
  - `Ctrl+Shift+N`：切换当前队伍流程。
  - `Ctrl+Shift+E`：打开编辑器。
  - `Ctrl+Shift+R`：重载配置。
  - `Ctrl+Shift+Q`：退出程序。
- 配置文件：`profiles/default.json`（UTF-8）。
- 资源目录：`assets/`（背景图、头像等）。

## 构建

要求：Windows 10/11、Visual Studio 2022、CMake 3.21+。

```bash
cmake --preset debug
cmake --build build/debug --config Debug
```

## 鸣潮兼容性

- 不注入游戏进程、不使用全局钩子，仅使用 `RegisterHotKey` 与本窗口消息处理。
- 按键检测使用 `GetAsyncKeyState` 轮询或本窗口 `WM_KEYDOWN` 事件。
- 反作弊风险低，但需以实测为准；若被拦截请切换为无边框窗口或副屏方案。

## 目录结构

```
MingC/
├── src/          # 源码
│   ├── overlay/  # 透明覆盖窗口与 D2D 渲染
│   ├── editor/   # ImGui 编辑器
│   ├── core/     # 配置、播放引擎、输入检测
│   └── utils/    # 日志、编码、资源加载、热键
├── profiles/     # JSON 配置
├── assets/       # 图片资源
└── README.md
```
