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

**控制器目标模型**（非鼠标模拟）：

当 `last_input_device_ == Gamepad` 时，目标从鼠标世界坐标切换到基于角色朝向的目标格：
- 默认目标 = 角色当前朝向的前方 1 格（与 `InteractionSystem::chooseFacingTarget` 同源）
- 仍受 `TOOL_TARGET_TILE_RANGE` 范围约束
- 目标光标 sprite 在手柄模式下锁定到目标格中心，不跟随鼠标

可选：右摇杆微调目标偏移（每次推动移动 1 格），但 MVP 阶段可以先只做朝向前方格。

**输入设备切换**：
- 任何键盘/鼠标事件 → `last_input_device_ = KeyboardMouse`
- 任何手柄事件 → `last_input_device_ = Gamepad`
- 切换时目标光标立即跟随对应模式，无过渡动画

---

#### 需要修改的文件

- `src/game/system/player_control_system.h` — 新增控制器目标逻辑
- `src/game/system/player_control_system.cpp` — 重构输入读取路径
- `src/game/system/interaction_system.cpp` — 统一目标读取（如需）
- `src/game/scene/game_scene.cpp` — 动作名迁移
- `src/game/scene/pause_menu_scene.cpp` — 动作名迁移
- `config/input.json` — 替换旧动作名、添加新动作

#### 需要新增的文件

- `tests/game/player_control_system_gamepad_test.cpp`

注意：控制器目标逻辑直接作为 `PlayerControlSystem` 的私有方法实现，不单独建文件。若后续复杂度增长再考虑抽离。

---

#### Step 2.1: 引入语义动作并替换旧动作

- 在 `config/input.json` 中：
  - `mouse_left` → 重命名为 `primary_action`，绑定 `["MouseLeft", "GamepadSouth"]`
  - `mouse_right` → 重命名为 `secondary_action`，绑定 `["MouseRight", "GamepadEast"]`
  - 新增 `hotbar_prev`，绑定 `["GamepadLeftShoulder"]`
  - 新增 `hotbar_next`，绑定 `["GamepadRightShoulder"]`
  - （注意：`rotate_left/right` 原本绑 Q/E + LB/RB，需要调整——LB/RB 给快捷栏后，手柄旋转可绑其他键或取消）
- 更新 `defaultMappings()` 同步。
- 全局搜索 `"mouse_left"` / `"mouse_right"` 的 `entt::hashed_string` 引用，全部迁移到新名称。
- `InputManager::initializeMappings` 中 `mouse_left` / `mouse_right` 的自动注入逻辑删除（不再需要隐式默认鼠标动作）。

#### Step 2.2: 重构 PlayerControlSystem 输入读取路径

- 主操作回调从 `onAction("mouse_left"_hs)` 迁移到 `onAction("primary_action"_hs)`。
- 取消/清除选择从 `onAction("mouse_right"_hs)` 迁移到 `onAction("secondary_action"_hs)`。
- 新增 `hotbar_prev` / `hotbar_next` 回调，实现快捷栏循环切换。

#### Step 2.3: 实现控制器目标逻辑

在 `PlayerControlSystem` 中新增私有方法：

```cpp
/// 根据当前输入设备返回有效的目标 tile 世界坐标
glm::vec2 resolveEffectiveTarget() const;
```

逻辑：
- `KeyboardMouse` 模式：沿用 `camera_.screenToWorld(input_manager_.getLogicalMousePosition())`
- `Gamepad` 模式：取角色当前朝向前方 1 格的 tile 中心坐标

调用方（`onPrimaryAction` / `updateTargetAndSelection`）统一使用 `resolveEffectiveTarget()` 替代直接读取鼠标坐标。

#### Step 2.4: 目标光标显示

- `KeyboardMouse` 模式：光标跟随鼠标世界坐标（现有行为）
- `Gamepad` 模式：光标锁定到 `resolveEffectiveTarget()` 返回的 tile 中心
- 无活动工具/种子时，两种模式下都隐藏光标（现有行为）

#### Step 2.5: 验证

确保以下操作在手柄和键鼠两种路径下都能正常工作：
- 移动（Phase 1 已覆盖）
- 工具使用（锄地、浇水）
- 种植
- 交互（与 NPC/宝箱/休息点，`InteractionSystem` 本身已基于朝向，天然兼容手柄）
- 快捷栏切换（数字键直达 + prev/next 循环）
- 取消选择

---

#### 待办清单

- [ ] 替换 `mouse_left` → `primary_action`，`mouse_right` → `secondary_action`
- [ ] 新增 `hotbar_prev` / `hotbar_next` 动作和回调
- [ ] 删除 `initializeMappings` 中 `mouse_left/right` 自动注入逻辑
- [ ] 实现 `resolveEffectiveTarget()` 双模式目标解析
- [ ] 统一 `PlayerControlSystem` 中所有目标读取路径
- [ ] 更新目标光标显示逻辑
- [ ] 调整 `rotate_left/right` 手柄绑定（LB/RB 让位给 hotbar_prev/next）
- [ ] 编写 `PlayerControlSystem` 手柄路径测试

#### 完成标准

- 手柄可自然完成移动、操作、取消和快捷栏切换。
- 世界操作逻辑不再把鼠标动作当成唯一入口。
- 键鼠和手柄共享同一套核心行为分发路径。
- 代码中不再存在 `mouse_left` / `mouse_right` 动作名引用。
