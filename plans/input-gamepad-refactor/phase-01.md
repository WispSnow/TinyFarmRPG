### Phase 1: 输入核心 - SDL3 手柄接入 + 轴方向数字化

**目标**：让输入系统在底层完整支持 SDL3 手柄，复用现有 action 状态机，手柄可驱动已有动作。

---

#### 设计要点

**不新增 binding 抽象层**。运行时保持与键盘/鼠标相同的分离 map 模式：

```cpp
// 新增（与 key_to_actions_ / mouse_to_actions_ 平行）
std::unordered_map<SDL_GamepadButton, std::vector<entt::id_type>> gamepad_button_to_actions_;
std::unordered_map<GamepadAxisDirection, std::vector<entt::id_type>> gamepad_axis_to_actions_;
// 对应 down state
std::unordered_map<SDL_GamepadButton, bool> gamepad_button_down_states_;
std::unordered_map<GamepadAxisDirection, bool> gamepad_axis_down_states_;
// 调试用模拟值缓存（固定轴数量，避免 map 查找）
std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> gamepad_axis_raw_values_{};
std::array<float, SDL_GAMEPAD_AXIS_COUNT> gamepad_axis_normalized_values_{};
```

解析侧新增 `gamepadButtonFromString()` 和 `gamepadAxisDirectionFromString()`，与现有 `scancodeFromString()` / `mouseButtonFromString()` 平行。

`initializeMappings()` 的解析链显式扩展为：

```cpp
scancodeFromString()
-> mouseButtonFromString()
-> gamepadButtonFromString()
-> gamepadAxisDirectionFromString()
-> warn unknown binding
```

**轴方向标识**：用枚举表示摇杆方向，映射配置字符串：

```cpp
enum class GamepadAxisDirection : uint8_t {
    LeftStickUp, LeftStickDown, LeftStickLeft, LeftStickRight,
    RightStickUp, RightStickDown, RightStickLeft, RightStickRight,
    LeftTrigger, RightTrigger
};
```

轴的**模拟值缓存**与动作状态机分离：`InputManager` 额外缓存每个 `SDL_GamepadAxis` 的原始值和归一化值，供调试面板显示，不参与 binding 查表。这里优先用固定数组而不是 `unordered_map`，因为 SDL3 的 gamepad axis 数量是固定且很小的。

**摇杆数字化阈值**：使用双阈值避免中位抖动：

```cpp
static constexpr float AXIS_PRESS_THRESHOLD  = 0.6f;  // 超过此值视为按下
static constexpr float AXIS_RELEASE_THRESHOLD = 0.4f;  // 低于此值视为释放
```

在 `AXIS_RELEASE_THRESHOLD` ~ `AXIS_PRESS_THRESHOLD` 之间保持当前状态不变（迟滞）。

**InputDevice 枚举**：

```cpp
enum class InputDevice : uint8_t { KeyboardMouse, Gamepad };
```

`InputManager` 新增 `last_input_device_` 成员，但**只在有意义输入发生时更新**：
- 键盘：`KEY_DOWN`
- 鼠标：`MOUSE_MOTION` / `MOUSE_BUTTON_DOWN` / `MOUSE_WHEEL`
- 手柄：`GAMEPAD_BUTTON_DOWN`，或摇杆/扳机方向首次越过 press threshold

不要在 `KEY_UP` / `BUTTON_UP` / `GAMEPAD_REMOVED` / `GAMEPAD_REMAPPED` 这类“清理型事件”上切换设备，避免 Phase 2 目标模式抖动。

**单手柄策略**：只维护一个活动手柄（`SDL_Gamepad* active_gamepad_` + `SDL_JoystickID active_gamepad_id_`），但维护“最近连接优先”的活动权：
- 启动时：若已有多个手柄，取 `SDL_GetGamepads()` 返回列表中的最后一个作为**确定性的初始 active**
- 说明：SDL 不提供可可靠读取的“真实最近连接时间”，因此“启动时最近连接优先”退化为“列表最后一个”这一稳定策略；运行时新接入设备仍直接抢占 active
- 运行时：新手柄接入时直接抢占 active
- 当前 active 被移除时：重新读取 `SDL_GetGamepads()`，取列表最后一个作为新的 active；若无则置空
- 只处理 `event.gbutton.which / event.gaxis.which == active_gamepad_id_` 的事件
- active 切换、移除、析构时都必须 `SDL_CloseGamepad()`，避免句柄泄漏

