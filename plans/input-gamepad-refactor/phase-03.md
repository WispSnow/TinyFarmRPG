### Phase 3: InputContext 上下文分层

**目标**：引入输入上下文栈，让不同场景（世界/菜单/对话/战斗）拥有清晰的输入边界，高优先级上下文可屏蔽低优先级动作。

---

#### 设计要点

**InputContext** 定义一组"当前允许的动作"。`InputManager` 维护一个**上下文 ID 栈**，并在内部维护每个 context 的运行时定义；只有栈顶 context 中的动作会被更新和 dispatch。

```cpp
/// 预定义的上下文 ID（可扩展）
enum class InputContextId : uint8_t {
    Gameplay,   // 移动、交互、工具、快捷栏
    Menu,       // 菜单/暂停/存档选择
    Dialogue,   // 对话/模态确认
    Battle      // 战斗菜单操作
};

struct InputContextDefinition {
    std::vector<entt::id_type> dispatch_actions;     // 保留稳定顺序，用于 dispatch
    std::unordered_set<entt::id_type> allowed_actions; // 用于 processEvent 快速判断动作是否允许
};
```

**栈行为**：

- `pushContext(InputContextId)` — 压入新上下文，**立即清空全部输入状态**：包括 `key/mouse/gamepad *_down_states_`、手柄调试状态、`active_count` 和动作状态（同 focus-lost 语义：直接 INACTIVE，不经过 RELEASED）。
- `popContext()` — 弹出栈顶，恢复上一层上下文；同样先执行一次完整输入状态清空。
- `dispatchActionCallbacks` 仅遍历栈顶 context 的 `dispatch_actions`。
- 状态查询（`isActionDown` 等）不受 context 过滤。context 只影响状态产生和回调触发；如果调用方要读非当前 context 的动作，自行判断。
  > 理由：状态查询是只读的，过滤它会增加复杂度但不增加安全性。真正需要屏蔽的是状态的产生（`processEvent` 中忽略非当前 context 的 binding）和回调的触发。
- **空栈回退到旧行为**：当 `context_stack_` 为空时，不做 context 过滤，保持当前“所有配置动作都可用”的行为，避免未接入 context 的 scene 和旧测试立即失效。
- **空栈 pop 安全返回**：`popContext()` 在空栈时只记一条 warn log 并直接返回，不做额外状态改动。
- **同动作多监听者仍沿用现有消费模型**：Phase 3 不修改 `onAction()` 的全局 signal 架构；同一动作在多层 scene 中的消费顺序继续依赖现有 `entt::sigh<bool()>` 语义。约定栈顶 scene 在 `init()` 中连接、在 `clean()` 中断开，并在已处理时返回 `true`。

**processEvent 过滤**：处理 SDL 输入事件时，物理键/按钮/轴的 `down_state` 仍然照常更新；只有落在栈顶 context 的 `allowed_actions` 中的动作才会更新 `active_count` 和 `ActionState`。不在当前 context 中的动作映射会被逐 action 跳过。

**谁持有上下文栈**：`InputManager` 自己。不引入独立的 `InputContextManager`，因为上下文切换与事件处理紧密耦合，拆开只会增加同步成本。

**上下文定义注册**：在 `InputManager` 初始化后（或 `loadConfig()` / `initializeMappings()` 之后），注册各 context 的动作定义。可以先用动作名硬编码，再解析为 `entt::id_type`。注册时只保留**当前配置里真实存在**的动作；未知的未来动作名（例如 Phase 4 才加入的 `menu_*`）直接跳过，不把空动作塞进运行时表。

**Scene 生命周期约束**：本阶段**不新增** `Scene::onEnter()` / `onExit()`。现有 `Scene` 生命周期只有 `init()` / `clean()`，context 接入必须落在这两个钩子中。

**TitleScene 约束**：`TitleScene` 不接入 context，保持空栈回退行为。它作为入口/过渡 scene，继续沿用当前 legacy 输入路径即可。

**本阶段暂不覆盖的范围**：`GameScene` 内部的 `InventoryUI` / `HotbarUI` 只是同场景内面板显示切换，不单独 push scene；Phase 3 不为它们新增独立 context，后续若需要再单独规划。

---

#### 需要修改的文件

- `src/engine/input/input_manager.h` — 新增 `InputContextId`、运行时 context 定义、上下文栈、push/pop 接口
- `src/engine/input/input_manager.cpp` — `processEvent` 过滤、dispatch 过滤、push/pop 与统一输入状态清理实现
- `src/game/scene/game_scene.cpp` — `init()/clean()` 中接入 `Gameplay`
- `src/game/scene/pause_menu_scene.cpp` — `init()/clean()` 中接入 `Menu`
- `src/game/scene/save_slot_select_scene.cpp` — `init()/clean()` 中接入 `Menu`
- `src/game/scene/rest_dialog_scene.cpp` — `init()/clean()` 中接入 `Dialogue`
- `src/game/scene/battle_scene.cpp` — `init()/clean()` 中接入 `Battle`
- `src/game/scene/title_scene.cpp` — 无需接入；保持空栈回退行为（文档说明即可）

