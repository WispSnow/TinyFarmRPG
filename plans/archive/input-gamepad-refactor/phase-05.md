### Phase 5: 后续增强 - Input Buffer / Glyph / Rumble / Rebind

**目标**：在前 4 个阶段稳定后，补齐手柄和多设备体验增强能力。

**前置**：Phase 1~4 均已完成。

---

#### 设计要点

- 这一阶段不再重构核心动作状态机，而是在既有 `InputManager + 语义动作 + InputContext` 基础上补增强能力。
- 不改变运行时 per-device lookup map 方案；增强只在"定义层 / 展示层 / 体验层"补元数据和辅助能力。
- Step 之间不是完全独立：
  - `Step 5.0` 是 `Step 5.2 / Step 5.4 / Step 5.5` 的前置基础设施。
  - `Step 5.1`（Buffer）和 `Step 5.3`（Rumble）可独立按需落地。
  - `Step 5.5` 只展示已实现的增强项，不要求一次补齐全部功能。
- 不新增第二个 SDL poll 点；所有捕获、缓冲、重绑定仍由 `InputManager` 统一处理。

---

#### 需要新增的文件

- `src/engine/input/input_buffer.h`
- `src/engine/input/input_buffer.cpp`
- `src/engine/input/input_glyphs.h`
- `src/engine/input/input_glyphs.cpp`
- `tests/engine/input/input_buffer_test.cpp`
- `tests/engine/input/input_glyphs_test.cpp`
- `tests/engine/input/input_rebind_test.cpp`
- `tests/engine/input/input_manager_rumble_test.cpp`

#### 需要修改的文件

- `src/engine/input/input_manager.*`
- `src/engine/debug/panels/input_debug_panel.*`
- `config/input.json`
- 相关 UI 文案 / 提示渲染文件

---

#### Step 5.0: Binding 元数据与持久化基础设施

> `Glyph / Rebind / Debug Panel` 共用这一步的结果；建议先做。

- 在 `InputManager` 内保留"有序 binding definition"而不只是运行时 lookup map：
  - 原始 token（如 `W`、`MouseLeft`、`GamepadSouth`）
  - 推导后的输入设备（`Keyboard` / `Mouse` / `Gamepad`）
  - 用于 UI 展示的 prompt token / fallback 文本
  - 绑定槽位顺序
- 建议显式保存解析后的物理键，而不是每次都从 token 反查，例如：
  ```cpp
  struct BindingDefinition {
      std::string token;
      InputDevice device;
      std::variant<SDL_Scancode, Uint32, SDL_GamepadButton, GamepadAxisDirection> physical_key;
  };
  ```
- 运行时 lookup map 仍由这些 definition 重建，不改现有 `key_to_actions_` / `mouse_to_actions_` / `gamepad_*_to_actions_` 的执行路径。
- 保留输入配置文件路径和重建入口，提供：
  - 从 definition 重建 runtime mapping
  - 将 definition 持久化回配置文件
- `InputManager` 需持有 `config_path_` 成员，构造时保存，用于后续重绑定落盘。
- 保持现有 JSON 外层结构 `input_mappings` 不变；内部仍可用字符串数组，但运行时必须解析成带元数据的 definition。
- 为后续重绑定明确目标粒度：**动作 + 绑定槽位**。
  - 不做"捕获一个输入后直接替换整个动作数组"这种粗粒度行为。
- Phase 5 直接扩展 `InputDevice` 为三值：`Keyboard / Mouse / Gamepad`。
  - 不额外引入平行的 `PromptDeviceFamily`。
  - `PlayerControlSystem` 等调用方继续用 `== InputDevice::Gamepad` 区分手柄路径，其他输入统一视为非手柄。
- 从这一步开始预留只读 debug snapshot 结构：
  - action 基础状态
  - binding metadata
  - 当前最近输入设备
  - 后续可由 `Step 5.1 / 5.3` 追加 buffer / rumble 字段

#### Step 5.1: 输入缓冲

> 建议在战斗系统开发时按需实施，不必提前做。

- 为需要高响应的系统补可选输入缓冲，优先服务战斗菜单和确认类操作。
- 缓冲只记录 `PRESSED` 边沿，不改变 `isActionPressed()` / `isActionDown()` 现有语义。
- 缓冲按 **action 粒度** 存储，不做全局 `deque<BufferedPress>` 扫描。
  - 建议直接挂在 `ActionEntry` 上。
  - 每个 action 使用固定容量的小环形缓冲，容量建议 `4~8`。
- 时间窗口统一使用**毫秒**，不要用"最近 N 渲染帧"表述。
  - 主循环的 SDL 采样和 fixed tick 分发是分离的，用 render frame 做窗口会产生歧义。
- 时间戳统一使用 `SDL_GetTicks()` / SDL 毫秒时钟，不自行引入第二套计时源。
- 建议接口：
  - `peekBufferedPress(action_id, window_ms)`
  - `consumeBufferedPress(action_id, window_ms)`
- 每个 action 的缓冲必须有容量上限；超出上限时淘汰最旧条目，不允许无限增长。
- 缓冲必须在以下路径清空：
  - `clearAllInputState()`
  - `pushContext() / popContext()`
  - 窗口失焦 / 最小化
  - 其他会清空动作状态的输入复位路径
- 测试至少覆盖：
  - 短时窗口命中
  - 超时失效
  - 消费后移除
  - context 切换后不会把旧 scene 的确认输入带到新 scene

#### Step 5.2: 按键图标与输入源展示（Glyph / Prompt）

