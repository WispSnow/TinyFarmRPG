### Phase 2: 玩法语义重构 - 语义动作 + Controller 目标模型

**目标**：让游戏世界操作不再依赖鼠标专用动作，使手柄拥有自然的世界交互路径。

#### 实现思路

- 把玩法层从“物理设备动作”切到“语义动作”。
- 把当前鼠标目标逻辑升级为“双输入模式”：
  - 键鼠继续使用鼠标目标
  - 手柄使用控制器目标模型
- 把主操作、取消操作、快捷栏切换统一到语义动作，避免世界逻辑继续依赖 `mouse_left` / `mouse_right` / 数字键直达。

#### 需要新增的文件

- `src/game/input/controller_target_model.h`
- `src/game/input/controller_target_model.cpp`
- `tests/game/player_control_system_gamepad_test.cpp`

#### 需要修改的文件

- `src/game/system/player_control_system.h`
- `src/game/system/player_control_system.cpp`
- `src/game/system/interaction_system.cpp`
- `src/game/scene/game_scene.cpp`
- `src/game/scene/pause_menu_scene.cpp`
- `config/input.json`

#### Step 2.1: 引入玩法语义动作

- 正式引入并消费以下动作：
  - `primary_action`
  - `secondary_action`
  - `hotbar_prev`
  - `hotbar_next`
- 保留旧动作作为底层兼容映射，但不再让核心玩法直接依赖它们。

#### Step 2.2: 重构 PlayerControlSystem 输入读取路径

- 把左键主操作迁到 `primary_action`。
- 把右键取消操作迁到 `secondary_action`。
- 快捷栏切换优先使用 `hotbar_prev` / `hotbar_next`，数字键直达保留给键盘路径。

#### Step 2.3: 引入控制器目标模型

- 新增控制器目标状态，保存当前目标格或目标偏移。
- 默认目标跟随角色朝向前方格子。
- 允许使用右摇杆或方向输入调整目标，但范围仍受 `TOOL_TARGET_TILE_RANGE` 限制。

#### Step 2.4: 融合键鼠与手柄双模式

- 最近输入来源是键鼠时，继续使用鼠标世界坐标。
- 最近输入来源是手柄时，切到控制器目标。
- 工具、种子、交互命中逻辑统一读取“当前有效目标”，而不是直接依赖鼠标。

#### Step 2.5: 覆盖世界操作验证

- 验证移动、工具使用、种植、交互、快捷栏切换都可从手柄路径触发。
- 确保键鼠路径不因重构退化。

#### 待办清单

- [ ] 引入 `primary_action` / `secondary_action`
- [ ] 引入 `hotbar_prev` / `hotbar_next`
- [ ] 重构 `PlayerControlSystem` 的输入读取路径
- [ ] 新增控制器目标模型
- [ ] 在键鼠/手柄之间切换当前有效目标
- [ ] 统一工具/种子/交互命中读取路径
- [ ] 增加 `PlayerControlSystem` 手柄测试

#### 完成标准

- 手柄可自然完成移动、操作、取消和快捷栏切换。
- 世界操作逻辑不再把鼠标动作当成唯一入口。
- 键鼠和手柄共享同一套核心行为分发路径。
