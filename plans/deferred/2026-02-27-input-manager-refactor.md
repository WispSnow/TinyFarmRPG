# InputManager 重构待办清单

## 元信息
- 创建日期：`2026-02-27`
- 涉及文件：`src/engine/input/input_manager.{h,cpp}`、`tests/engine/input/input_manager_test.cpp`、`config/input.json`
- 来源：Claude + Codex 联合审查 + Codex 二次审阅修正
- 状态：`Done`（P0~P2 全部完成，P3 待 RPG 拓展时按需实施）

---

## P0 — 存在运行时 Bug

### INPUT-001 多键绑定同一动作时释放逻辑错误
- **现状**：`updateActionState` 直接以最新事件覆盖动作状态（`input_manager.cpp:430-448`）。当同一动作绑定多个键（如 `move_left -> [A, Left]`），释放其中一个键会立即将动作置为 RELEASED，即使另一个键仍然按住。
- **复现场景**：同时按 A + Left 移动，松开 A → 角色停止移动（Left 仍按住）。
- **修复方案**：引入 per-input down 状态表 + per-action active_count 引用计数。
  - 新增 `input_down_states_`（类型取决于 INPUT-003 / INPUT-004 的 key 设计），记录每个物理输入当前是否按下。
  - **关键约束：只在物理边沿变化时计数，而非每次事件。** 具体逻辑：
    ```
    on KEY_DOWN / BUTTON_DOWN:
        if input_down_states_[key] == true:
            return;  // 已按下（含 SDL repeat），忽略
        input_down_states_[key] = true;
        action.active_count++;
        if active_count == 1:
            action.state = PRESSED;

    on KEY_UP / BUTTON_UP:
        if input_down_states_[key] == false:
            return;  // 未按下，忽略
        input_down_states_[key] = false;
        action.active_count--;
        if active_count == 0:
            action.state = RELEASED;
    ```
  - 这里 `is_repeat_event` 检查不再需要（被 `input_down_states_` 边沿检查覆盖），但保留它做 double-check 也无害。
- **测试**：
  - `MultiKeyBindingSameAction`：A+Left 同时按下 → 释放 A → 仍 HELD → 释放 Left → RELEASED。
  - `RepeatKeyDownDoesNotInflateActiveCount`：按下 A → 发送多次 repeat KEY_DOWN → 释放 A → 正常 RELEASED（active_count 回到 0）。

### INPUT-002 窗口失焦导致卡键
- **现状**：`processEvent` 没有处理 `SDL_EVENT_WINDOW_FOCUS_LOST` / `SDL_EVENT_WINDOW_MINIMIZED`（`input_manager.cpp:153`）。如果在按键期间切出窗口，SDL 不会发送 KEY_UP，动作会永久停在 HELD。
- **行为契约（定死语义）**：失焦时所有活跃动作 **直接置为 INACTIVE，不经过 RELEASED**，同时清零 `input_down_states_` 和所有 `active_count`。
  - **理由**：RELEASED 的语义是"用户主动释放输入"，失焦是系统事件而非用户操作。若走 RELEASED 路径，会触发 `onMouseReleased` 等回调，可能导致 UI 误判为用户点击（`UIInteractive::mouseReleased` 在 `is_inside=true` 时会触发 `onClick`）。
  - **UI 清理方案**：通过 `entt::dispatcher` 发送 `FocusLostEvent`。UIManager 监听此事件，对 `pressed_element_` 调用 `mouseReleased(false)`（即"非 in-bounds 释放"），确保按压态正确清理且不触发 onClick。
  - 参考：`ui_manager.cpp:210-226`（onMouseReleased 依赖 RELEASED 回调）、`ui_interactive.cpp:355-386`（mouseReleased 的 is_inside 逻辑）。
- **修复方案**：
  ```cpp
  case SDL_EVENT_WINDOW_FOCUS_LOST:
  case SDL_EVENT_WINDOW_MINIMIZED: {
      // 清空所有物理输入状态
      for (auto& [key, down] : input_down_states_) down = false;
      // 清空所有动作状态（跳过 RELEASED，直接 INACTIVE）
      for (auto& [id, entry] : actions_) {
          entry.active_count = 0;
          entry.state = ActionState::INACTIVE;
      }
      dispatcher_->trigger<engine::utils::FocusLostEvent>();
      break;
  }
  ```
