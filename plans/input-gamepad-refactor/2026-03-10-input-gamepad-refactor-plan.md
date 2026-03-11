# Input 模块手柄重构计划索引

状态标记：

- `[x]` 已完成
- `[~]` 部分完成
- `[ ]` 未开始

## 当前进度（2026-03-11）

| Phase | 状态 | 文档 |
|------|------|------|
| Phase 1: 输入核心（SDL3 手柄接入 + 轴方向数字化） | `[x]` | [`phase-01.md`](./phase-01.md) |
| Phase 2: 玩法语义重构（语义动作 + 控制器目标模型） | `[x]` | [`phase-02.md`](./phase-02.md) |
| Phase 3: InputContext 上下文分层 | `[x]` | [`phase-03.md`](./phase-03.md) |
| Phase 4: UI 导航（Menu 动作 + 导航控制器） | `[x]` | [`phase-04.md`](./phase-04.md) |
| Phase 5: 后续增强（Buffer / Glyph / Rumble / Rebind） | `[x]` | [`phase-05.md`](./phase-05.md) |
| Phase 6: 输入提示与菜单焦点打磨 | `[ ]` | [`phase-06.md`](./phase-06.md) |

## 目标

- 为 `InputManager` 补齐 SDL3 手柄支持。
- 将输入层从"键鼠特化"整理为"设备无关动作层"。
- 让键盘、鼠标、手柄共用同一套语义动作。
- 为后续 JRPG 菜单、对话、战斗和技能系统预留统一输入基础。

## 全局约束

- 采用最优方案，不考虑向后兼容（CLAUDE.md 明确要求）。
- `InputManager` 仍然是 SDL 事件唯一入口，不新增第二个 poll 点。
- 保留现有动作查询主接口：`onAction()`、`isActionDown()`、`isActionPressed()`、`isActionReleased()`。
- Phase 默认按 `1 -> 6` 顺序执行，不要跳过前置阶段直接做后置特性。
- 每次实施只读取本索引 + 当前 Phase 文档，避免无关上下文占用。
- 只支持单个活动手柄（单人 RPG 场景），多手柄插入时取最近连接的。

## 架构共识（跨 Phase）

- **不过度抽象 binding 层**：运行时保持分离的 per-device map（`key_to_actions_`、`mouse_to_actions_`、`gamepad_button_to_actions_`、`gamepad_axis_to_actions_`），只在解析侧新增 `gamepadButtonFromString` / `gamepadAxisDirectionFromString` 等函数。不引入统一的 `InputBinding` 包装类型。
- **InputDevice 枚举**：当前运行时使用 `enum class InputDevice : uint8_t { Keyboard, Mouse, Gamepad }`，`InputManager` 维护 `last_input_device_`，每次处理输入事件时更新。
- 上层玩法从 `mouse_left` / `mouse_right` / `hotbar_1..10` **直接迁移**到 `primary_action` / `secondary_action` / `hotbar_prev` / `hotbar_next` 等语义动作，旧名称直接删除。
- 手柄目标选择不走"强模拟鼠标"，而走控制器友好的目标模型（基于角色朝向 + 范围约束）。
- 菜单导航通过 `menu_*`、`menu_confirm`、`menu_cancel` 等语义动作驱动。
- `InputContext` 和输入缓冲是后续扩展的基础设施，不提前塞进 Phase 1。
- **测试基础设施**：手柄测试使用 SDL3 `SDL_AttachVirtualJoystick` API 模拟手柄设备，确保 CI 环境无物理手柄时也能运行。

## 使用方式

1. 先读本索引，确认当前阶段和全局约束。
2. 只打开当前要执行的 `phase-0x.md`。
3. 执行完成后，只回写本索引状态和对应 Phase 文档中的待办。
4. 如果某阶段只完成一部分，统一标记为 `[~]`，并在该阶段文档顶部补未完成项。

## 阶段划分说明

- Phase 1：让 SDL3 手柄事件、按钮/轴映射、动作状态流转跑通，配合调试面板验证。
- Phase 2：让手柄自然参与世界操作（移动、工具、交互、快捷栏）。
- Phase 3：引入 InputContext 上下文栈，区分 Gameplay / Menu / Dialogue / Battle 输入域。
- Phase 4：为菜单和 UI 建立手柄导航能力，接入 RmlUI。
- Phase 5：补增强项（输入缓冲、Glyph、震动、重绑定），不阻塞前 4 阶段交付。
- Phase 6：收紧游戏内提示条 HUD，并统一菜单中的 hover/focus/方向导航激活项。

## Phase 1 完成记录（2026-03-10）

