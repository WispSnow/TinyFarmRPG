### Phase 4: UI 导航 - `menu_*` 动作 + RmlUI 菜单桥接

**目标**：让当前基于 RmlUI 的菜单类 scene 可以只用键盘/手柄完成导航和确认，不再依赖鼠标悬停与点击；同时统一菜单输入语义，避免 SDL 原始键盘事件与 `menu_*` 语义桥接双重触发。

**前置**：Phase 3 已完成，`InputContext` 栈可用。但当前运行时仍是：

- `Menu` context 只允许 `pause`
- `TitleScene` 仍走空栈 legacy 回退
- `PauseMenuScene` / `SaveSlotSelectScene` 仍监听 `pause`

Phase 4 的第一步就是把这些临时状态迁移到正式的菜单语义动作。

---

#### 设计要点

**本阶段覆盖范围**

只覆盖当前已经使用 **RmlUI** 的菜单类 scene：

- `TitleScene`
- `PauseMenuScene`
- `SaveSlotSelectScene`
- `RestDialogScene`
- `BattleScene`

**不在本阶段范围内**：

- `GameScene` 内部的 `InventoryUI` / `HotbarUI`
- 任何自研非 RmlUI 导航树
- 菜单按住后的自动连发/重复导航

---

**RmlUI 导航前提：`nav-*` 与 `:focus`**

RmlUI 的方向键导航不是“只要 `ProcessKeyDown(Up/Down)` 就会自动工作”。要让内置导航生效，当前聚焦元素必须具备 `nav-up/down/left/right` 属性。

本阶段采用 **RCSS 统一声明** 方案，而不是自己重写导航算法：

- 在 `ui/rmlui/theme/menu_widgets.rcss` 中，为 `.tf-button-primary`、`.tf-button-secondary`、`.tf-icon-button` 统一补上：
  - `nav-up: auto`
  - `nav-down: auto`
  - `nav-left: auto`
  - `nav-right: auto`
- 这会启用 RmlUI 内置的几何启发式导航；当前菜单布局先以 `auto` 为默认方案
- 如果个别 scene 的布局让 `auto` 导航结果不理想，再在对应 RML 元素上补显式 `nav-*`

补充说明：

- `GetLocalProperty()` 会读取元素 definition 中由 RCSS 规则提供的属性，因此共用 class rule 足够，不需要把 `nav-*` 内联写到每个按钮上
- 当前菜单按钮缺少 `:focus` 样式；Phase 4 必须在 `menu_widgets.rcss` 中为同一组按钮补可见的 `:focus` 高亮，避免键盘/手柄导航时“有焦点但看不见”

**Tab 键策略**：

- `Tab` / `Shift+Tab` 保持走现有原始 RmlUI 路由
- `Tab` 不纳入 `menu_*` 语义动作，也不纳入 suppress 范围
- 原因很简单：RmlUI 对表单/焦点控件原生就支持 Tab 导航，保留它对键盘用户更自然

---

**菜单语义动作**

| 动作名 | 键盘绑定 | 手柄绑定 |
|--------|---------|---------|
| `menu_up` | `W`, `Up` | `GamepadDpadUp`, `LeftStickUp` |
| `menu_down` | `S`, `Down` | `GamepadDpadDown`, `LeftStickDown` |
| `menu_left` | `A`, `Left` | `GamepadDpadLeft`, `LeftStickLeft` |
| `menu_right` | `D`, `Right` | `GamepadDpadRight`, `LeftStickRight` |
| `menu_confirm` | `Return`, `Space` | `GamepadSouth` |
| `menu_cancel` | `Escape` | `GamepadEast` |

`menu_*` 与 `move_*` 共享部分物理键是允许的。它们由 `InputContext` 隔离：

- `Gameplay` context 中，`menu_*` 不激活
- `Menu` / `Dialogue` / `Battle` context 中，`move_*` 不激活

---

**上下文迁移**

Phase 4 结束后，各 context 的动作白名单应统一为：

| Context | 允许的动作 |
|---------|-----------|
| `Gameplay` | 保持 Phase 3 的世界动作：`move_*`, `primary_action`, `secondary_action`, `interact`, `hotbar_*`, `pause`, `inventory`, `rotate_*`, `player_light`, `camera_reset_zoom` |
| `Menu` | `menu_up`, `menu_down`, `menu_left`, `menu_right`, `menu_confirm`, `menu_cancel` |
| `Dialogue` | `menu_up`, `menu_down`, `menu_left`, `menu_right`, `menu_confirm`, `menu_cancel` |
| `Battle` | `menu_up`, `menu_down`, `menu_left`, `menu_right`, `menu_confirm`, `menu_cancel` |

