# 输入系统约定：InputManager / 动作映射 / 状态机 / 鼠标逻辑坐标（TinyFarm）

> 用途：统一项目内“输入系统”的心智模型与约定（动作映射、`PRESSED/HELD/RELEASED` 帧语义、占用/转发规则、以及鼠标 logical 坐标映射），统一项目内"输入系统"的心智模型与约定。

TinyFarm 的输入系统可以用一句话概括：
> **InputManager 把 SDL 的“物理输入事件”翻译成“动作（Action）状态”，上层只依赖动作，不直接依赖 SDL。**

---

## 1) 关键模块与职责边界

### 1.1 InputManager（动作外观层）
- 入口：`src/engine/input/input_manager.h/.cpp`
- 核心类型：
  - `engine::input::InputManager`
  - `engine::input::ActionState`：`PRESSED / HELD / RELEASED / INACTIVE`

职责：
- 从 `config/input.json` 读取 `action_name -> keys` 映射
- 将 `SDL_Event` 转成 `action_states_`（状态机）
- 提供两种使用方式：
  - polling：`isActionDown/isActionPressed/isActionReleased`
  - callback：`onAction(...).connect(...)`
- 维护鼠标坐标：
  - `mouse_position_`：SDL window coordinates
  - `logical_mouse_position_`：映射到逻辑分辨率（用于 UI hit-test 与世界交互）

### 1.2 GameApp（输入阶段与 ImGui 转发）
- 入口：`src/engine/core/game_app.cpp`
- 关键点：
  - 主循环的输入阶段调用 `InputManager::update()`
  - `InputManager` 会把 SDL 事件先转发给 ImGui（通过 `setImGuiEventForwarder`），再决定是否映射成游戏动作

### 1.3 UI（占用/转发的落地点）
- `src/engine/ui/ui_manager.cpp`：监听 `mouse_left` 的 `PRESSED/RELEASED` 回调并返回 `bool`，用于“吃掉输入”
- `src/engine/ui/ui_input_blocker.cpp/h`：全屏透明 `UIInteractive`，用来阻断 world 点击

### 1.4 游戏系统示例（polling + callback 混用）
- `src/game/system/player_control_system.cpp/h`
  - 移动：polling（`isActionDown(move_*)`）
  - 点击/快捷栏：callback（`onAction(...).connect`）
  - 鼠标：`logical_mouse → camera.screenToWorld → world`
- `src/game/system/interaction_system.cpp/h`
  - 交互：`isActionPressed("interact")` 触发事件

---

## 2) 动作状态机（帧语义）

InputManager 的动作状态机可按每帧理解为：
- `PRESSED`：本帧第一次按下（边沿）
- `HELD`：持续按住（SDL 不会每帧产生 KEY_DOWN，因此需要该状态）
- `RELEASED`：本帧释放（边沿）
- `INACTIVE`：未激活

状态推进（每帧 update 开始时）：
- `PRESSED → HELD`
- `RELEASED → INACTIVE`

这允许上层清晰地区分：
- “按下一次触发”（只看 `isActionPressed` / 绑定 `PRESSED`）
- “持续按住移动”（看 `isActionDown` / 绑定 `HELD`）

---

## 3) 占用/转发规则（为什么回调要返回 bool）

InputManager 的回调分发遵循一个工程化目标：
> **同一个动作可以被多个系统订阅，但应允许“优先级更高的系统”占用输入，阻止继续传播。**

在 TinyFarm 中：
- `onAction(action, state)` 的回调返回 `bool`
- 当某个回调返回 `true` 时，本次分发停止（后续订阅者不再收到）
- 订阅调用顺序为“后绑定先调用”（后注册的订阅者优先）

典型用法：
- UI 在需要时“吃掉”鼠标点击（避免 UI 点击同时触发世界操作）

---

## 4) 鼠标逻辑坐标（logical）与 letterbox / 高 DPI

为什么要区分 `mouse_position` 与 `logical_mouse_position`：
- SDL 的鼠标坐标是 **window coordinates**
- 渲染通常在 **logical resolution** 上做布局（UI）或通过相机从 screen 推到 world（交互）

因此 TinyFarm 在 InputManager 内做统一映射：
- 输入：`mouse_position_`（window coords）
- 结合：
  - `GameState::getWindowSize()`（window coords 尺寸）
  - `GameState::getLogicalSize()`（逻辑分辨率）
  - `engine::utils::computeLetterboxMetrics()`（viewport + scale）
- 输出：`logical_mouse_position_`（clamp 到 `[0..logical_size]`）

推荐验证入口：
- `F5` → `Engine Debug Panels` → `Input`：同时观察 `Mouse Position` 与 `Logical Position`，并在窗口缩放/letterbox 时确认 logical 坐标稳定。

---

## 5) `config/input.json`（参数说明）

入口：`config/input.json`（也允许通过 `InputManager::create(..., config_path)` 指定自定义路径）

结构：
- 根对象可直接包含映射，也支持包一层 `input_mappings`（推荐）
- `input_mappings`：对象，`{ "<action_name>": ["KeyName", "MouseLeft", ...] }`

键名来源：
- 键盘：SDL 的 key name（示例：`"W"`, `"Space"`, `"Escape"`, `"Left"`, `"Right"`）
- 鼠标按钮（本项目约定的字符串）：
  - `MouseLeft`
  - `MouseRight`
  - `MouseMiddle`
  - `MouseX1`
  - `MouseX2`

注意：
- `mouse_left` / `mouse_right` 若未在配置里声明，InputManager 会自动补默认映射（主要服务 UI 交互）。

---

## 6) 常见坑

1) **调试 UI 打字时，游戏动作被误触发**
- 原因：ImGui 捕获键盘时仍把 KEY_DOWN 映射成动作
- 排查：看 `Input` 面板状态变化 + 日志

2) **UI 点击与世界点击同时触发**
- 原因：没有明确的“占用/转发”规则，或 UI 没有吃掉输入
- 解决：回调返回 `true` 占用；必要时用 `UIInputBlocker` 覆盖全屏

3) **鼠标坐标不对（缩放/letterbox/高 DPI）**
- 原因：把 window coords 当成 logical coords 使用
- 解决：统一使用 `getLogicalMousePosition()` 做 UI hit-test 与 `camera.screenToWorld`
