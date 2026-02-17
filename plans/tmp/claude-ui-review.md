# UI 模块代码审查与重构建议

> 审查范围: `src/engine/ui/`
> 日期: 2026-02-17
> 分支: codex

---

## 一、核心问题：InteractionBehavior 能否完全替代 UIState？

### 结论：不能直接替代，但可以将 UIState 的职责内化为 UIInteractive 的简单枚举+行为组合

### 1.1 两者的职责对比

| 维度 | UIState (State Pattern) | InteractionBehavior (Behavior Pattern) |
|------|------------------------|---------------------------------------|
| **解决的问题** | 管理 Normal/Hover/Pressed 视觉状态转换 | 为交互事件挂载可插拔的自定义逻辑 |
| **所有权模型** | 1:1（同时只有一个活跃状态） | 1:N（可挂载多个行为） |
| **核心能力** | 状态转换 + 视觉切换 + 音效播放 | 事件回调转发 |
| **耦合方式** | `friend class` + 直接访问 owner_ 成员 | 通过 `UIInteractive&` 引用的公开 API |
| **生命周期** | 每次转换 new/delete 一个状态对象 | 创建后长期存活 |

### 1.2 UIState 当前做了什么（实际代码分析）

三个具体状态类做的事情非常简单：

**UINormalState::enter()** → `applyStateVisual(NORMAL_ID)`
**UINormalState::onMouseEnter()** → `playSoundEvent(HOVER)` + 切换到 HoverState

**UIHoverState::enter()** → `applyStateVisual(HOVER_ID)` + `hover_enter()`
**UIHoverState::onMouseExit()** → `hover_leave()` + 切换到 NormalState
**UIHoverState::onMousePressed()** → 切换到 PressedState

**UIPressedState::enter()** → `applyStateVisual(PRESSED_ID)` + `playSoundEvent(CLICK)`
**UIPressedState::onMouseReleased(inside)** → inside ? (切换 Hover + `clicked()`) : 切换 Normal

**总计：3个类 + 6个 .h/.cpp 文件，只为实现一个简单的三态转换。**

### 1.3 为什么 UIState 是过度设计

1. **状态空间固定且极小**：只有 Normal/Hover/Pressed 三个状态，转换规则完全确定，不存在"未来可能新增状态"的合理场景（Disabled 在 UIButton 中已经通过 `UIButtonVisualState` 枚举单独处理）
2. **每次转换都 heap-allocate**：`std::make_unique<UIHoverState>(owner_)` 在每次鼠标进出时都分配内存
3. **deferred transition 增加复杂度**：`next_state_` + `setState()` 的两阶段提交机制是为了避免在事件处理中销毁当前状态，但如果用枚举就根本不存在这个问题
4. **friend class 破坏封装**：所有状态类都需要 `friend class UIInteractive` 才能调用 protected 方法
5. **职责分散**：视觉反馈逻辑散布在 3 个状态类 + UIInteractive 中，难以一目了然

### 1.4 推荐方案：枚举 + 内联状态机

将 UIState 的整个状态机逻辑内联到 UIInteractive 中，用一个简单的枚举替代：

```cpp
// ui_interactive.h
enum class InteractionState : uint8_t {
    Normal,
    Hovered,
    Pressed
};

class UIInteractive : public UIElement {
protected:
    InteractionState interaction_state_{InteractionState::Normal};

    // 状态转换（直接内联，无需堆分配）
    void transitionTo(InteractionState new_state);

public:
    bool isHovered() const {
        return interaction_state_ == InteractionState::Hovered
            || interaction_state_ == InteractionState::Pressed;
    }
    bool isPressed() const {
        return interaction_state_ == InteractionState::Pressed;
    }
    InteractionState getInteractionState() const { return interaction_state_; }
    // ...
};
```

**transitionTo 实现**（约 20 行，替代 3 个类 6 个文件）：
```cpp
void UIInteractive::transitionTo(InteractionState new_state) {
    if (new_state == interaction_state_) return;
    interaction_state_ = new_state;

    switch (new_state) {
        case InteractionState::Normal:
            applyStateVisual(UI_IMAGE_NORMAL_ID);
            break;
        case InteractionState::Hovered:
            applyStateVisual(UI_IMAGE_HOVER_ID);
            hover_enter();
            break;
        case InteractionState::Pressed:
            applyStateVisual(UI_IMAGE_PRESSED_ID);
            playSoundEvent(UI_SOUND_EVENT_CLICK_ID);
            break;
    }
}
```