#### 需要新增的文件

- `tests/engine/input/input_context_test.cpp`
- （建议）`tests/game/...` 中补一个 scene 栈集成测试

---

#### Step 3.1: 定义 InputContext 数据结构与清理辅助

- 在 `input_manager.h` 中定义 `InputContextId` 和 `InputContextDefinition`。
- `InputManager` 新增：
  ```cpp
  std::unordered_map<InputContextId, InputContextDefinition> context_definitions_;
  std::vector<InputContextId> context_stack_;
  void pushContext(InputContextId id);
  void popContext();
  std::optional<InputContextId> currentContext() const;
  void clearAllInputState();
  ```
- `clearAllInputState()` 抽取现有 focus-lost 的**状态清理部分**，供 `pushContext()` / `popContext()` / `SDL_EVENT_WINDOW_FOCUS_LOST` 复用。
- `clearAllInputState()` **不触发** `FocusLostEvent`；`SDL_EVENT_WINDOW_FOCUS_LOST` / `SDL_EVENT_WINDOW_MINIMIZED` 路径在调用它之后，继续显式触发 `FocusLostEvent`，保持语义分离。
- 注册上下文定义的方法在 `loadConfig()` / `initializeMappings()` 之后执行，避免 future action 名被提前物化成空动作。
- `dispatch_actions` 的构建顺序必须来自 `action_dispatch_order_`：遍历配置加载顺序，只把命中当前 context 白名单的动作 push 进去，确保跨 context 的 dispatch 顺序稳定一致。

#### Step 3.2: processEvent 中的上下文过滤

- 不在 `handleInputEdge` 调用前做整键过滤；过滤粒度必须落在 **per-action 循环** 内。
- `handleInputEdge` 建议增加一个 `const std::unordered_set<entt::id_type>* allowed_actions` 参数：
  ```cpp
  template <typename KeyT>
  void handleInputEdge(KeyT key,
                       /* existing params */,
                       bool is_down,
                       const std::unordered_set<entt::id_type>* allowed_actions);
  ```
- `down_state` 始终照常更新，确保边沿检测准确；只对 `allowed_actions` 中的动作更新 `active_count` 和 `ActionState`。
- 这样即使“同一物理键映射到多个动作，其中只有一部分属于当前 context”，允许的动作仍能正常生效。
- context 切换前后必须调用 `clearAllInputState()`，否则会残留旧 context 的物理按键缓存，导致“切换回来后第一次按键/松键丢失”。
- 特例：`SDL_EVENT_WINDOW_FOCUS_LOST`、`SDL_EVENT_WINDOW_MINIMIZED` 和 `SDL_EVENT_QUIT` 不受 context 过滤。

#### Step 3.3: dispatch 中的上下文过滤

- `dispatchActionCallbacks` 从遍历 `action_dispatch_order_` 改为只遍历栈顶 context 的 `dispatch_actions`。
- 同一动作若在多层 scene 中都有监听者，仍沿用当前 bool-return consume 语义，不在 Phase 3 再引入第二套 scene-local dispatch。

#### Step 3.4: 场景接入

- `GameScene::init()` / `clean()` → push/pop `Gameplay`
- `PauseMenuScene::init()` / `clean()` → push/pop `Menu`
- `SaveSlotSelectScene::init()` / `clean()` → push/pop `Menu`
- `RestDialogScene::init()` / `clean()` → push/pop `Dialogue`
- `BattleScene::init()` / `clean()` → push/pop `Battle`
- `TitleScene` 不接入 context，保持空栈 legacy 行为。
- scene 不新增 `onEnter/onExit`，直接复用现有生命周期。
- 为避免 `init()` 中途失败导致 `clean()` 误 pop，scene 侧需要用一个本地标志（如 `context_pushed_`）保证 push/pop 成对且幂等。
- `SceneManager::replaceScene()` 当前按“从栈顶到栈底逐个 clean()”执行，context 会自然按 LIFO 次序回退；该行为与本方案兼容，不需要额外改动。

#### Step 3.5: 定义各上下文的动作白名单

初期硬编码，但**运行时只启用当前配置中实际存在的动作**：