- **测试**：`FocusLostClearsHeldActions`：按下 A → 发送 FOCUS_LOST → 验证状态为 INACTIVE（不是 RELEASED） → 验证 `isActionDown` 为 false。

---

## P1 — 结构性改进 （已完成）

### INPUT-004 合并三个 per-action map 为单一结构
- **现状**：`actions_to_func_`、`action_states_`、`action_id_to_name_` 三个 map 以相同的 `entt::id_type` 为 key（`input_manager.h:49-55`），查找和遍历需要分别访问，缓存不友好且容易不一致。
- **修复方案**：
  ```cpp
  struct ActionEntry {
      ActionState state = ActionState::INACTIVE;
      uint8_t     active_count = 0;            // INPUT-001 引入
      std::string name;                         // debug only
      std::array<entt::sigh<bool()>, CALLBACK_STATE_COUNT> signals;
  };
  std::unordered_map<entt::id_type, ActionEntry> actions_;
  ```
- **建议**：与 INPUT-001 一起做，避免二次重构。

### INPUT-005 窗口事件职责迁移
- **现状**：`processEvent` 处理 `SDL_EVENT_WINDOW_RESIZED` 和 `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`（`input_manager.cpp:206-225`），dispatch `WindowResizedEvent`。这属于窗口管理关注点，放在 InputManager 里职责不清。
- **架构约束：SDL_PollEvent 保持单一所有者**。当前只有 `InputManager::sampleInputEvents()` 调用 `SDL_PollEvent`（`input_manager.cpp:97`），全代码库无第二处消费。迁移窗口事件处理时 **不能引入第二个 poll 点**，否则会出现事件重复消费或丢失。
- **修复方案**：InputManager 仍然是唯一的 SDL 事件泵，但窗口事件处理改为"仅 dispatch，不内联处理"：
  - `processEvent` 中的 `SDL_EVENT_WINDOW_RESIZED` / `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` 分支简化为仅通过 `dispatcher_->trigger<WindowResizedEvent>(...)` 转发。
  - InputManager 自身通过监听 `WindowResizedEvent`（或在 `recalculateLogicalMousePosition` 中懒取 metrics）来更新鼠标逻辑坐标。
  - GameApp / GameState 等窗口关注方同样通过 dispatcher 监听，不直接接触 SDL 事件。

---

## P2 — 健壮性与可维护性 （已完成）

### INPUT-003 拆分 `std::variant<SDL_Scancode, Uint32>` map key（结构优化）
- **现状**：`input_to_actions_` 使用 `std::variant<SDL_Scancode, Uint32>` 作为 key（`input_manager.h:58`）。
- **澄清**：`std::hash<std::variant>` 在 C++17 已标准化（只要所有 alternative 可哈希），且 variant 比较包含 alternative index，不存在值域碰撞。此项 **不是正确性修复**，而是可读性 / 结构优化。
- **修复方案**：拆为两个独立 map：
  ```cpp
  std::unordered_map<SDL_Scancode, std::vector<entt::id_type>> key_to_actions_;
  std::unordered_map<Uint32, std::vector<entt::id_type>>       mouse_to_actions_;
  ```
  `processEvent` 中键盘和鼠标本身是分支处理的，拆开后更清晰。同时为未来手柄输入（INPUT-010）预留扩展点。
- **注意**：若已在 INPUT-001 中引入了 `input_down_states_`，此处同步拆分其 key 类型。

