### Phase 1: 输入核心 - Binding 模型 + SDL3 手柄接入

**目标**：让输入系统在底层完整支持 SDL3 手柄，并继续复用当前 action 状态机。

#### 实现思路

- 把 binding 解析从 `InputManager` 中拆出来，明确区分键盘、鼠标、手柄按钮、手柄轴方向。
- `InputManager` 继续作为 SDL 事件唯一入口，但补齐手柄设备生命周期、状态维护和动作映射。
- 手柄轴先做“数字化方向输入”，优先覆盖 `move_*` 这种已存在的动作路径，不急着暴露完整模拟量 API。

#### 需要新增的文件

- `src/engine/input/input_binding.h`
- `src/engine/input/input_binding.cpp`
- `tests/engine/input/input_binding_test.cpp`
- `tests/engine/input/input_manager_gamepad_test.cpp`

#### 需要修改的文件

- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `config/input.json`

#### Step 1.1: 抽离 binding 数据模型

- 定义统一 binding 类型，至少覆盖：
  - 键盘键
  - 鼠标按钮
  - 手柄按钮
  - 手柄轴方向
- 在解析层统一处理别名，例如 `GamepadSouth`、`GamepadEast`、`GamepadStart`、`GamepadDpadUp`、`LeftStickLeft`。

#### Step 1.2: 扩展 InputManager 的运行时状态

- 增加当前活动手柄句柄、实例 ID、按钮 down state、轴方向激活 state。
- 启动时枚举并打开已有手柄。
- 处理 `SDL_EVENT_GAMEPAD_ADDED` 和 `SDL_EVENT_GAMEPAD_REMOVED`。

#### Step 1.3: 把手柄输入接到动作状态机

- 处理 `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP`。
- 处理 `SDL_EVENT_GAMEPAD_AXIS_MOTION`。
- 为摇杆方向实现 press / release 双阈值，避免中位抖动反复触发。
- 保证同一动作同时绑定键盘和手柄时，释放其中一个设备不会错误清空动作。

#### Step 1.4: 清理与边界条件

- 手柄移除时清空对应动作引用计数和 down state。
- 窗口失焦时同步清空手柄状态，行为与键盘/鼠标一致。
- 记录最近一次输入来源，供后续 Phase 2/3 使用。

#### Step 1.5: 扩展默认映射和配置

- 为 `move_*`、`interact`、`pause`、`inventory` 补默认手柄映射。
- 预留 `primary_action`、`secondary_action`、`hotbar_prev`、`hotbar_next`、`menu_*` 的配置项，即使 Phase 2/3 才真正消费。

#### 待办清单

- [ ] 定义 binding 类型和解析接口
- [ ] 为配置解析补手柄按钮和轴方向支持
- [ ] 为 `InputManager` 增加手柄设备管理
- [ ] 为按钮输入补动作状态流转
- [ ] 为摇杆方向补动作状态流转
- [ ] 在失焦和手柄移除时清空状态
- [ ] 扩展 `config/input.json` 和默认映射
- [ ] 增加 `input_binding` 测试
- [ ] 增加 `input_manager_gamepad` 测试

#### 完成标准

- 仅靠 `InputManager` 就能让 SDL3 手柄驱动现有动作系统。
- `move_* / interact / pause / inventory` 能同时支持键盘和手柄。
- 所有按钮、摇杆方向、插拔、失焦路径都有自动化测试。