| Context | 允许的动作 |
|---------|-----------|
| Gameplay | `move_*`, `primary_action`, `secondary_action`, `interact`, `hotbar_*`, `pause`, `inventory`, `rotate_*`, `player_light`, `camera_reset_zoom` |
| Menu | **Phase 3 运行时先只保留** `pause`（用于返回）；Phase 4 引入 `menu_*` / `menu_cancel` 后，再将 Menu context 迁移为 `menu_*`, `menu_confirm`, `menu_cancel`，并移除 `pause`，避免同一物理键同时触发两个语义动作 |
| Dialogue | Phase 3 可先为空（只负责阻断 Gameplay 动作）；后续对话输入接入时再加入 `menu_confirm`, `menu_cancel` |
| Battle | Phase 3 可先为空（只负责阻断 Gameplay 动作）；后续战斗菜单接入时再加入 `menu_*`, `menu_confirm`, `menu_cancel` |

> `menu_*` 动作在 Phase 4 中正式引入。Phase 3 只需要把 context 基础设施和现有 scene 栈行为铺好，不要求提前把不存在的动作接进运行时表。

---

#### 测试用例

| 测试用例 | 说明 |
|---------|------|
| `ContextFiltersPressEvents` | Gameplay context 下，菜单动作按键不会产生 PRESSED 状态 |
| `PushContextClearsActiveActions` | push 新 context 时，旧 context 中 HELD 的动作变为 INACTIVE |
| `ContextSwitchClearsPhysicalDownCaches` | context 切换会同步清空 `key/mouse/gamepad *_down_states_`，切回后第一次输入不会失效 |
| `SharedPhysicalBindingOnlyActivatesAllowedActions` | 同一物理键映射到两个不同 context 动作时，只激活当前 context 允许的动作 |
| `PopContextRestoresPreviousFilter` | pop 后，原 context 的动作恢复可用 |
| `ContextStackMultipleLevels` | 连续 push 多层 → pop 逐层恢复 |
| `EmptyStackUsesLegacyBehavior` | 未 push 任何 context 时，InputManager 与当前版本行为一致 |
| `FocusLostBypassesContextFilter` | 失焦清理不受 context 限制 |

另补一个 game 层集成用例：验证 `PauseMenuScene -> SaveSlotSelectScene` 叠层时，栈顶 scene 的返回动作优先消费，pop 后 context 正确恢复到下层 scene。

---

#### 待办清单

- [x] 定义 `InputContextId` 和运行时 context 定义
- [x] 实现 push/pop 上下文栈
- [x] 抽取 `clearAllInputState()` 并复用现有 focus-lost 清理逻辑
- [x] processEvent 中加入上下文过滤
- [x] dispatchActionCallbacks 中加入上下文过滤
- [x] 在 GameScene / PauseMenuScene / SaveSlotSelectScene / RestDialogScene / BattleScene 中接入 push/pop
- [x] 定义各上下文的动作白名单
- [x] 明确 `TitleScene` 继续使用空栈 legacy 行为
- [x] 编写上下文测试（含 stale down-state 回归）
- [x] 编写共享物理键跨 context 过滤测试
- [x] 编写 scene 栈集成测试（PauseMenu -> SaveSlotSelect）

#### 完成记录（2026-03-11）

- `InputManager` 已引入 `InputContextId`、context 定义表与 LIFO 上下文栈；空栈保持 legacy 行为，空栈 `popContext()` 仅 warn 并安全返回。
- `processEvent()` 已改为“物理 down-state 始终更新、动作状态按栈顶 `allowed_actions` 逐 action 过滤”；`dispatchActionCallbacks()` 只遍历当前 context 的 `dispatch_actions`，并继续沿用既有 consume 语义。
- `clearAllInputState()` 已从 focus-lost 流程抽取，供 context push/pop 与失焦路径复用；该 helper 不触发 `FocusLostEvent`，窗口失焦/最小化路径仍显式派发事件。
- `GameScene`、`PauseMenuScene`、`SaveSlotSelectScene`、`RestDialogScene`、`BattleScene` 已在 `init()/clean()` 中接入 context push/pop，并通过 `context_pushed_` 保证成对清理；`TitleScene` 保持空栈回退路径。
- `Gameplay` / `Menu` / `Dialogue` / `Battle` 的运行时白名单已落地，其中 `Dialogue` 与 `Battle` 当前按计划保持空 context，只负责阻断 Gameplay 输入，等待 Phase 4 再接入 `menu_*` 动作。
- 自动化验证已通过：
  - `ninja -C build/debug engine_tests game_tests`
  - `./build/debug/tests/engine_tests`：192/192 通过
  - `./build/debug/tests/game_tests`：184 通过，6 个 headless/RmlUi 相关用例按预期跳过

#### 完成标准

- Gameplay 和 Menu 场景的输入边界清晰：在菜单中按 WASD 不会移动角色。
- push/pop 配对正确，无栈泄漏。
- 上下文切换时不残留旧的动作 HELD 状态，也不残留旧 context 的物理按键缓存。
- 现有 modal scene（Pause / SaveSlotSelect / RestDialog / Battle）都和 scene 栈保持一致，不会出现 context 栈漂移。