---

#### 需要修改的文件

- `src/engine/input/input_manager.h` — 新增手柄相关成员、`InputDevice`、`GamepadAxisDirection`
- `src/engine/input/input_manager.cpp` — 手柄事件处理、解析函数、设备生命周期
- `config/input.json` — 为现有动作添加手柄绑定
- `src/engine/debug/panels/input_debug_panel.*` — 显示手柄连接状态和按钮/轴实时值
- `src/engine/core/game_app.cpp` — 添加 `SDL_INIT_GAMEPAD` flag
- `tests/engine/input/input_manager_test.cpp` — 测试 SDL 初始化和共用辅助函数更新
- `tests/engine/input/input_manager_rmlui_routing_test.cpp` — 测试 SDL 初始化更新
- `tests/CMakeLists.txt` — 新增测试文件

#### 需要新增的文件

- `tests/engine/input/input_manager_gamepad_test.cpp`

---

#### Step 1.1: SDL 初始化与手柄设备管理

- 在主程序和测试的 `SDL_Init` 调用中添加 `SDL_INIT_GAMEPAD` flag。
- `InputManager` 构造时枚举已连接手柄，取 `SDL_GetGamepads()` 返回列表中的最后一个作为确定性的初始 `active_gamepad_`。
- 维护 `active_gamepad_id_`，并提供统一的 `closeActiveGamepad()` / `switchActiveGamepad()` 收口逻辑。
- 处理 `SDL_EVENT_GAMEPAD_ADDED`：打开新设备并切换为 active，同时清理旧 active 的设备贡献。
- 处理 `SDL_EVENT_GAMEPAD_REMOVED`：如果是当前 active，按“系统清理”语义清空该设备贡献，关闭句柄，并重新读取 `SDL_GetGamepads()`，取列表最后一个作为新的 active。
- 处理 `SDL_EVENT_GAMEPAD_REMAPPED`：若是 active 手柄，仅 log，不改动作状态。
- `InputManager` 析构时关闭当前活动手柄。

#### Step 1.2: 手柄按钮接入动作状态机

- 处理 `SDL_EVENT_GAMEPAD_BUTTON_DOWN` / `SDL_EVENT_GAMEPAD_BUTTON_UP`。
- 只处理来自 `active_gamepad_id_` 的按钮事件，其余手柄事件直接忽略。
- 复用 `handleInputEdge` 模板函数，传入 `gamepad_button_to_actions_` 和 `gamepad_button_down_states_`。
- 新增 `gamepadButtonFromString()`，支持以下字符串映射：
  - `GamepadSouth` / `GamepadEast` / `GamepadWest` / `GamepadNorth` （对应 SDL3 位置命名）
  - `GamepadStart` / `GamepadBack` / `GamepadGuide`
  - `GamepadLeftStick` / `GamepadRightStick` （按下摇杆）
  - `GamepadLeftShoulder` / `GamepadRightShoulder`
  - `GamepadDpadUp` / `GamepadDpadDown` / `GamepadDpadLeft` / `GamepadDpadRight`
- `initializeMappings()` 中将手柄按钮解析正式接入现有字符串 binding 解析链。
- 仅在 `GAMEPAD_BUTTON_DOWN` 生效时更新 `last_input_device_ = InputDevice::Gamepad`。
- 继续沿用现有 `sampleInputEvents() -> UI forwarder -> processEvent()` 单一路径；Phase 1 **不**新增手柄专用 UI 导航消费逻辑，菜单/界面层手柄消费留到 Phase 4。

#### Step 1.3: 摇杆轴方向数字化

- 处理 `SDL_EVENT_GAMEPAD_AXIS_MOTION`。
- 只处理来自 `active_gamepad_id_` 的轴事件，其余手柄事件直接忽略。
- 缓存原始轴值，并做归一化：
  - 左/右摇杆：`[-1.0, 1.0]`
  - 左/右扳机：`[0.0, 1.0]`