- `InputManager` 已支持 SDL3 gamepad 按钮、轴方向数字化、活动手柄切换、设备移除清理和 `last_input_device_` 跟踪。
- `config/input.json` 与默认映射已加入 Phase 1 手柄绑定；调试面板已显示活动手柄、按钮、轴值和最近输入设备。
- 自动化验证已通过：
  - `./build/debug/tests/engine_tests`：185/185 通过
  - `./build/debug/tests/game_tests`：173 通过，5 个 headless RmlUI 相关用例按预期跳过

## Phase 2 完成记录（2026-03-10）

- 世界玩法主/副操作已从 `mouse_left` / `mouse_right` 迁移为 `primary_action` / `secondary_action`。
- 快捷栏已补齐 `hotbar_prev` / `hotbar_next` 手柄环形切换，键盘 `hotbar_1..10` 直达保留。
- `PlayerControlSystem` 已改为设备无关目标模型：键鼠走鼠标目标，手柄走“当前 move intent 优先，否则角色朝向”的面前一格目标。
- 相关测试配置、visual tester 与核心文档已同步迁移。
- 自动化验证已通过：
  - `./build/debug/tests/engine_tests`：185/185 通过
  - `./build/debug/tests/game_tests`：178 通过，5 个 headless RmlUI 相关用例按预期跳过

## Phase 3 完成记录（2026-03-11）

- `InputManager` 已支持 InputContext 栈与运行时白名单过滤，空栈保留 legacy 行为，context 切换会同步清理动作状态与物理 down-state 缓存。
- `GameScene`、`PauseMenuScene`、`SaveSlotSelectScene`、`RestDialogScene`、`BattleScene` 已接入 `init()/clean()` push/pop；`TitleScene` 明确保持空栈路径。
- `processEvent()` 与 `dispatchActionCallbacks()` 已按栈顶 context 过滤，且共享物理键多动作映射、stale down-state、scene 栈恢复等回归点都有测试覆盖。
- 自动化验证已通过：
  - `ninja -C build/debug engine_tests game_tests`
  - `./build/debug/tests/engine_tests`：192/192 通过
  - `./build/debug/tests/game_tests`：184 通过，6 个 headless RmlUI 相关用例按预期跳过

## Phase 4 完成记录（2026-03-11）

- 已新增 `menu_*` / `menu_confirm` / `menu_cancel` 语义动作，并在菜单类 context 下抑制对应原始键盘事件直通 RmlUI。
- 已引入 `UINavigationController`，由 `GameApp` 持有并驱动 `RmlUILayer` 的方向导航、确认和焦点控制。
- `RmlUILayer` 已支持焦点查询、直接聚焦、按 id / class 聚焦，以及在 `context->Update()` 后执行的 deferred focus 队列。
- `TitleScene`、`PauseMenuScene`、`SaveSlotSelectScene`、`RestDialogScene`、`BattleScene` 已接入默认焦点与 `menu_cancel` 语义。
- 菜单样式已补齐 `nav-*` 与 `:focus`，RML 关键按钮已有稳定 id 可供聚焦。
- 自动化验证已通过：
  - `ninja -C build/debug engine_tests game_tests`
  - `./build/debug/tests/engine_tests --gtest_filter='*Input*:*Navigation*'`
  - `./build/debug/tests/game_tests --gtest_filter='*InputContext*:*RmlMenuNavigationStyle*:*TitleSceneMenuButton*:*PauseMenuScene*:*SaveSlotSelectScene*:*BattleSceneSmoke*:*RestAreaInteraction*'`

## Phase 5 完成记录（2026-03-11）

- `InputDevice` 已扩展为 `Keyboard / Mouse / Gamepad` 三值，`BindingDefinition` / `ActionPrompt` / `InputDebugSnapshot` 已成为后续输入展示与重绑定的统一元数据基础。
- `InputManager` 已补齐：
  - per-action `InputBuffer`
  - glyph / prompt 查询
  - rumble 请求与调试状态
  - capture 模式重绑定
  - 绑定持久化与运行时 mapping 重建
- `GameScene` 左下角 overlay 已改为从输入 prompt 动态取词；输入调试面板也已切换到 snapshot 驱动，不再直接依赖旧的 debug map。
- 重绑定流程已覆盖：
  - capture 期间阻断 RmlUI / ImGui 普通输入转发
  - 冲突检测
  - `Escape` 固定物理取消
  - 临时文件 + 备份恢复式落盘
- 自动化验证已通过：
  - `ninja -C build/debug engine_tests game_tests`
  - `./build/debug/tests/engine_tests`：207/207 通过
  - `./build/debug/tests/game_tests`：185 通过，6 个 headless RmlUI 相关用例按预期跳过
