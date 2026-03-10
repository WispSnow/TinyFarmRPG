### Phase 3: InputContext 上下文分层

**目标**：引入输入上下文栈，让不同场景（世界/菜单/对话/战斗）拥有清晰的输入边界，高优先级上下文可屏蔽低优先级动作。

---

#### 设计要点

**InputContext** 定义一组"当前允许的动作"。`InputManager` 维护一个上下文栈，只有栈顶 context 中的动作会被 dispatch。

```cpp
/// 预定义的上下文 ID（可扩展）
enum class InputContextId : uint8_t {
    Gameplay,   // 移动、交互、工具、快捷栏
    Menu,       // menu_* 导航、确认、取消
    Dialogue,   // 对话推进、选项选择
    Battle      // 战斗菜单操作
};

struct InputContext {
    InputContextId id;
    std::vector<entt::id_type> allowed_actions;  // 此上下文中活跃的动作 ID 集合
};
```

**栈行为**：

- `pushContext(InputContextId)` — 压入新上下文，**立即清空所有当前动作状态**（同 focus-lost 语义：直接 INACTIVE，不经过 RELEASED）。
- `popContext()` — 弹出栈顶，恢复上一层上下文。同样清空动作状态。
- `dispatchActionCallbacks` 仅遍历栈顶 context 的 `allowed_actions`。
- 状态查询（`isActionDown` 等）不受 context 过滤——context 只影响回调分发和状态更新。如果调用方需要查询非当前 context 的动作，它自行判断。
  > 理由：状态查询是只读的，过滤它会增加复杂度但不增加安全性。真正需要屏蔽的是状态的产生（processEvent 中忽略非当前 context 的 binding）和回调的触发。

**processEvent 过滤**：处理 SDL 输入事件时，只有在栈顶 context 的 `allowed_actions` 中的动作才会更新状态。不在当前 context 中的 binding 被忽略。

**谁持有上下文栈**：`InputManager` 自己。不引入独立的 `InputContextManager`——上下文切换与事件处理紧密耦合，拆开反而增加调用开销。

**上下文定义注册**：在 `InputManager` 初始化后（或 loadConfig 时），注册各 context 的 allowed_actions 列表。可以硬编码在代码中（用 `entt::hashed_string` 列表），也可以从配置文件读取。初期硬编码更简单。

---

#### 需要修改的文件

- `src/engine/input/input_manager.h` — 新增 `InputContext`、上下文栈、push/pop 接口
- `src/engine/input/input_manager.cpp` — processEvent 过滤、dispatch 过滤、push/pop 实现
- `src/game/scene/game_scene.cpp` — 进入时 push `Gameplay`
- `src/game/scene/pause_menu_scene.cpp` — 进入时 push `Menu`，退出时 pop
- 后续对话/战斗场景入口

#### 需要新增的文件

- `tests/engine/input/input_context_test.cpp`

---

#### Step 3.1: 定义 InputContext 数据结构

- 在 `input_manager.h` 中定义 `InputContextId` 枚举和 `InputContext` 结构。
- `InputManager` 新增：
  ```cpp
  std::vector<InputContext> context_stack_;
  void pushContext(InputContextId id);
  void popContext();
  InputContextId currentContext() const;
  ```
- 注册上下文定义的方法（或直接在 push 时传入 allowed_actions）。

#### Step 3.2: processEvent 中的上下文过滤

- `handleInputEdge` 调用前，检查目标动作是否在栈顶 context 的 allowed_actions 中。
- 不在当前 context 中的 binding → 跳过，不更新 down_state 也不更新 active_count。
- 特例：`SDL_EVENT_WINDOW_FOCUS_LOST` 和 `SDL_EVENT_QUIT` 不受 context 过滤。

#### Step 3.3: dispatch 中的上下文过滤

- `dispatchActionCallbacks` 从遍历 `action_dispatch_order_` 改为只遍历栈顶 context 的 allowed_actions（取交集或直接用 context 列表）。

#### Step 3.4: 场景接入

- `GameScene::onEnter()` → `pushContext(Gameplay)`
- `PauseMenuScene::onEnter()` → `pushContext(Menu)`
- `PauseMenuScene::onExit()` → `popContext()`
- 确保 push/pop 配对，栈不会泄漏。

#### Step 3.5: 定义各上下文的动作白名单

初期硬编码：

| Context | 允许的动作 |
|---------|-----------|
| Gameplay | `move_*`, `primary_action`, `secondary_action`, `interact`, `hotbar_*`, `pause`, `inventory`, `rotate_*`, `player_light`, `camera_reset_zoom` |
| Menu | `menu_*`, `menu_confirm`, `menu_cancel`, `pause`（用于返回） |
| Dialogue | `menu_confirm`（推进对话）, `menu_cancel`（跳过/关闭） |
| Battle | `menu_*`, `menu_confirm`, `menu_cancel`（战斗菜单导航） |

> `menu_*` 动作在 Phase 4 中正式引入。Phase 3 只需要确保 context 机制能过滤这些动作名。

---

#### 测试用例

| 测试用例 | 说明 |
|---------|------|
| `ContextFiltersPressEvents` | Gameplay context 下，`menu_confirm` 按键不产生 PRESSED 状态 |
| `PushContextClearsActiveActions` | push 新 context 时，旧 context 中 HELD 的动作变为 INACTIVE |
| `PopContextRestoresPreviousFilter` | pop 后，原 context 的动作恢复可用 |
| `ContextStackMultipleLevels` | 连续 push 多层 → pop 逐层恢复 |
| `FocusLostBypassesContextFilter` | 失焦清理不受 context 限制 |

---

#### 待办清单

- [ ] 定义 `InputContextId` 和 `InputContext`
- [ ] 实现 push/pop 上下文栈
- [ ] processEvent 中加入上下文过滤
- [ ] dispatchActionCallbacks 中加入上下文过滤
- [ ] 在 GameScene / PauseMenuScene 中接入 push/pop
- [ ] 定义各上下文的动作白名单
- [ ] 编写上下文测试

#### 完成标准

- Gameplay 和 Menu 场景的输入边界清晰：在菜单中按 WASD 不会移动角色。
- push/pop 配对正确，无栈泄漏。
- 上下文切换时不残留旧的动作 HELD 状态。