### INPUT-006 `scancodeFromString` 接口收口（稳健性改进）
- **现状**：`scancodeFromString` 对 `string_view` 直接调用 `.data()` 传给 C API（`input_manager.cpp:416`）。`string_view::data()` 不保证 null-terminated。
- **说明**：当前所有调用链传入的是 `std::string`（来自 JSON 解析），`.data()` 实际是 NUL 结尾的。**这不是现网 bug，而是 API 契约稳健性改进**，防止未来调用方传入 substring view 导致 UB。
- **修复**：改为接收 `const std::string&`，或构造临时 `std::string`：
  ```cpp
  return SDL_GetScancodeFromName(std::string(key_name).c_str());
  ```
  仅在初始化时调用，拷贝开销可忽略。

### INPUT-007 同步默认映射与实际配置
- **现状**：`defaultMappings()`（`input_manager.cpp:24-34`）包含 `jump`、`attack`，但 `config/input.json` 中没有这两个动作，反而有 `interact`、`inventory`、`hotbar` 等。配置加载失败时大量功能丢失。
- **修复**：使 `defaultMappings()` 与 `input.json` 保持一致，或直接删除默认映射改为加载失败时 fatal。

### INPUT-008 分发顺序不稳定
- **现状**：`dispatchActionCallbacks` 遍历 `unordered_map`（`input_manager.cpp:110`），同帧多个动作的分发顺序不确定，可能导致 UI/场景切换等场景下的复现困难。
- **修复方案**：维护一个 `std::vector<entt::id_type> action_dispatch_order_`，按配置加载顺序（或显式 priority）排列，`dispatchActionCallbacks` 按此顺序遍历。

---

## P3 — 面向 JRPG 拓展（可延后）

### INPUT-009 Input Context 分层
- **现状**：所有场景共享同一份 action 状态和回调。场景切换时需手动 connect/disconnect（如 `game_scene.cpp:112-114`），容易遗漏。
- **建议**：引入 InputContext 概念（类似 Unreal InputMappingContext），支持按优先级 push/pop，高优先级 context 可屏蔽低优先级动作。

### INPUT-010 手柄支持
- **现状**：`processEvent` 完全没有处理 `SDL_EVENT_GAMEPAD_*` 事件。JRPG 玩家常用手柄操作。
- **建议**：扩展 input key 类型支持 `GamepadButton` / `GamepadAxis`，在 `initializeMappings` 中支持 `"GamepadA"`, `"LeftStickUp"` 等字符串映射。

### INPUT-011 输入缓冲
- **现状**：边沿状态（PRESSED）只存活一个 tick。回合制战斗 UI 中快速连续按键可能丢失输入。
- **建议**：可选的 input buffer queue，保留最近 N 帧的边沿事件供战斗系统消费。

---

## 测试覆盖补充

当前测试（`tests/engine/input/input_manager_test.cpp`）覆盖了单键生命周期、鼠标事件、ImGui 转发、三段式接口、滚轮 delta。需补充：

| 测试用例 | 对应任务 | 说明 |
|---|---|---|
| `MultiKeyBindingSameAction` | INPUT-001 | A+Left 同按 → 释放 A → 仍 HELD → 释放 Left → RELEASED |
| `RepeatKeyDownDoesNotInflateActiveCount` | INPUT-001 | 按下 A → 多次 repeat → 释放 → 正常 RELEASED |
| `FocusLostClearsHeldActions` | INPUT-002 | 按下 A → FOCUS_LOST → 状态直接 INACTIVE（不经过 RELEASED） |
| `FocusLostDoesNotDispatchReleasedCallbacks` | INPUT-002 | 注册 RELEASED 回调 → FOCUS_LOST → 回调不被调用 |
| `DispatchOrderIsStable` | INPUT-008 | 注册多个动作回调，验证分发顺序一致 |

---

## 建议实施顺序

```
INPUT-001 + INPUT-004  (合并做，引入 ActionEntry + active_count + input_down_states_)
    ↓
INPUT-002              (在新结构上加 focus lost 清空 + FocusLostEvent)
    ↓
INPUT-006, INPUT-007   (小修，可随时穿插)
    ↓
INPUT-003              (拆 variant map，结构优化)
    ↓
INPUT-005, INPUT-008   (职责迁移 + 分发顺序稳定化)
    ↓
INPUT-009 ~ INPUT-011  (RPG 拓展时按需)
```