**mouseEnter/mouseExit/mousePressed/mouseReleased** 中直接调用 `transitionTo()`，消除所有间接层。

### 1.5 迁移影响评估

| 受影响文件 | 改动量 | 说明 |
|-----------|--------|------|
| `ui_interactive.h/cpp` | 中 | 删除 `state_`/`next_state_`，新增 `interaction_state_` 枚举和 `transitionTo()` |
| `ui_button.h/cpp` | 小 | 删除 `setState(UINormalState)` 调用，改为直接设置枚举 |
| `ui_item_slot.cpp` | 小 | 同上 |
| `state/ui_state.h` | 删除 | |
| `state/ui_normal_state.h/cpp` | 删除 | |
| `state/ui_hover_state.h/cpp` | 删除 | |
| `state/ui_pressed_state.h/cpp` | 删除 | |
| CMakeLists.txt | 小 | 移除 state 相关源文件 |

**风险**：低。状态转换逻辑完全确定性，行为等价替换。

---

## 二、其它重构建议

### 2.1 UIInteractive 的虚函数回调与 InteractionBehavior 职责重叠

**现状**：UIInteractive 同时提供两种事件回调机制：

```cpp
// 方式 A：虚函数重写（UIButton 使用）
virtual void clicked() {}
virtual void hover_enter() {}
virtual void hover_leave() {}

// 方式 B：InteractionBehavior（inventory_ui / hotbar_ui 使用）
behaviors_ → onClick / onHoverEnter / onHoverExit
```

**问题**：
- 两套机制做同样的事，增加理解成本
- UIButton 用方式 A（通过 `std::function` 包装），UIItemSlot 用方式 B
- hover_enter/hover_leave 既在 UIState 内部被调用（UIHoverState::enter / onMouseExit），又在 mouseEnter/mouseExit 中通知 behaviors——调用时序不明显

**建议**：统一为 InteractionBehavior 机制

新增一个通用的 `ClickBehavior`（类似已有的 `HoverBehavior`），然后：
- UIButton 的 click/hover 回调通过 ClickBehavior + HoverBehavior 挂载
- 删除 UIInteractive 上的 `virtual void clicked()` / `hover_enter()` / `hover_leave()`
- 所有事件回调统一走 behaviors_ 管道

```cpp
class ClickBehavior final : public InteractionBehavior {
    std::function<void(UIInteractive&)> on_click_{};
public:
    void setOnClick(std::function<void(UIInteractive&)> cb) { on_click_ = std::move(cb); }
    void onClick(UIInteractive& owner) override { if (on_click_) on_click_(owner); }
};
```

**收益**：
- 单一事件分发路径，消除"在哪里挂回调"的困惑
- UIInteractive 更纯粹——只负责状态管理，不承载业务逻辑回调
- 可以在运行时动态替换/添加/移除行为

**注意**：这个重构依赖上面 P0 完成后的结果，需要确认 `transitionTo()` 中 `hover_enter()` / `clicked()` 的调用替换为 behaviors 通知。

### 2.2 UIButton 的双重视觉状态追踪

**现状**：UIButton 有两层状态追踪：
1. 继承自 UIInteractive 的 `state_`（UIState 状态机）
2. 自身的 `current_visual_state_`（`UIButtonVisualState` 枚举）

```cpp
// ui_button.h
enum class UIButtonVisualState : std::uint8_t {
    Normal, Hover, Pressed, Disabled, Count
};
UIButtonVisualState current_visual_state_{UIButtonVisualState::Normal};
```

UIButton 重写了 `applyStateVisual()` 来将 UIState 的切换映射到自己的枚举：

```cpp
void UIButton::applyStateVisual(entt::id_type state_id) {
    if (const auto state = fromStateId(state_id)) {
        current_visual_state_ = *state;  // 将 UIState 的 ID 映射到自己的枚举
    }
}
```

**问题**：如果采用方案一（枚举替代 UIState），UIButton 的 `UIButtonVisualState` 与 `InteractionState` 几乎重叠（区别仅是 Disabled）。