- 对每个 `GamepadAxisDirection` 先投影成单方向强度 `magnitude in [0, 1]`，再应用双阈值：
  - `LeftStickUp = max(-left_y, 0)`
  - `LeftStickDown = max(left_y, 0)`
  - `LeftStickLeft = max(-left_x, 0)`
  - `LeftStickRight = max(left_x, 0)`
  - 右摇杆同理
  - `LeftTrigger` / `RightTrigger` 直接使用归一化后的 trigger 值
  ```
  当前未激活 && magnitude > AXIS_PRESS_THRESHOLD   → 触发 press
  当前已激活 && magnitude < AXIS_RELEASE_THRESHOLD → 触发 release
  ```
- 通过 `handleInputEdge` 接入 `gamepad_axis_to_actions_` / `gamepad_axis_down_states_`。
- 新增 `gamepadAxisDirectionFromString()`，支持 `LeftStickUp`、`LeftStickDown` 等字符串。
- `initializeMappings()` 中将轴方向解析正式接入现有字符串 binding 解析链。
- 仅在某个方向首次跨过 press threshold 时更新 `last_input_device_ = InputDevice::Gamepad`。

#### Step 1.4: 状态清理

- 新增统一 helper：`clearGamepadContributions()`，只清当前 active 手柄对 action 的贡献，不影响键盘/鼠标 down state。
- **手柄移除 / active 手柄切换时**：
  - 具体算法：
    1. 遍历 `gamepad_button_down_states_` 中 `value == true` 的条目
    2. 通过 `gamepad_button_to_actions_[button]` 找到受影响 `action_id`
    3. 对每个 action 执行 `active_count--`
    4. 再遍历 `gamepad_axis_down_states_` 中 `value == true` 的条目，重复上述过程
    5. 将两个 down-state 容器中的所有值统一重置为 `false`
  - 若递减后 `active_count == 0`，直接设为 `INACTIVE`，**不要**走 `RELEASED` 回调（设备移除是系统事件，不是用户主动释放）
  - 若键盘/鼠标仍持有同一 action，则动作保持 `PRESSED/HELD`
- **窗口失焦 / 最小化时**：现有 focus-lost 清理扩展到同步清空手柄 down state，行为与键盘/鼠标一致。

#### Step 1.5: 配置与默认映射

为现有动作添加手柄绑定。**不预留 Phase 2/3 的动作配置项**——那些动作在对应 Phase 中引入。

同时避免与 Phase 2 计划中的 `primary_action` / `secondary_action` / `hotbar_prev` / `hotbar_next` 默认位冲突：
- `GamepadSouth` / `GamepadEast` 预留给 Phase 2 的主/副操作
- `GamepadLeftShoulder` / `GamepadRightShoulder` 暂不分配给 `rotate_left/right`

目标 `config/input.json` 格式示例（手柄绑定与键盘绑定混写，解析时按前缀自动分类）：

```json
{
  "input_mappings": {
    "move_up":    ["W", "Up", "GamepadDpadUp", "LeftStickUp"],
    "move_down":  ["S", "Down", "GamepadDpadDown", "LeftStickDown"],
    "move_left":  ["A", "Left", "GamepadDpadLeft", "LeftStickLeft"],
    "move_right": ["D", "Right", "GamepadDpadRight", "LeftStickRight"],
    "interact":   ["F", "GamepadWest"],
    "pause":      ["P", "Escape", "GamepadStart"],
    "inventory":  ["I", "GamepadBack"],
    "hotbar":     ["Tab", "GamepadNorth"],
    "rotate_left":  ["Q"],
    "rotate_right": ["E"],
    "camera_reset_zoom": ["MouseMiddle"],
    "player_light": ["L"],
    "hotbar_1": ["1"], "hotbar_2": ["2"], "hotbar_3": ["3"],
    "hotbar_4": ["4"], "hotbar_5": ["5"], "hotbar_6": ["6"],
    "hotbar_7": ["7"], "hotbar_8": ["8"], "hotbar_9": ["9"],
    "hotbar_10": ["0"]
  }
}
```

同步更新 `defaultMappings()` 函数。

