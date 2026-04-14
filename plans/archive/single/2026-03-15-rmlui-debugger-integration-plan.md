# RmlUi Debugger 独立集成计划

## 目标

- 为 RmlUi Debugger 提供与 ImGui Debug UI 独立的编译开关。
- 为 RmlUi Debugger 提供与 ImGui Debug UI 独立的运行时配置开关。
- 提供 `F4` 快捷键切换 RmlUi Debugger 显示状态。
- 发布构建可通过关闭编译选项避免将 debugger 编译进游戏目标。

## 约束

- 不改变现有 ImGui Debug UI 的 `ENABLE_DEBUG_UI` / `TF_ENABLE_DEBUG_UI` 语义。
- RmlUi Debugger 必须作为 RmlUi 自带调试插件接入，不自建替代调试窗口。
- `F4` 不走动作映射，不受 `InputContext`、菜单上下文或 RmlUi 菜单导航抑制逻辑影响。
- 未编译或运行时禁用时，相关接口保持安全 no-op。

## 方案

### 1. 构建层

- 在根 [CMakeLists.txt](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/CMakeLists.txt) 新增：
  - `option(ENABLE_RMLUI_DEBUGGER "启用 RmlUi Debugger" ON)`
- 仅在该选项开启时：
  - 给 `engine` 增加 `TF_ENABLE_RMLUI_DEBUGGER`
  - 给 `engine` 链接 `rmlui_debugger`

说明：

- `setup_rmlui_dependencies()` 仍保留对 `rmlui_debugger` 目标的创建。
- 当 `engine` 不链接 `rmlui_debugger` 时，该库不会成为主目标依赖，发布构建不会把它编译进游戏目标。

### 2. 运行时配置层

- 在 [config.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/config.h) / [config.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/config.cpp) 新增：
  - `bool rmlui_debugger_enabled_ = true;`
- 在 [config/window.json](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/config/window.json) 的 `graphics` 段新增：
  - `"rmlui_debugger": true`

语义：

- `false` 表示运行时禁用，`F4` 不会打开 debugger。
- `true` 表示允许 debugger 初始化和显示。

### 3. RmlUi 层

- 在 [rml_ui_layer.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/ui/rmlui/rml_ui_layer.h) / [rml_ui_layer.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/ui/rmlui/rml_ui_layer.cpp) 增加：
  - `setDebuggerEnabled(bool)`
  - `isDebuggerEnabled()`
  - `toggleDebuggerVisible()`
  - `setDebuggerVisible(bool)`
  - `isDebuggerVisible()`

行为：

- `TF_ENABLE_RMLUI_DEBUGGER` 开启时，首次启用会调用 `Rml::Debugger::Initialise(context_)`。
- 禁用时主动 `Rml::Debugger::SetVisible(false)`，并执行 `Rml::Debugger::Shutdown()`。
- 未编译时所有接口保持 no-op / `false` 返回。

### 4. 渲染器层

- 在 [gl_renderer.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/render/opengl/gl_renderer.h) / [gl_renderer.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/render/opengl/gl_renderer.cpp) 增加：
  - `setRmlUiDebuggerEnabled(bool)`
  - `isRmlUiDebuggerEnabled()`
  - `toggleRmlUiDebuggerVisible()`
  - `isRmlUiDebuggerVisible()`
- `handleSDLEvent(const SDL_Event&)` 扩展：
  - `F4` 切换 RmlUi Debugger
  - 保留 ImGui 现有 `F5+` 类别切换逻辑
  - ImGui 事件转发逻辑保持原位置

### 5. 输入事件桥接

- 将 `InputManager` 的“ImGui 事件前置转发”泛化为“SDL 事件观察回调”。
- [input_manager.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/input/input_manager.h) / [input_manager.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/input/input_manager.cpp) 调整为：
  - `setSdlEventObserver(std::function<void(const SDL_Event&)>)`
- 回调时机：
  - 在每个 SDL 事件进入 RmlUi / InputManager 路由前先调用
- 目的：
  - 保证 `F4` / `F5+` 这类调试快捷键不依赖动作映射
  - 保持 ImGui `ProcessEvent` 的原有优先级

### 6. GameApp 接线

- 在 [game_app.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/game_app.cpp)：
  - `initGLRenderer()` 中应用 `config_->rmlui_debugger_enabled_`
  - `initInputManager()` 中始终注册 SDL 事件观察回调到 `gl_renderer_->handleSDLEvent(event)`

## 涉及文件

- [CMakeLists.txt](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/CMakeLists.txt)
- [config/window.json](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/config/window.json)
- [src/engine/core/config.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/config.h)
- [src/engine/core/config.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/config.cpp)
- [src/engine/core/game_app.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/core/game_app.cpp)
- [src/engine/input/input_manager.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/input/input_manager.h)
- [src/engine/input/input_manager.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/input/input_manager.cpp)
- [src/engine/render/opengl/gl_renderer.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/render/opengl/gl_renderer.h)
- [src/engine/render/opengl/gl_renderer.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/render/opengl/gl_renderer.cpp)
- [src/engine/ui/rmlui/rml_ui_layer.h](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/ui/rmlui/rml_ui_layer.h)
- [src/engine/ui/rmlui/rml_ui_layer.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/src/engine/ui/rmlui/rml_ui_layer.cpp)
- [tests/CMakeLists.txt](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/tests/CMakeLists.txt)
- 新增测试文件

## 验证

- `ENABLE_RMLUI_DEBUGGER=ON/OFF` 两种构建配置均通过。
- `graphics.rmlui_debugger=true` 时，`F4` 可以显示/隐藏 debugger。
- `graphics.rmlui_debugger=false` 时，`F4` 无效。
- `ENABLE_DEBUG_UI=OFF` 但 `ENABLE_RMLUI_DEBUGGER=ON` 时，RmlUi Debugger 仍可工作。
- 现有 ImGui `F5/F6...` 面板切换不回归。
