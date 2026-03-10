### Phase 2: 玩法语义重构 - 语义动作 + 控制器目标模型

**目标**：让游戏世界操作不再依赖鼠标专用动作，使手柄拥有自然的世界交互路径。

---

#### 设计要点

**直接替换，不保留旧动作名**（项目不考虑向后兼容）：

| 旧动作 | 新语义动作 | 说明 |
|-------|-----------|------|
| `mouse_left` | `primary_action` | 工具使用、种植、主操作 |
| `mouse_right` | `secondary_action` | 取消选择、副操作 |
| _(新增)_ | `hotbar_prev` | 快捷栏向前切换（手柄 LB） |
| _(新增)_ | `hotbar_next` | 快捷栏向后切换（手柄 RB） |

`hotbar_1..10` 数字键直达保留给键盘路径，手柄通过 `hotbar_prev/next` 循环切换。
`hotbar_prev/next` 直接复用现有 `HotbarComponent::active_slot_index_` 与 `switchToHotbarSlot()`，不新增一套热栏命令流。

**控制器目标模型**（非鼠标模拟）：

当 `last_input_device_ == Gamepad` 时，目标从鼠标世界坐标切换到基于角色朝向的目标格：
- 默认目标 = 角色朝向前方 1 格（与 `InteractionSystem::chooseFacingTarget` 的“面前一格”语义保持一致）
- 为避免“本 tick 正在推方向 + 同 tick 按下主操作”时朝向滞后一帧，手柄模式下优先取**当前 move intent 推导方向**；若当前无 move intent，再退回 `StateComponent::direction_`
- 仍统一走现有目标范围校验，不另开一套距离规则
- 目标位置语义保持与现有 `TargetComponent` / `RenderTargetSystem` / 动画事件一致：使用**目标 tile 的 world pos（tile rect.pos）**，不是 tile 中心
- 目标光标 sprite 在手柄模式下锁定到该目标格，不跟随鼠标

本阶段**不做**右摇杆微调目标偏移；若后续确有需要，再放到后续阶段单独扩展。

**输入设备切换**：
- 延续 Phase 1 语义：只在**有意义输入**发生时切换 `last_input_device_`
  - 键盘：`KEY_DOWN`
  - 鼠标：`MOUSE_MOTION` / `MOUSE_BUTTON_DOWN` / `MOUSE_WHEEL`
  - 手柄：`GAMEPAD_BUTTON_DOWN`，或轴方向首次越过 press threshold
- 切换时目标光标立即跟随对应模式，无过渡动画

**UI 边界（本阶段明确不扩 scope）**：
- Phase 2 不引入通用 gameplay/UI 输入上下文，也不实现手柄 UI 导航
- 因此 `primary_action` / `secondary_action` / `hotbar_prev` / `hotbar_next` 仍然是世界玩法动作
- 若 RmlUI 叠层可见，手柄世界动作仍可能继续生效；这是已知限制，统一留给 Phase 3/4 处理，不在本阶段临时拼接半套 UI capture 逻辑

---

#### 需要修改的文件

- `src/game/system/player_control_system.h` — 新增控制器目标逻辑
- `src/game/system/player_control_system.cpp` — 重构输入读取路径
- `src/engine/input/input_manager.cpp` — 删除 `mouse_left` / `mouse_right` 的隐式默认注入
- `config/input.json` — 替换旧动作名、添加新动作
- `tests/game/player_control_system_targeting_test.cpp` — 扩展手柄路径与目标解析测试
- `tests/engine/input/input_manager_test.cpp` — 默认鼠标动作测试改为显式语义动作
- 受影响的最小输入配置测试 fixture（动作名迁移）：
  - `tests/game/dialogue_bubble_controller_test.cpp`
  - `tests/game/map_manager_async_preload_test.cpp`
  - `tests/game/save_service_async_test.cpp`
  - `tests/game/ui_layout_integration_test.cpp`
  - `tests/game/world/async_preload_pipeline_test.cpp`

注意：控制器目标逻辑直接作为 `PlayerControlSystem` 的私有方法实现，不单独建文件。测试优先扩展现有 `tests/game/player_control_system_targeting_test.cpp`；若后续用例膨胀再拆分。

---

#### Step 2.1: 引入语义动作并替换旧动作

- 在 `config/input.json` 中：
  - `mouse_left` → 重命名为 `primary_action`，绑定 `["MouseLeft", "GamepadSouth"]`
  - `mouse_right` → 重命名为 `secondary_action`，绑定 `["MouseRight", "GamepadEast"]`
  - 新增 `hotbar_prev`，绑定 `["GamepadLeftShoulder"]`
  - 新增 `hotbar_next`，绑定 `["GamepadRightShoulder"]`
  - `rotate_left/right` 当前实际仅保留键盘绑定，无需额外为 LB/RB 让位
- 更新 `defaultMappings()` 同步。
- 全局搜索 `"mouse_left"` / `"mouse_right"` 的 `entt::hashed_string` 引用和测试配置，全部迁移到新名称。
- `InputManager::initializeMappings` 中 `mouse_left` / `mouse_right` 的自动注入逻辑删除（不再需要隐式默认鼠标动作）。