#### Step 1.6: 调试面板更新

在输入调试面板中增加手柄区域，显示：
- 当前活动手柄名称和实例 ID（或"未连接"）
- 当前已连接手柄数量（便于验证 active 切换逻辑）
- 所有按钮的实时按下状态
- 左/右摇杆轴原始值和归一化值
- 当前 `last_input_device_` 值

这些数据通过 `InputManager` 提供只读调试快照 / getter 暴露；不要让调试面板直接访问手柄内部容器。

---

#### 测试策略

使用 SDL3 `SDL_AttachVirtualJoystick` API 在测试中模拟手柄设备，无需物理手柄即可在 CI 上运行。

测试实现注意：
- 使用 `SDL_VirtualJoystickDesc` 构造虚拟设备，并通过 `SDL_INIT_INTERFACE(&desc)` 初始化
- 设置 `desc.type = SDL_JOYSTICK_TYPE_GAMEPAD`
- 再配合 `SDL_SetJoystickVirtualButton()` / `SDL_SetJoystickVirtualAxis()` 驱动按钮与轴输入

测试用例：

| 测试用例 | 说明 |
|---------|------|
| `GamepadButtonActionLifecycle` | 手柄按钮按下→PRESSED→HELD→释放→RELEASED→INACTIVE |
| `GamepadAxisDirectionPress` | 摇杆推到阈值以上→对应方向动作 PRESSED |
| `GamepadAxisDirectionHysteresis` | 摇杆在双阈值之间波动→不反复触发 |
| `GamepadTriggerDirectionPress` | 扳机超过阈值→对应 trigger 动作 PRESSED |
| `GamepadAndKeyboardSameAction` | 手柄+键盘同时绑定 `move_up`，释放其中一个不清空动作 |
| `GamepadRemovalPreservesKeyboardContribution` | 同一动作被键盘+手柄共同按住时，移除手柄后动作仍保持激活 |
| `GamepadRemovalClearsState` | 手柄移除→所有手柄相关动作状态清空 |
| `NewestConnectedGamepadBecomesActive` | 第二个手柄接入后抢占 active，旧手柄事件被忽略 |
| `FocusLostClearsGamepadState` | 失焦→手柄 down state 和动作状态一并清空 |
| `LastInputDeviceTracking` | 按键盘→KeyboardMouse；按手柄→Gamepad |
| `GamepadButtonFromStringParsing` | 验证所有支持的手柄按钮字符串映射 |
| `GamepadAxisDirectionFromStringParsing` | 验证所有支持的轴方向字符串映射 |

---

#### 待办清单

- [ ] 添加 `SDL_INIT_GAMEPAD` 到 SDL 初始化
- [ ] 定义 `InputDevice` 枚举和 `GamepadAxisDirection` 枚举
- [ ] 实现手柄设备生命周期管理（枚举/打开/移除）
- [ ] 实现 active 手柄切换与 `SDL_CloseGamepad()` 收口逻辑
- [ ] 实现 `gamepadButtonFromString()` 解析
- [ ] 实现 `gamepadAxisDirectionFromString()` 解析
- [ ] 处理 `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP`，接入 `handleInputEdge`
- [ ] 处理 `SDL_EVENT_GAMEPAD_AXIS_MOTION`，实现双阈值数字化
- [ ] 缓存手柄轴原始值 / 归一化值供调试显示
- [ ] 手柄移除 / active 切换时按“系统清理”语义清空手柄侧状态
- [ ] 窗口失焦 / 最小化时同步清空手柄状态
- [ ] 维护 `last_input_device_` 并暴露查询接口
- [ ] 更新 `config/input.json` 和 `defaultMappings()`
- [ ] 更新输入调试面板
- [ ] 更新 `tests/CMakeLists.txt`
- [ ] 编写手柄测试（使用 virtual joystick）

#### 完成标准

- 仅靠 `InputManager` 就能让 SDL3 手柄驱动现有动作系统。
- `move_* / interact / pause / inventory` 能同时支持键盘和手柄。
- 所有按钮、摇杆方向、插拔、失焦路径都有自动化测试，CI 可运行。
- 调试面板可实时查看手柄状态。