约束：

- `pause` 保持为 **Gameplay / world** 语义，不再在菜单类 context 中复用
- `PauseMenuScene`、`SaveSlotSelectScene` 的“返回/关闭”监听从 `pause` 迁移为 `menu_cancel`
- `TitleScene` 在本阶段正式接入 `Menu` context，替代 Phase 3 的空栈例外路径

---

**单一路由策略**

Phase 4 必须明确菜单导航只有一条生效路径，不能同时依赖：

1. `SDL_Event -> RmlUi::InputEventHandler` 的原始键盘路由
2. `SDL_Event -> InputManager -> menu_* -> RmlUI bridge` 的语义动作路由

否则在 `Menu` / `Dialogue` / `Battle` context 中，键盘方向键、回车、空格、Esc 可能被处理两次。

本阶段统一策略：

- **菜单导航键**（`menu_up/down/left/right/confirm/cancel` 对应的键盘按键）在菜单类 context 中改走 **语义动作路由**
- 鼠标输入继续走现有原始 RmlUI 路由
- 非菜单导航键继续走现有原始 RmlUI 路由
- 当前 scene 若没有激活菜单类 context，则保持现有原始 SDL -> RmlUI 行为

实现上可在 `InputManager::sampleInputEvents()` 的 RmlUI forward 之前，识别：

- 当前栈顶 context 是否为 `Menu` / `Dialogue` / `Battle`
- 当前 `SDL_Event` 的 scancode 是否命中菜单导航 suppress 集合

命中时，不把这类原始键盘事件再转发给 `rmlui_event_callback_`，而只保留后续 `processEvent()` 产生的 `menu_*` 语义动作。

**suppress 集合不能硬编码**：

- `InputManager` 不应手写一组固定 scancode
- 应在输入配置加载完成后，根据 `menu_up/down/left/right/confirm/cancel` 的**实际键盘绑定**反查生成一个运行时集合，例如 `menu_navigation_scancodes_for_rmlui_`
- `sampleInputEvents()` 只在“当前 context 是 menu-like 且 scancode 命中该集合”时 suppress 原始 RmlUI forward

这样 suppress 逻辑会自动跟随 `config/input.json` 变化，不会和配置漂移。

**文本输入例外**：

- 当前这些菜单 scene 没有文本输入控件，因此 Phase 4 可以先不做 focused text field bypass
- 如果后续某个 RmlUI 菜单加入 `input` / `textarea`，则在 suppress 原始键盘路由前，必须先补“文本输入控件聚焦时放行原始键盘事件”的分支

---

**导航控制器模型**

不在 scene 内逐帧轮询 `InputManager`，而是采用 **事件驱动**：

```cpp
[InputManager]
  |- onAction(menu_up/down/left/right/menu_confirm, PRESSED)
  |      -> [UINavigationController]
  |      -> [RmlUILayer bridge]
  |
  `- onAction(menu_cancel, PRESSED)
         -> [Pause/SaveSlot/RestDialog 等 scene 自己处理]