- Glyph 不要只返回裸 `std::string_view`，而应返回结构化 prompt 描述，例如：
  ```cpp
  struct ActionPrompt {
      InputDevice device;
      std::string_view icon_id;
      std::string_view fallback_text;
  };
  ```
- 提示选择基于"最近提示来源"而不是硬编码字符串。
  - 键盘、鼠标、手柄必须能区分。
  - 例如 `primary_action` 在鼠标模式下应显示鼠标左键，而不是笼统的 `KeyboardMouse` 提示。
- Prompt 查询依赖 `Step 5.0` 的 binding metadata，不应直接从 runtime lookup map 反推。
- UI 层调用 prompt 接口渲染提示，而不是把 `"Enter"` / `"A"` / `"鼠标左键"` 硬编码在 RML 或文本里。
- 设备切换后提示应在下一帧自动更新。
- 若某个绑定没有专用 icon，则回退到稳定文本 token。

#### Step 5.3: 手柄震动

- 震动由语义事件驱动，如确认、取消、命中、切换成功，不由某个物理按钮直接触发。
- 使用 SDL3 `SDL_RumbleGamepad` API，仅对当前活动手柄生效；无活动手柄时为 no-op。
- 对外只提供简单接口：`intensity + duration_ms`。
  - 底层 SDL 是双通道 rumble。
  - 当前阶段统一映射为 `low = high = intensity * 0xFFFF`，后续如有需要再扩展双通道接口。
- **不引入复杂震动队列**。
  - 当前阶段只维护"当前活跃震动状态"和"最近一次请求"用于调试展示。
  - 新请求直接覆盖旧请求，避免把简单能力做成小型调度器。
- 测试至少覆盖：
  - 无活动手柄时安全 no-op
  - 有请求时内部状态更新正确
  - 调试面板可读到最近一次请求

#### Step 5.4: 重绑定

- 重绑定依赖 `Step 5.0` 的 definition/persistence 基础设施。
- 流程改为：
  - 用户选择一个动作和一个绑定槽位
  - `InputManager` 进入"等待输入"捕获模式
  - 捕获下一个有效物理输入
  - 更新该槽位 definition
  - 重建 runtime mapping
  - 持久化到配置文件
- 捕获模式必须**拦截正常动作派发和 UI 导航**，避免"按下要绑定的键"同时触发游戏逻辑。
- 捕获模式还必须在 `sampleInputEvents()` 入口同时跳过：
  - RmlUI 原始事件转发
  - ImGui 事件转发
  只保留"提取候选物理输入"这一路径。
- 捕获输入时仅接受：
  - `KEY_DOWN`
  - `MOUSE_BUTTON_DOWN`
  - `GAMEPAD_BUTTON_DOWN`
  - 达到阈值的 `GAMEPAD_AXIS_MOTION` 方向输入
- 默认忽略：
  - 鼠标移动
  - 滚轮
  - 文本输入
  - 未越过阈值的轴抖动
  - `key.repeat == true` 的重复按键事件
- 取消方式需明确：
  - 例如 `Escape` 或 `menu_cancel`
  - 取消时不修改现有绑定
- 冲突策略需显式定义：
  - 至少做冲突检测和用户确认
  - 不要静默移除其他动作的绑定
- 重建 runtime mapping 时必须同步重建：
  - `rmlui_suppressed_navigation_scancodes_`
  - 其他依赖物理键定义派生出的运行时缓存
- `context_definitions_` 不需要因重绑定而重建；它依赖 action 名称，不依赖物理键。
- 持久化建议用"写临时文件再原子替换"方式，避免配置写坏。

#### Step 5.5: 调试面板与测试补强

- 输入调试面板补充显示：
  - 输入缓冲区当前内容、容量、剩余有效时间
  - 各动作当前 prompt / glyph
  - 最近一次和当前活跃 rumble 状态
  - 当前是否处于 rebind capture 模式
- 不把 `getActionsDebug()` 继续扩成万能运行时接口；调试所需数据应通过明确的只读 debug snapshot 提供。
- 调试数据按阶段增量扩展：
  - `Step 5.0` 提供 action + binding metadata snapshot
  - `Step 5.1` 追加 buffer snapshot
  - `Step 5.3` 追加 rumble snapshot
  - `Step 5.4` 追加 rebind capture snapshot
- 新增测试覆盖：
  - Buffer 生命周期
  - Glyph / prompt 选择与 fallback
  - Rumble 状态
  - Rebind capture、冲突处理、持久化回读

---

#### 推荐执行顺序

```text
5.0 -> 5.2
5.0 -> 5.4
5.0 -> 5.5
5.1 独立，建议随战斗系统按需落地
5.3 独立，只依赖 active_gamepad_
```

---

#### 待办清单

- [ ] 实现 `Step 5.0` 的 binding metadata / persistence 基础设施
- [ ] 实现输入缓冲（建议随战斗系统开发一起做）
- [ ] 为战斗/菜单预留缓冲消费接口
- [ ] 实现结构化 prompt / glyph 查询接口
- [ ] 在 UI 中切换设备提示文案
- [ ] 增加手柄震动接口
- [ ] 增加按槽位的重绑定能力
- [ ] 补全调试面板与测试覆盖

#### 完成标准

- 战斗和菜单类系统可以可靠消费短时缓冲输入，且 context 切换不会串输入。
- UI 能根据最近输入来源显示合适提示，并正确区分键盘、鼠标、手柄。
- 重绑定可以在运行时安全捕获输入、更新指定槽位并持久化。
- 震动与提示能力都建立在统一语义动作之上，而不是新的临时分支。