#### Step 2.2: 重构 PlayerControlSystem 输入读取路径

- 主操作回调从 `onAction("mouse_left"_hs)` 迁移到 `onAction("primary_action"_hs)`，并将回调命名同步整理为 `onPrimaryAction()`。
- 取消/清除选择从 `onAction("mouse_right"_hs)` 迁移到 `onAction("secondary_action"_hs)`，并将回调命名同步整理为 `onSecondaryAction()`。
- 新增 `hotbar_prev` / `hotbar_next` 回调，基于当前 `active_slot_index_` 做环形切换：
  - `prev = (active - 1 + SLOT_COUNT) % SLOT_COUNT`
  - `next = (active + 1) % SLOT_COUNT`
- `hotbar_1..10` 的键盘直达逻辑保留，不和 `hotbar_prev/next` 合并。
- `InteractionSystem` 本阶段不改行为：它本来就是基于朝向的交互路径，Phase 2 只做玩法主/副操作与目标模型重构。

#### Step 2.3: 实现控制器目标逻辑

建议将当前“鼠标 world → tile 目标”逻辑重构为设备无关的两层 helper，而不是把所有判断塞进一个函数里：

```cpp
[[nodiscard]] glm::vec2 computeMouseWorldPosition() const;
[[nodiscard]] std::optional<glm::vec2> resolveTargetTilePositionFromWorld(glm::vec2 world_pos) const;
[[nodiscard]] std::optional<glm::vec2> resolveEffectiveTargetTilePosition() const;
```

逻辑：
- `KeyboardMouse` 模式：沿用 `computeMouseWorldPosition()`，再交给 `resolveTargetTilePositionFromWorld()`
- `Gamepad` 模式：
  1. 读取当前 move intent
  2. 若 move intent 非零，则按 move intent 推导本次玩法朝向
  3. 否则退回 `StateComponent::direction_`
  4. 取玩家前方 1 格
  5. 返回该 tile 的 world pos（`tile_rect.pos`）
- 两种模式最终都返回同一种“tile 原点”语义，供 `TargetComponent`、动画事件和农务系统复用

调用方（`onPrimaryAction()` / `updateTargetAndSelection()`）统一使用 `resolveEffectiveTargetTilePosition()`，不要再在点击路径里直接拼鼠标专用逻辑。

#### Step 2.4: 目标光标显示

- `KeyboardMouse` 模式：光标跟随鼠标解析出来的目标 tile
- `Gamepad` 模式：光标锁定到 `resolveEffectiveTargetTilePosition()` 返回的目标 tile
- 无活动工具/种子时，两种模式下都隐藏光标（现有行为）
- 若目标超出范围或无有效 tile，两种模式下都隐藏光标
- 输入设备切换后的下一次 `PlayerControlSystem::update()` 立即刷新光标位置

#### Step 2.5: 验证

确保以下操作在手柄和键鼠两种路径下都能正常工作：
- 移动（Phase 1 已覆盖）
- 工具使用（锄地、浇水）
- 种植
- 交互（与 NPC/宝箱/休息点，`InteractionSystem` 本身已基于朝向，天然兼容手柄）
- 快捷栏切换（数字键直达 + prev/next 循环）
- 取消选择
- 键鼠 / 手柄切换后目标光标立即切换到对应解析模式
- “同 tick 推动方向 + 主操作”时，不出现朝向滞后一帧

建议测试用例优先补在 `tests/game/player_control_system_targeting_test.cpp`：
- `ToolSelected_GamepadMode_ShowsFacingTileTarget`
- `PrimaryAction_GamepadMode_UsesFacingTileTarget`
- `PrimaryAction_GamepadMode_PrefersCurrentMoveIntentDirection`
- `HotbarPrevNextWrapAround`
- `SecondaryAction_ClearsSelection`
- `MouseAndGamepadSwitch_UpdatesTargetModel`

---

#### 待办清单

- [ ] 替换 `mouse_left` → `primary_action`，`mouse_right` → `secondary_action`
- [ ] 新增 `hotbar_prev` / `hotbar_next` 动作和回调
- [ ] 删除 `initializeMappings` 中 `mouse_left/right` 自动注入逻辑
- [ ] 实现设备无关的目标解析 helper（鼠标 world → tile / gamepad facing → tile）
- [ ] 统一 `PlayerControlSystem` 中所有目标读取路径
- [ ] 更新目标光标显示逻辑
- [ ] 更新依赖旧动作名的测试配置与断言
- [ ] 编写/扩展 `PlayerControlSystem` 手柄路径测试
- [ ] 在文档中保留“手柄 UI capture 留到 Phase 3/4”的已知限制说明

#### 完成标准

- 手柄可自然完成移动、操作、取消和快捷栏切换。
- 世界操作逻辑不再把鼠标动作当成唯一入口。
- 键鼠和手柄共享同一套核心行为分发路径。
- 运行时代码和输入配置中不再存在 `mouse_left` / `mouse_right` 动作名引用。