```

`UINavigationController` 只负责把 `menu_up/down/left/right/menu_confirm` 的 **PRESSED** 语义转成导航信号：

- 不读取 HELD
- 不实现自动连发
- 不直接依赖某个具体 scene

**归属建议**：由 `GameApp` 持有单个 `UINavigationController`。

理由：

- `GameApp` 已同时持有 `InputManager` 与 `GLRenderer/RmlUILayer`
- 不需要把 RmlUI 依赖塞回 `InputManager`
- 也不需要让每个 scene 各自管理一份导航控制器
- 生命周期上不要依赖成员声明顺序，`GameApp::close()` 中应显式 reset/controller 断开，再销毁 `input_manager_` / `gl_renderer_`

---

**RmlUI 桥接与焦点策略**

在补齐 `nav-*` RCSS 基础后，RmlUI 已有可用的键盘焦点导航能力，本阶段不重复实现一套导航算法。桥接层只做“语义动作 -> RmlUI 键盘语义”的最小翻译：

- `menu_up` -> `ProcessKeyDown(Up)`
- `menu_down` -> `ProcessKeyDown(Down)`
- `menu_left` -> `ProcessKeyDown(Left)`
- `menu_right` -> `ProcessKeyDown(Right)`
- `menu_confirm` -> `ProcessKeyDown(Return)`

`menu_confirm` 本阶段不计划额外 fallback。当前使用的 `button` 都是 focusable form control，`ProcessKeyDown(Return)` 已足以触发 focused button 的 click 语义。

`menu_cancel` 不应依赖全局 `ProcessKeyDown(Escape)` 作为主语义。原因：

- `PauseMenuScene` / `SaveSlotSelectScene` 的返回语义是 scene 栈操作
- `RestDialogScene` 的取消语义是关闭当前模态
- `BattleScene` 当前没有统一的“Esc = 某个按钮”语义

当前 scene RML 中也没有依赖原始 `Escape` / `keydown` / `keyup` 的菜单逻辑，因此把 `Escape` 纳入 `menu_cancel` suppress 范围在 Phase 4 是安全的；后续若某个菜单引入原始按键处理，再单独评估例外策略。

因此本阶段的主约束是：

- **方向导航 + confirm** 走 `UINavigationController -> RmlUI bridge`
- **cancel/back** 由 scene 继续通过 `InputManager::onAction(menu_cancel, PRESSED)` 自己处理

**焦点策略必须显式化**：

- 文档加载完成后，主动聚焦第一个主操作控件，不能依赖鼠标 hover
- 弹出 confirm/modal 时，主动聚焦默认按钮
- modal 关闭后，恢复到父层之前的焦点；若旧焦点已失效，再回退到该 scene 的默认焦点
- 对于通过 data model 改变 DOM/可见性的目标，focus 请求不能和 `markDirty()` 同步立即执行；需要由 `RmlUILayer` 提供一个 **deferred focus helper**，在下一次 `context_->Update()` 之后再应用

建议在 RML 文档上补充稳定的默认焦点标记（`id` 或 `data-nav-default` 一类），不要依赖 DOM 顺序猜测。

本阶段各 scene 的推荐默认焦点：

- `TitleScene` -> `Start`
- `PauseMenuScene` -> `Resume`
- `SaveSlotSelectScene` -> 第一个可用 slot；若没有则 `Back`
- `SaveSlotSelectScene` confirm modal -> `OK`
- `RestDialogScene` -> 第一个可操作控件（建议 `hours_down` 或 `Confirm`，以实现时手感更自然者为准）
- `BattleScene` -> `Attack`

---

#### 需要新增的文件

- `src/engine/ui/ui_navigation_controller.h`
- `src/engine/ui/ui_navigation_controller.cpp`
- `tests/engine/ui/ui_navigation_controller_test.cpp`

#### 需要修改的文件

- `config/input.json` — 添加 `menu_*` 动作
- `ui/rmlui/theme/menu_widgets.rcss` — 菜单按钮 `nav-*` 与 `:focus` 样式
- `src/engine/input/input_manager.h` — 如需声明菜单原始事件 suppress helper
- `src/engine/input/input_manager.cpp` — context 白名单迁移、按配置生成 suppress scancode 集合、菜单原始键盘路由 suppress
- `src/engine/core/game_app.h` — 持有 `UINavigationController`
- `src/engine/core/game_app.cpp` — 创建/显式销毁控制器并接入 RmlUI bridge
- `src/engine/ui/rmlui/rml_ui_layer.h` — 暴露最小导航/聚焦 helper
- `src/engine/ui/rmlui/rml_ui_layer.cpp` — `ProcessKeyDown` 桥接、焦点保存/恢复、deferred focus helper
- `src/game/scene/title_scene.h` — 接入 `Menu` context 所需状态
- `src/game/scene/title_scene.cpp` — `Menu` context + 默认焦点
- `src/game/scene/pause_menu_scene.h` — `menu_cancel` 回调声明
- `src/game/scene/pause_menu_scene.cpp` — `pause -> menu_cancel` 迁移 + 默认焦点
- `src/game/scene/save_slot_select_scene.h` — `menu_cancel` / modal 焦点状态
- `src/game/scene/save_slot_select_scene.cpp` — `pause -> menu_cancel` 迁移 + modal 焦点恢复
- `src/game/scene/rest_dialog_scene.h` — `menu_cancel` 回调声明（如采用 scene 直连）
- `src/game/scene/rest_dialog_scene.cpp` — 菜单导航接入 + 默认焦点 + cancel
- `src/game/scene/battle_scene.cpp` — 菜单导航接入 + 默认焦点
- `ui/rmlui/scenes/title.rml` — 默认焦点标记
- `ui/rmlui/scenes/pause_menu.rml` — 默认焦点标记
- `ui/rmlui/scenes/save_slot_select.rml` — 默认焦点标记 / modal 默认焦点标记
- `ui/rmlui/scenes/rest_dialog.rml` — 默认焦点标记
- `ui/rmlui/scenes/battle.rml` — 默认焦点标记
- `tests/engine/input/input_manager_rmlui_routing_test.cpp` — 菜单 context 双路由抑制 / suppress-set 测试
- `tests/game/input_context_scene_stack_test.cpp` — `menu_cancel` 叠层恢复测试

---

#### Step 4.0: 补齐 RmlUI 菜单导航基础样式

- 在 `ui/rmlui/theme/menu_widgets.rcss` 中为 `.tf-button-primary`、`.tf-button-secondary`、`.tf-icon-button` 增加：
  - `nav-up/down/left/right: auto`
  - 明确可见的 `:focus` 样式
- 当前先统一使用 `auto` 导航；如某个具体菜单的几何布局让自动导航不稳定，再在对应 RML 上做定点覆写
- `Tab` / `Shift+Tab` 保持原始 RmlUI 路由，不参与本阶段 suppress

#### Step 4.1: 添加 `menu_*` 动作并迁移 context 白名单

- 在 `config/input.json` 与 `defaultMappings()` 中加入 `menu_up/down/left/right/confirm/cancel`
- 更新 `initializeContextDefinitions()`：
  - `Menu` 从只允许 `pause` 改为正式的 `menu_*`
  - `Dialogue` / `Battle` 加入同一套菜单语义动作
- 保持 `pause` 只属于 `Gameplay`
- `TitleScene` 在本阶段接入 `Menu` context，不再依赖空栈 legacy 回退

#### Step 4.2: 统一菜单输入路由，消除双触发

- 在输入映射初始化后，根据 `menu_*` 的实际键盘绑定构建运行时 suppress scancode 集合
- 在 `InputManager::sampleInputEvents()` 中补一个“菜单类 context 下的原始键盘导航 suppress”分支
- 被 suppress 的只应是：
  - suppress 集合命中的 **键盘** down/up 事件
- 不要 suppress：
  - 鼠标事件
  - 非菜单键盘事件
  - `Tab` / `Shift+Tab`
  - 非菜单类 context 下的原始事件
- 目标是让菜单导航只经由 `menu_*` 语义动作进入 RmlUI

#### Step 4.3: 实现事件驱动的 `UINavigationController`

- 新增 `UINavigationController`，在构造/绑定阶段订阅：
  - `menu_up`
  - `menu_down`
  - `menu_left`
  - `menu_right`
  - `menu_confirm`
- 只在 `PRESSED` 时发出导航信号
- 不在 Phase 4 处理 HELD repeat
- 由 `GameApp` 创建单例并在关闭时显式断开绑定

#### Step 4.4: RmlUI bridge 与焦点 helper

- 在 `RmlUILayer` 中封装最小桥接 helper：
  - 方向键导航
  - confirm 触发
  - 聚焦默认元素
  - 记录/恢复 modal 前焦点
- 延迟到下一次 `context_->Update()` 之后执行的 deferred focus 请求
- 采用 `ProcessKeyDown` 作为主路径
- 不把 `menu_cancel` 做成全局 RmlUI `Escape` 翻译；scene 级返回语义优先
- `GameApp::close()` 中显式销毁/断开 `UINavigationController`，不依赖成员析构顺序

#### Step 4.5: Scene 接入

- `TitleScene`
  - `init()/clean()` 中 push/pop `Menu`
  - 文档加载后默认焦点到 `Start`
- `PauseMenuScene`
  - 返回/关闭逻辑从 `pause` 迁移到 `menu_cancel`
  - 默认焦点到 `Resume`
- `SaveSlotSelectScene`
  - 返回/关闭逻辑从 `pause` 迁移到 `menu_cancel`
  - 默认焦点到首个可用 slot 或 `Back`
  - overwrite confirm modal 打开后通过 deferred focus 在下一次 `RmlUI::Update()` 后聚焦 `OK`
  - modal 关闭后恢复上一焦点；若旧焦点失效则退回默认焦点
- `RestDialogScene`
  - 支持方向导航与 confirm
  - `menu_cancel` 直接关闭对话
- `BattleScene`
  - 支持动作按钮的方向导航与 confirm
  - 默认焦点到 `Attack`

#### Step 4.6: 自动化测试与人工验证

自动化测试至少覆盖：

| 测试用例 | 说明 |
|---------|------|
| `MenuContextSuppressesRawKeyboardNavigationToRmlUi` | 菜单类 context 下，方向键/回车/Esc 不再以原始键盘事件重复转发到 RmlUI |
| `GameplayContextKeepsLegacyRawRmlUiForwarding` | 非菜单类 context 不改变原始事件转发行为 |
| `MenuSuppressSetFollowsConfiguredBindings` | 菜单 suppress scancode 集合来自 `menu_*` 的真实配置，而不是硬编码 |
| `NavigationControllerEmitsOnlyOnPressed` | `UINavigationController` 只响应 `PRESSED` |
| `NavigationControllerDoesNotRepeatOnHeld` | Phase 4 不引入 HELD 自动连发 |
| `MenuWidgetsExposeNavAutoAndFocusStyle` | 菜单按钮公共 RCSS 已提供 `nav-*: auto` 和可见 `:focus` 样式 |
| `StackedMenuCancelReturnsToLowerScene` | `PauseMenuScene -> SaveSlotSelectScene` 叠层时，`menu_cancel` 先关闭上层，pop 后恢复下层菜单上下文 |
| `SaveSlotConfirmFocusIsDeferredAfterModalOpen` | overwrite confirm 的焦点在 DOM 更新后再应用，不依赖同帧 `markDirty() + Focus()` |
| `TitleSceneUsesMenuContext` | `TitleScene` 已接入 `Menu` context，而不是继续依赖空栈 |

补充说明：

- `tests/engine/input/input_manager_rmlui_routing_test.cpp` 已覆盖 RmlUI raw forward 语义，可直接扩展
- `tests/game/input_context_scene_stack_test.cpp` 已有 scene 栈/上下文基础设施，可迁移到 `menu_cancel`
- 若 headless RmlUI 环境允许，补一个 `PauseMenuScene` / `SaveSlotSelectScene` 焦点 smoke test；若环境不稳定，则保留为人工验证项

人工验证必须覆盖：

1. `TitleScene` 可仅用键盘/手柄启动、读档、打开菜单、退出
2. `PauseMenuScene` 可导航 `Resume/Save/Load/Title`，且 `menu_cancel` 可返回
3. `PauseMenuScene -> SaveSlotSelectScene` 叠层中，`menu_cancel` 先退上层，再退下层
4. `SaveSlotSelectScene` overwrite confirm modal 的默认焦点与关闭后焦点恢复正确
5. `RestDialogScene` 可调整小时并确认/取消
6. `BattleScene` 可仅用键盘/手柄选择行动按钮
7. 菜单导航不会出现一次按键跳两项的双触发
8. `Tab` / `Shift+Tab` 在菜单里仍可工作
9. 焦点高亮在键盘/手柄导航时始终可见

---

#### 待办清单

- [ ] 添加 `menu_*` 动作配置
- [ ] 为菜单按钮公共 RCSS 添加 `nav-*: auto`
- [ ] 为菜单按钮公共 RCSS 添加可见 `:focus` 样式
- [ ] 迁移 `Menu` / `Dialogue` / `Battle` context 白名单
- [ ] 让 `TitleScene` 正式接入 `Menu` context
- [ ] 将菜单类 context 的原始键盘导航统一为单一路由
- [ ] 让 suppress scancode 集合跟随 `menu_*` 配置自动生成
- [ ] 实现事件驱动的 `UINavigationController`
- [ ] 在 `RmlUILayer` 中封装导航/聚焦 helper
- [ ] 在 `RmlUILayer` 中封装 deferred focus helper
- [ ] 将 `PauseMenuScene` / `SaveSlotSelectScene` 的返回语义从 `pause` 迁移到 `menu_cancel`
- [ ] 为 `Title` / `Pause` / `SaveSlotSelect` / `RestDialog` / `Battle` 补默认焦点策略
- [ ] 为 `SaveSlotSelectScene` confirm modal 补焦点恢复
- [ ] 编写导航控制器测试
- [ ] 编写菜单 raw-routing 抑制测试
- [ ] 编写 scene 栈 `menu_cancel` 恢复测试
- [ ] 完成人工回归验证

#### 完成标准

- 当前 RmlUI 菜单类 scene（`Title` / `Pause` / `SaveSlotSelect` / `RestDialog` / `Battle`）可仅用键盘或手柄完成导航和确认
- 菜单类 context 中不存在 SDL 原始键盘导航与 `menu_*` 语义桥接的双重触发
- `pause` 不再在菜单类 context 中复用，返回/取消统一迁移到 `menu_cancel`
- `PauseMenuScene -> SaveSlotSelectScene` 叠层返回行为正确
- `TitleScene` 已脱离空栈 legacy 输入路径，正式纳入 `Menu` context
- 菜单焦点在键盘/手柄导航时有稳定可见的 `:focus` 高亮
- `Tab` / `Shift+Tab` 仍保持可用
- 本阶段不宣称覆盖自研 UI；`InventoryUI` / `HotbarUI` 等留待后续单独规划
