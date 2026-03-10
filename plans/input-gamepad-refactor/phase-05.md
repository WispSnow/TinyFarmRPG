### Phase 5: 后续增强 - Input Buffer / Glyph / Rumble / Rebind

**目标**：在前 4 个阶段稳定后，补齐手柄和多设备体验增强能力。

**前置**：Phase 1~4 均已完成。

---

#### 设计要点

- 这一阶段不再重构核心动作流转，而是在稳定基础上做增强。
- 所有增强都以前 4 个阶段的语义动作和上下文模型为前提。
- **各 step 之间相互独立**，可根据实际开发需要选择性实施，不需要按序全做。

---

#### 需要新增的文件

- `src/engine/input/input_buffer.h`
- `src/engine/input/input_buffer.cpp`
- `src/engine/input/input_glyphs.h`
- `src/engine/input/input_glyphs.cpp`
- `tests/engine/input/input_buffer_test.cpp`

#### 需要修改的文件

- `src/engine/input/input_manager.*`
- `src/engine/debug/panels/input_debug_panel.*`
- 相关 UI 文案 / 提示渲染文件

---

#### Step 5.1: 输入缓冲

> 建议在战斗系统开发时按需实施，不必提前做。

- 为需要高响应的系统补可选输入缓冲，优先服务战斗菜单和连招/确认类操作。
- 缓冲保留最近 N 帧或最近 T 毫秒的边沿输入（PRESSED 事件）。
- 消费方可查询"最近 buffer_window 内是否有某动作的 PRESSED"，消费后从 buffer 移除。
- 不影响现有 `isActionPressed` 语义——缓冲是独立的查询通道。

#### Step 5.2: 按键图标与输入源展示（Glyph）

- 根据 `last_input_device_` 显示键盘或手柄提示图标。
- 为常见语义动作提供 glyph 查询接口：
  ```cpp
  /// 返回动作当前应显示的提示文本或图标标识
  std::string_view getActionGlyph(entt::id_type action_id) const;
  ```
- UI 层调用此接口渲染提示，而不是硬编码按钮文本。
- 设备切换时提示自动更新（下一帧生效）。

#### Step 5.3: 手柄震动

- 为确认、取消、命中、切换等行为提供有限的震动反馈接口。
- 震动应由语义事件驱动（如 `ToolHitEvent`），而不是由某个物理按钮直接触发。
- 使用 SDL3 `SDL_RumbleGamepad` API。
- 提供简单的强度+时长参数，不做复杂的震动曲线。

#### Step 5.4: 重绑定

- 在已有 binding 模型上扩展重绑定能力。
- 运行时重绑定：用户选择一个动作 → 进入"等待输入"模式 → 捕获下一个物理输入 → 更新映射。
- 重绑定结果持久化到配置文件。
- 优先支持动作级重绑定，不做设备专属路径。

#### Step 5.5: 调试面板补强

- 在输入调试面板中补充显示：
  - 输入缓冲区当前内容和容量
  - 各动作的当前 glyph 文本
  - 震动队列状态

---

#### 待办清单

- [ ] 实现输入缓冲（建议随战斗系统开发一起做）
- [ ] 为战斗/菜单预留缓冲消费接口
- [ ] 实现 glyph 查询接口
- [ ] 在 UI 中切换设备提示文案
- [ ] 增加手柄震动接口
- [ ] 增加动作级重绑定能力
- [ ] 补全调试面板展示

#### 完成标准

- 战斗和菜单类系统可以可靠消费短时缓冲输入。
- UI 能根据输入源显示合适的提示。
- 设备体验增强能力建立在统一语义动作之上，而不是新的临时分支。