**建议**：
- 在 `InteractionState` 枚举中增加 `Disabled` 状态
- 删除 `UIButtonVisualState`，UIButton 直接使用 `InteractionState`
- UIButton 的 `Disabled` 可以通过 `setInteractive(false)` + 一个 disabled 标志实现

### 2.3 UIInteractive 中拖拽状态管理可提取为内置行为

**现状**：`is_pressed_`、`is_dragging_`、`last_mouse_pos_` 这些拖拽相关的字段直接写在 UIInteractive 中：

```cpp
// ui_interactive.h (line 44-46)
bool is_pressed_{false};
bool is_dragging_{false};
glm::vec2 last_mouse_pos_{0.0f, 0.0f};
```

update() 中包含拖拽检测逻辑（line 163-175）。但并非所有 UIInteractive 子类都需要拖拽。

**建议**：将拖拽状态追踪逻辑下沉到一个 `DragTracker` behavior 中，或至少将拖拽检测逻辑提取为可选功能。不过考虑到当前 UIInteractive 的子类数量不多，且拖拽是常见需求，**此项优先级较低**，可以在后续需要时再做。

### 2.4 UIManager 中硬编码的鼠标事件绑定

**现状**：`UIManager` 中的 `registerMouseEvents()` / `unregisterMouseEvents()` 通过 InputManager 注册鼠标回调。

**潜在问题**：如果将来需要支持触摸、手柄等输入方式，UIManager 需要大量修改。

**建议**：暂时不动，但在架构文档中注明未来可考虑引入 `UIInputAdapter` 抽象层。**此项优先级低**，当前只有鼠标输入。

### 2.5 state/ 目录删除后的文件组织

如果执行方案一，`src/engine/ui/state/` 整个目录将被删除。建议：
- `InteractionState` 枚举定义在 `ui_interactive.h` 中（紧靠使用处）
- `behavior/` 目录保留不变
- 考虑新增 `behavior/click_behavior.h`（如果执行 2.1 建议）

---

## 三、重构优先级排序

| 优先级 | 项目 | 收益 | 风险 | 工作量 |
|--------|------|------|------|--------|
| **P0** | 枚举替代 UIState | 删除 7 个文件，消除不必要的堆分配和间接层 | 低 | 小 |
| **P1** | 统一回调为 Behavior | 单一事件路径，降低理解成本 | 中（需修改 UIButton 创建方式） | 中 |
| **P2** | 合并 UIButtonVisualState | 消除双重状态追踪 | 低 | 小（依赖 P0） |
| **P3** | 拖拽逻辑可选化 | 减少 UIInteractive 基类膨胀 | 低 | 中 |
| **P4** | UIInputAdapter 抽象 | 未来扩展性 | — | 大（暂不执行） |

---

## 四、建议执行顺序

1. **P0: 枚举替代 UIState** — 独立完成，风险最低，收益最直接
2. **P2: 合并 UIButtonVisualState** — 紧跟 P0，因为此时 UIButton 的双重状态追踪问题自然暴露
3. **P1: 统一回调为 Behavior** — 需要修改 UIButton 和 UIItemSlot 的创建方式，建议单独分支
4. **P3/P4** — 按需执行

---

## 五、附录：当前文件清单

### 将删除的文件（P0 完成后）
```
src/engine/ui/state/ui_state.h
src/engine/ui/state/ui_normal_state.h
src/engine/ui/state/ui_normal_state.cpp
src/engine/ui/state/ui_hover_state.h
src/engine/ui/state/ui_hover_state.cpp
src/engine/ui/state/ui_pressed_state.h
src/engine/ui/state/ui_pressed_state.cpp
```

### 将修改的文件（P0）
```
src/engine/ui/ui_interactive.h     — 新增 InteractionState 枚举，删除 state_/next_state_
src/engine/ui/ui_interactive.cpp   — 新增 transitionTo()，简化 mouse* 方法
src/engine/ui/ui_button.h          — 可选：删除 UIButtonVisualState（如果同时做 P2）
src/engine/ui/ui_button.cpp        — 修改 initFromPreset() 和 applyStateVisual()
src/engine/ui/ui_item_slot.cpp     — 修改构造函数中的 setState() 调用
CMakeLists.txt                     — 移除 state 相关源文件
```

### 将新增的文件（P1 完成后）
```
src/engine/ui/behavior/click_behavior.h  — 点击行为封装
```
