# DialogueBubble 世界锚点 UI 解耦重构

## 背景与问题

`DialogueBubble` 当前由 `UIManager` 管理，作为 UIElement 子树挂在 Scene 级 UI 树中。但它与其他屏幕固定 UI（InventoryUI、HotbarUI、TimeClock 等）有本质区别：

1. **世界坐标依赖** — 每帧在 `update()` 中执行 `camera.worldToScreen(world_position_)` 将世界坐标投影到屏幕坐标（`dialogue_bubble.cpp:106`），是 UIManager 中唯一依赖世界坐标的元素。
2. **事件订阅耦合** — 自身直接订阅 `DialogueShow/Move/HideEvent`（`dialogue_bubble.cpp:71-74`），承担了事件路由与 channel 分发的控制逻辑。
3. **文本排版逻辑** — `onShowEvent()` 内含 28 字符行宽的自动换行逻辑（`dialogue_bubble.cpp:117-146`），这是业务逻辑而非视图职责。
4. **职责过重** — 同一个类承担了事件订阅、世界坐标跟随、文本排版、UI 渲染四项职责，难以复用。

这导致 `UIManager` 的边界模糊：它本应专注于屏幕空间 UI 树管理（布局、输入、渲染调度），却隐式承载了一个世界锚点 UI 的全部生命周期。未来将新增大量世界锚点 UI（飘字、血条、任务标记、头顶状态图标等），如果不在引擎层提供通用支持，每个都要复制相同的 `worldToScreen` 模式。

## 目标
- 在引擎层 `UIElement` 中内置世界锚点定位模式，UIManager 根节点的直接子元素可通过 `setWorldAnchor()` 锚定世界坐标，由 `UIManager` 每帧自动完成投影。
- `UIManager` 在 render 阶段统一处理所有世界锚点元素的插值投影（使用插值相机 + 插值锚点坐标），屏幕固定元素不受影响。
- 对话气泡的**位置相关事件**（`DialogueShowEvent` / `DialogueMoveEvent`）改为 `dispatcher.trigger()` 同步分发，消除 1 tick 跟随延迟。
- `DialogueBubble` 退化为纯视图组件（View），不直接持有 `dispatcher`、`channel`、`world_position_`。
- 新增游戏层 `DialogueBubbleController` 处理事件订阅与业务逻辑路由（精简的 controller，不含投影逻辑），并以 `view_id` 解析目标视图，支持动态移除/重建。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem 等）调用点无需任何改动。
- 为后续世界锚点 UI（`FloatingText`、`DamageNumber`、`QuestMarker`、`BuffIcon`、`HealthBar`）打好引擎级地基。

## 非目标
- 不修改 `DialogueShowEvent` / `DialogueMoveEvent` / `DialogueHideEvent` 事件结构。
- 不修改任何业务系统（DialogueSystem、ChestSystem、ItemUseSystem）的业务判断与调用点（仍通过 `emitDialogueBubble*` helper 发事件）。
- 不在本次重构中解决文本自动换行的像素级测量问题（可后续独立推进）。
- 不引入新的渲染通道（仅调整 `GameScene::render()` 中相机恢复时机，确保 UI 使用插值相机）。

## 方案选型

### 历史方案（已否决）

| 方案 | 做法 | 否决理由 |
|------|------|----------|
| A：仅修正文档 | 保持现状 | 不解决实际问题 |
| B：双管理器 | ScreenUIManager + WorldUIManager | 80% 代码重复，维护成本翻倍 |
| C：外挂 Controller | 游戏层 WorldAnchorUIController 做投影 | 引擎层不感知世界坐标，每新增一种世界 UI 都要在 controller 里手动注册 |

### 采用方案（D）：引擎层锚点策略 + 游戏层事件 Controller

参考 Unity Canvas 的 Screen/World Space 模式和自研引擎的常见做法：在 `UIElement` 内置定位模式区分，`UIManager` 统一处理投影。游戏层只需一个轻量 Controller 做事件路由。

**优势**：
- 未来任何 UIElement 设一个 mode + 世界坐标就能锚定，无需额外注册。
- 投影逻辑在引擎层统一维护，游戏层零重复。
- `UIElement::update()` 已接收 `Context&`，`Context` 已持有 `Camera&`，引擎层不引入新的外部依赖。

## 渲染插值问题分析

### 问题

本项目采用 **固定步长 + 可变渲染** 架构（`game_app.cpp:164-236`）。渲染阶段使用 `interpolation_alpha` 对实体位置和相机位置做线性插值，以消除逻辑帧率与渲染帧率不一致时的视觉抖动：

- **实体渲染**：`render_system.cpp:15-46` 使用 `glm::mix(previous_position_, position_, alpha)` 插值。
- **相机渲染**：`game_scene.cpp:199-203` 临时将相机设为 `glm::mix(previous_camera_position_, camera_position, alpha)`。

如果世界锚点投影在 **update 阶段**执行（如原方案），会导致以下不一致：

1. **相机未插值** — `UIManager::update()` 时相机仍为逻辑位置，而实体在 render 阶段使用插值相机渲染。
2. **锚点世界坐标未插值** — `world_anchor_` 存储的是实体逻辑位置，但实体精灵渲染在 `lerp(previous_position_, position_, alpha)` 处。
3. **相机在 UI render 前已恢复** — `game_scene.cpp:215-216` 在 `Scene::render()` 之前恢复了相机原位，UI 渲染上下文中相机也是非插值的。

**结果**：气泡与其锚定的实体精灵之间会产生视觉偏移/抖动，在相机或实体快速移动时尤为明显。

### 解决方案：引擎层内置锚点插值（方案 B）

将 `previous_world_anchor_` 内置到 `UIElement`，由引擎层在渲染阶段统一做插值投影。使用者只需调 `setWorldAnchor(pos)` 即可获得平滑跟随，无需关心插值。此模式与 `TransformComponent` 的 `previous_position_` / `position_` 完全一致。

核心改动：

1. `UIElement` 新增 `previous_world_anchor_` 成员，在 `setWorldAnchor()` 中自动快照。
2. `resolveWorldAnchors()` 从 `UIManager::update()` **移至 `UIManager::render()`**，接收 `interpolation_alpha` 参数。
3. 投影时使用 `glm::mix(previous_world_anchor_, world_anchor_, alpha)` 做插值。
4. `GameScene::render()` 延迟相机恢复，确保 `Scene::render()`（含 UIManager）在**插值相机**下执行。

**注意**：为满足“零 tick 跟随延迟”要求，本方案将 `DialogueShowEvent` / `DialogueMoveEvent` 的发送路径改为 `dispatcher.trigger()`（同步触发）。这样 `setWorldAnchor()` 在 `fixedUpdate()` 同帧生效，渲染时即可使用最新锚点。

## 当前基线（关键代码）

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/engine/ui/ui_element.h:36-144` | UIElement 基类 | `position_`（屏幕局部坐标）、`ensureLayout()`、`update(Context&)` |
| `src/engine/ui/ui_element.cpp:287-300` | `invalidateLayout()` | 递归向子节点传播脏标记 |
| `src/engine/ui/ui_element.cpp:302-376` | `ensureLayout()` | 锚点/pivot/margin 屏幕空间布局计算 |
| `src/engine/ui/ui_element.cpp:402-408` | `setPosition()` | 调用 `invalidateLayout()` 触发子树重算 |
| `src/engine/ui/ui_manager.cpp:67-80` | `UIManager::update()` | `processMouseHover()` + 递归 `root->update()` |
| `docs/engine/layout-contract.md:134-138` | 变更纪律 | 语义变更需同步测试与文档 |
| `src/game/ui/dialogue_bubble.h:19-55` | DialogueBubble | 同时持有 dispatcher、world_position、channel |
| `src/game/ui/dialogue_bubble.cpp:104-108` | `update()` | 每帧 `worldToScreen` + `setPosition` |
| `src/game/ui/dialogue_bubble.cpp:110-148` | `onShowEvent()` | 事件过滤 + 文本换行 + setText + setVisible |
| `src/game/scene/game_scene.cpp:342-369` | `initUI()` | 创建 3 个 DialogueBubble 加入 UIManager |
| `src/game/system/system_helpers.h:60-91` | `emitDialogueBubble*` | 业务系统发送事件的 helper（当前为 enqueue，重构后 Show/Move 改 trigger） |

## 设计约束与契约

### C1. WorldAnchor 仅限根节点直接子元素

`setWorldAnchor()` 仅对 UIManager 根节点（`root_element_`）的直接子元素生效。理由：
- WorldAnchor 元素的 `layout_position_` 是绝对屏幕坐标（由 `worldToScreen` 投影），不依赖父元素 content bounds。
- 如果嵌套使用（WorldAnchor 元素的子节点也是 WorldAnchor），父子投影语义不明确。
- 当前所有世界锚点 UI（DialogueBubble）以及未来的飘字/标记/血条都是根节点的直接子元素。

**实现保障**：`resolveWorldAnchors()` 只遍历 `root_element_` 的直接子节点（不递归），遇到 WorldAnchor 元素时处理，其余跳过。如果非直接子元素设置了 WorldAnchor mode，`ensureLayout()` 会 fallback 到 Screen 模式并输出 warn 日志。

### C2. 时序契约

```
每帧执行顺序：
  1. fixedUpdate() ×N       — 业务系统更新实体位置，trigger(Show/Move) 同步更新气泡锚点
  2. UIManager::update()
     2a. processMouseHover()     — 世界锚点元素使用上一帧的投影位置做命中检测（可接受）
     2b. root->update()          — 递归更新 UI 树（含 ensureLayout）
  3. render(interpolation_alpha)
     3a. GameScene::render()
         - 相机临时设为 lerp(prev_cam, cam, alpha)   — 插值相机
         - 实体渲染: lerp(prev_pos, pos, alpha)       — 插值实体
     3b. Scene::render(alpha)
         - UIManager::render(context, alpha)
           - resolveWorldAnchors(camera, alpha)        — 插值锚点 + 插值相机 → worldToScreen
           - root->render()                            — UI 渲染
     3c. 相机恢复原位
  4. dispatcher_->update()   — 处理剩余 enqueue 事件（位置锚点更新不依赖此阶段）
```

**规则**：
- `resolveWorldAnchors()` 在 **render 阶段**执行，此时相机已被设为插值位置。锚点坐标使用 `glm::mix(previous_world_anchor_, world_anchor_, alpha)` 做插值，确保气泡与实体精灵位置一致。
- `setWorldAnchor()` 内部自动将当前 `world_anchor_` 快照为 `previous_world_anchor_`，无需外部手动管理。
- `DialogueShowEvent` / `DialogueMoveEvent` 通过 `trigger` 同步分发，`setWorldAnchor()` 在 `fixedUpdate` 内立即生效，不再出现 1 tick 跟随延迟。
- `DialogueHideEvent` 可保留 `enqueue`（兼容现有脚本调用）或后续统一切换为 `trigger`；两者都不影响锚点跟随一致性。

### C3. clearWorldAnchor() 后的位置恢复

调用 `clearWorldAnchor()` 后：
- `positioning_mode_` 恢复为 `Screen`。
- 元素使用调用 `clearWorldAnchor()` 前最后一次 `setPosition()` 设置的 `position_` 值重新参与正常布局计算。
- 如果从未调用过 `setPosition()`，则使用构造时的初始 `position_`（通常为 `{0, 0}`）。
- `world_anchor_`、`previous_world_anchor_` 和 `world_anchor_offset_` 被清零。

## 详细设计

### 1. 引擎层：UIElement 新增世界锚点定位模式

**改动文件**：`src/engine/ui/ui_element.h`

```cpp
// 新增枚举（放在 UIElement 类定义之前，namespace engine::ui 内）
enum class PositioningMode : std::uint8_t {
    Screen,      // 默认：position_ 是相对父元素的屏幕局部坐标
    WorldAnchor  // 世界锚点模式：position_ 不参与布局，由 UIManager 投影 world_anchor_ 写入 layout_position_
};
```

**UIElement 新增成员**：

```cpp
protected:
    PositioningMode positioning_mode_{PositioningMode::Screen};
    glm::vec2 world_anchor_{0.0f, 0.0f};              // 世界坐标锚点（当前值）
    glm::vec2 previous_world_anchor_{0.0f, 0.0f};     // 世界坐标锚点（上一次值，用于插值）
    glm::vec2 world_anchor_offset_{0.0f, 0.0f};       // 投影后的屏幕偏移
```

**UIElement 新增公开接口**：

```cpp
public:
    void setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset = {0.0f, 0.0f});
    void clearWorldAnchor();
    [[nodiscard]] PositioningMode getPositioningMode() const { return positioning_mode_; }
    [[nodiscard]] const glm::vec2& getWorldAnchor() const { return world_anchor_; }
    [[nodiscard]] const glm::vec2& getPreviousWorldAnchor() const { return previous_world_anchor_; }
    [[nodiscard]] const glm::vec2& getWorldAnchorOffset() const { return world_anchor_offset_; }
```

**UIElement 新增 protected 友元方法**：

```cpp
protected:
    friend class UIManager;
    void applyWorldAnchorPosition(glm::vec2 screen_pos);
```

**接口语义**：
- `setWorldAnchor(pos, offset)`：设置 `positioning_mode_ = WorldAnchor`，将当前 `world_anchor_` 快照为 `previous_world_anchor_`（首次进入 WorldAnchor 模式时 previous = current，避免从原点插值飞入），存储世界坐标和屏幕偏移，调用 `invalidateLayout()` 传播脏标记。
- `clearWorldAnchor()`：设置 `positioning_mode_ = Screen`，清零 `world_anchor_`、`previous_world_anchor_` 和 `world_anchor_offset_`，调用 `invalidateLayout()` 恢复正常布局。
- `applyWorldAnchorPosition(screen_pos)`：由 UIManager 在投影阶段调用，写入 `layout_position_` 和 `layout_size_`，然后**向子节点传播脏标记**确保子树重新布局。

### 2. 引擎层：applyWorldAnchorPosition() 实现（含子树脏标记传播）

**改动文件**：`src/engine/ui/ui_element.cpp`

```cpp
void UIElement::setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset) {
    // 首次进入 WorldAnchor 模式时，previous 与 current 相同（避免从原点插值飞入）
    if (positioning_mode_ != PositioningMode::WorldAnchor) {
        previous_world_anchor_ = world_pos;
    } else {
        previous_world_anchor_ = world_anchor_;
    }
    positioning_mode_ = PositioningMode::WorldAnchor;
    world_anchor_ = world_pos;
    world_anchor_offset_ = screen_offset;
    invalidateLayout();  // 传播到子节点
}

void UIElement::clearWorldAnchor() {
    positioning_mode_ = PositioningMode::Screen;
    world_anchor_ = {0.0f, 0.0f};
    previous_world_anchor_ = {0.0f, 0.0f};
    world_anchor_offset_ = {0.0f, 0.0f};
    invalidateLayout();  // 恢复 Screen 模式后子树需要重新布局
}

void UIElement::applyWorldAnchorPosition(glm::vec2 screen_pos) {
    const glm::vec2 final_size = layout_override_size_.value_or(size_);
    const glm::vec2 new_position = screen_pos - final_size * pivot_;

    // 如果位置实际没变，跳过子树脏化（避免每帧无谓传播）
    if (!layout_dirty_ && sameVec2(layout_position_, new_position) && sameVec2(layout_size_, final_size)) {
        return;
    }

    layout_position_ = new_position;
    layout_size_ = final_size;
    layout_dirty_ = false;

    // 关键：向子节点传播脏标记，确保子节点在 ensureLayout() 中基于新的父位置重算
    // 与 setPosition() -> invalidateLayout(true) 保持一致的语义
    for (auto& child : children_) {
        if (child) {
            child->invalidateLayout(true);
        }
    }
}
```

**为什么需要子树传播**（修正 Codex 审阅问题 #1）：

现有布局体系中，子节点的 `ensureLayout()` 通过 `parent_->getContentBounds()` 读取父节点的 `layout_position_`（`ui_element.cpp:324`）。如果父节点 `layout_position_` 变了但子节点 `layout_dirty_` 仍为 false，子节点会使用旧的缓存位置，导致渲染和命中检测错位。

`setPosition()` 通过 `invalidateLayout(true)`（`ui_element.cpp:407`）递归脏化子节点来解决这个问题。`applyWorldAnchorPosition()` 必须遵循同样的契约。

**性能保障**：增加了 `sameVec2` diff guard — 如果投影结果和上一帧完全一致（静止不动的实体），跳过子树脏化，避免每帧无意义的布局重算。

### 3. 引擎层：UIElement::ensureLayout() 适配

**改动文件**：`src/engine/ui/ui_element.cpp`

在 `ensureLayout()` 中，当 `positioning_mode_ == WorldAnchor` 且 `parent_` 存在时：

```cpp
void UIElement::ensureLayout() const {
    if (!layout_dirty_) return;
    ++g_layout_recompute_counter;

    if (!parent_) {
        // 根元素逻辑不变
        layout_size_ = layout_override_size_.value_or(size_);
        layout_position_ = position_;
        layout_dirty_ = false;
        return;
    }

    // 世界锚点模式：layout_position_ 已由 resolveWorldAnchors() -> applyWorldAnchorPosition() 写入
    // 此处只需确认 size 并调用 onLayout()
    if (positioning_mode_ == PositioningMode::WorldAnchor) {
        // 仅限根节点直接子元素使用（约束 C1）
        if (parent_->getParent() != nullptr) {
            spdlog::warn("UIElement::ensureLayout: WorldAnchor mode on non-root-child element (id={}), "
                         "falling back to Screen mode.", id_);
            // fallback: 走正常 Screen 路径（不改 positioning_mode_，仅本次布局 fallback）
        } else {
            layout_size_ = layout_override_size_.value_or(size_);
            // layout_position_ 保持 applyWorldAnchorPosition() 写入的值
            layout_dirty_ = false;
            const_cast<UIElement*>(this)->onLayout();
            return;
        }
    }

    // 原有屏幕定位逻辑完全不变 ...
    auto parent_content = parent_->getContentBounds();
    // ... (后续代码不变)
}
```

### 4. 引擎层：UIManager 渲染阶段统一插值投影

**改动文件**：`src/engine/ui/ui_manager.h` / `ui_manager.cpp`

```cpp
// ui_manager.h 变更
public:
    void render(engine::core::Context&, float interpolation_alpha);  // 新增 alpha 参数

private:
    void resolveWorldAnchors(const engine::render::Camera& camera, float interpolation_alpha);
```

```cpp
// ui_manager.cpp
#include "engine/render/camera.h"

// update() 不再调用 resolveWorldAnchors — 保持原有逻辑不变
void UIManager::update(float delta_time, engine::core::Context& context) {
    UIElement::resetLayoutRecomputeCounter();

    processMouseHover();

    if (root_element_ && root_element_->isVisible()) {
        root_element_->update(delta_time, context);
    }

    spdlog::trace("UIManager::update layout_recompute_count={}",
                  UIElement::consumeLayoutRecomputeCounter());
}

// render() 新增 interpolation_alpha 参数，在渲染前执行世界锚点插值投影
void UIManager::render(engine::core::Context& context, float interpolation_alpha) {
    if (!root_element_ || !root_element_->isVisible()) return;

    // 在渲染阶段执行世界锚点插值投影（此时相机已被 GameScene 设为插值位置）
    resolveWorldAnchors(context.getCamera(), interpolation_alpha);

    root_element_->render(context);
    renderCursor(context);
}

void UIManager::resolveWorldAnchors(const engine::render::Camera& camera,
                                    float interpolation_alpha) {
    if (!root_element_) return;

    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);

    // 约束 C1：只遍历根节点的直接子元素，不递归
    for (auto& child : root_element_->getChildren()) {
        if (!child || !child->isVisible()) continue;
        if (child->getPositioningMode() != PositioningMode::WorldAnchor) continue;

        // 对世界坐标做插值（与 RenderSystem 对实体 position 的插值一致）
        const glm::vec2 interpolated_anchor = glm::mix(
            child->getPreviousWorldAnchor(),
            child->getWorldAnchor(),
            clamped_alpha);

        const glm::vec2 screen_pos =
            camera.worldToScreen(interpolated_anchor) + child->getWorldAnchorOffset();
        child->applyWorldAnchorPosition(screen_pos);
    }
}
```

**关键变化**：
- `resolveWorldAnchors()` 从 `update()` 移至 `render()`，确保使用**插值相机**。
- 使用 `glm::mix(previous_world_anchor_, world_anchor_, alpha)` 对世界锚点坐标做插值。
- `render()` 签名新增 `float interpolation_alpha` 参数。

### 5. 模块职责划分（重构后）

```
┌──────────────────────────────────────────────────────────────────┐
│ 引擎层 (engine::ui)                                              │
│                                                                  │
│  UIElement                                                       │
│    ├─ positioning_mode_: Screen | WorldAnchor                    │
│    ├─ world_anchor_: glm::vec2            (当前世界坐标)           │
│    ├─ previous_world_anchor_: glm::vec2   (上一次世界坐标，插值用) │
│    ├─ world_anchor_offset_: glm::vec2     (屏幕偏移)              │
│    ├─ setWorldAnchor()   — 自动快照 previous，首次 prev=current   │
│    ├─ clearWorldAnchor() — 清零 prev/current/offset              │
│    └─ applyWorldAnchorPosition()      (UIManager 友元调用)        │
│         写入 layout_position_ + 向子树传播脏标记                   │
│                                                                  │
│  UIManager                                                       │
│    └─ render(context, alpha) 中调用 resolveWorldAnchors()         │
│       仅遍历根节点直接子元素（约束 C1）                             │
│       glm::mix(prev_anchor, anchor, alpha) → worldToScreen       │
│       使用插值相机（GameScene 在 render 阶段设置）                  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 游戏层 (game::ui)                                                │
│                                                                  │
│  DialogueBubbleView (纯视图)                                      │
│    setText() / setVisible() / buildSkin() / buildLayout()         │
│    不再持有 dispatcher/channel/world_position                      │
│                                                                  │
│  DialogueBubbleController (事件路由)                               │
│    订阅 DialogueShow/Move/HideEvent                               │
│    按 channel 查找 slot（channel -> view_id + offset）并解析 View： │
│      view->setWorldAnchor(world_pos, offset)                      │
│      view->setText(formatted_text)                                │
│      view->setVisible(true/false)                                 │
│    若 view_id 已失效（动态移除）则自动跳过并清理 slot               │
│    不做坐标投影（由引擎层 UIManager 统一处理）                       │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 业务系统 (零改动)                                                 │
│  DialogueSystem ──── trigger(DialogueShow/MoveEvent)             │
│  ChestSystem ─────── trigger(DialogueShow/MoveEvent, channel=1)  │
│  ItemUseSystem ───── trigger(DialogueShow/MoveEvent, channel=2)  │
│  Hide 事件保持 enqueue（兼容现有脚本/调用路径）                     │
└──────────────────────────────────────────────────────────────────┘
```

### 6. 游戏层：DialogueBubble → DialogueBubbleView

**位置**：`src/game/ui/dialogue_bubble_view.h` / `.cpp`（重命名自 `dialogue_bubble.*`）

**移除**：
- `dispatcher_`、`world_position_`、`offset_`、`channel_`
- `subscribeEvents()` / `onShowEvent()` / `onMoveEvent()` / `onHideEvent()`
- `setWorldPosition()` / `setOffset()`
- `update()` override（世界锚点投影已由引擎层处理）

**保留**：
- `text_renderer_`、`panel_`、`label_`、`bubble_image_`、`padding_`、`font_id_`、`font_size_`
- `setText(std::string_view text)`
- `buildSkin()` / `buildLayout()` / `refreshLayoutFromText()`

**简化后的构造函数**：
```cpp
DialogueBubbleView(engine::core::Context& context,
                   engine::render::TextRenderer& text_renderer,
                   entt::id_type font_id = entt::null,
                   int font_size = DEFAULT_UI_FONT_SIZE_PX);
```

### 7. 游戏层：DialogueBubbleController

**位置**：`src/game/ui/dialogue_bubble_controller.h` / `.cpp`
**命名空间**：`game::ui`

```cpp
class DialogueBubbleController {
public:
    DialogueBubbleController(entt::dispatcher& dispatcher, engine::ui::UIManager& ui_manager);
    ~DialogueBubbleController(); // RAII disconnect

    void registerBubble(std::uint8_t channel,
                        entt::id_type view_id,
                        glm::vec2 screen_offset = {0.0f, -4.0f});
    void unregisterBubble(std::uint8_t channel);

private:
    entt::dispatcher& dispatcher_;
    engine::ui::UIManager& ui_manager_;
    struct BubbleSlot {
        entt::id_type view_id{entt::null};
        glm::vec2 screen_offset{0.0f, -4.0f};
    };
    std::unordered_map<std::uint8_t, BubbleSlot> slots_;

    void onShow(const game::defs::DialogueShowEvent& evt);
    void onMove(const game::defs::DialogueMoveEvent& evt);
    void onHide(const game::defs::DialogueHideEvent& evt);

    DialogueBubbleView* resolveView(std::uint8_t channel);
    static std::string formatDialogueText(std::string_view speaker, std::string_view text);
};
```

**核心逻辑**：
- `onShow`：按 channel 查找 slot，`resolveView()` 成功后调用 `view->setWorldAnchor(world_pos, offset)` + `view->setText(formatted)` + `view->setVisible(true)`；未注册或 view 已失效时安全忽略。
- `onMove`：按 channel 查找 slot，调用 `view->setWorldAnchor(new_pos, offset)` 更新世界坐标（无队列延迟）。
- `onHide`：按 channel 查找 slot，调用 `view->clearWorldAnchor()` + `view->setVisible(false)`。
- `resolveView()`：通过 `ui_manager_.getRootElement()->getChildById(view_id)` 动态解析 `DialogueBubbleView`；解析失败时自动 `erase(channel)` 防止后续野指针问题。
- **不需要 `update()` 方法** — 投影由 UIManager 统一处理。

### 8. GameScene 接入

**`game_scene.h` 变更**：
```diff
- class DialogueBubble;
+ class DialogueBubbleView;
+ class DialogueBubbleController;

- game::ui::DialogueBubble* dialogue_bubble_{nullptr};
+ std::unique_ptr<game::ui::DialogueBubbleController> dialogue_controller_;
```

**`game_scene.cpp::initUI()` 变更**：
```cpp
using namespace entt::literals;
constexpr entt::id_type BUBBLE_CH0_ID = "dialogue_bubble_ch0"_hs;
constexpr entt::id_type BUBBLE_CH1_ID = "dialogue_bubble_ch1"_hs;
constexpr entt::id_type BUBBLE_CH2_ID = "dialogue_bubble_ch2"_hs;

// 1. 创建 controller（dispatcher + ui_manager，用于按 id 解析 view）
dialogue_controller_ = std::make_unique<game::ui::DialogueBubbleController>(dispatcher_ref, *ui_manager_);

// 2. 创建 DialogueBubbleView（不传 dispatcher/channel）
auto bubble_0 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
bubble_0->setId(BUBBLE_CH0_ID);
ui_manager_->addElement(std::move(bubble_0));

auto bubble_1 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
bubble_1->setId(BUBBLE_CH1_ID);
ui_manager_->addElement(std::move(bubble_1));

auto bubble_2 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
bubble_2->setId(BUBBLE_CH2_ID);
ui_manager_->addElement(std::move(bubble_2));

// 3. 注册到 controller（channel -> view_id；支持动态移除后安全失效）
dialogue_controller_->registerBubble(0, BUBBLE_CH0_ID);
dialogue_controller_->registerBubble(1, BUBBLE_CH1_ID);
dialogue_controller_->registerBubble(2, BUBBLE_CH2_ID, {0.0f, -56.0f});
```

**`game_scene.cpp::render()` 变更 — 延迟相机恢复**：

当前代码在实体渲染完毕后立即恢复相机（`game_scene.cpp:215-216`），然后才调 `Scene::render()`。重构后需要让 `Scene::render()`（含 UIManager）在**插值相机**下执行，确保 `resolveWorldAnchors()` 使用正确的相机位置。

```cpp
void GameScene::render(float interpolation_alpha) {
    // ... 省略 abort_to_title_ 检查 ...
    auto& camera = context_.getCamera();
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);
    const glm::vec2 camera_position_before = camera.getPosition();
    if (has_previous_camera_position_) {
        const glm::vec2 render_camera_position =
            glm::mix(previous_camera_position_, camera_position_before, clamped_alpha);
        camera.setPosition(render_camera_position);
    }

    // 实体渲染（使用插值相机）
    systems_->ysort_system->render(registry_, clamped_alpha);
    systems_->render_system->render(registry_, renderer, camera, clamped_alpha);
    systems_->light_system->render(registry_, renderer, clamped_alpha);
    systems_->render_target_system->render(renderer);

    // 变更：先渲染 UI（仍在插值相机下），再恢复相机
    // 这确保 UIManager::render() -> resolveWorldAnchors() 使用插值相机
    Scene::render(interpolation_alpha);

    // 恢复相机位置（移到 Scene::render 之后）
    if (has_previous_camera_position_) {
        camera.setPosition(camera_position_before);
    }
}
```

**为什么屏幕固定 UI 不受影响**：屏幕固定 UI（InventoryUI、HotbarUI、TimeClock 等）使用屏幕坐标定位，不调用 `camera.worldToScreen()`，因此相机处于何种状态对它们完全没有影响。

### 8.1. Scene 基类适配

**改动文件**：`src/engine/scene/scene.h` / `scene.cpp`

`Scene::render()` 需要将 `interpolation_alpha` 传递给 `UIManager::render()`：

```cpp
void Scene::render(float interpolation_alpha) {
    if (!is_initialized_) return;
    if (ui_manager_) {
        ui_manager_->render(context_, interpolation_alpha);  // 新增 alpha 参数
    }
}
```

**`game_scene.cpp::update()` 不变**：
```cpp
void GameScene::update(float delta_time) {
    // 不需要手动调用 controller->update()
    Scene::update(delta_time);
}
```

**`game_scene.cpp::clean()` 变更**：
```cpp
void GameScene::clean() {
    auto& dispatcher = context_.getDispatcher();
    // 兼容脚本/遗留 enqueue 发送路径，场景切换时主动清空
    dispatcher.clear<game::defs::DialogueShowEvent>();
    dispatcher.clear<game::defs::DialogueMoveEvent>();
    dispatcher.clear<game::defs::DialogueHideEvent>();

    // controller 必须在 UIManager 析构前销毁（断开事件订阅）
    dialogue_controller_.reset();
    // ... 其余清理逻辑（保持原有顺序）
}
```

### 9. 数据流（重构后）

```
DialogueSystem              DialogueBubbleController         DialogueBubbleView        UIManager
    │                              │                              │                       │
    │            ── fixedUpdate() 阶段（同步 trigger） ──           │                       │
    ├─ trigger(ShowEvent) ────────→ onShow()                       │                       │
    │                              ├─ formatDialogueText()        │                       │
    │                              ├─ resolveView(channel,id)     │                       │
    │                              ├─ view->setWorldAnchor() ────→ prev = current         │
    │                              │                              │ current = new_pos      │
    │                              │                              │ invalidateLayout()     │
    │                              ├─ view->setText(formatted) ──→ setText()              │
    │                              └─ view->setVisible(true) ───→ setVisible()            │
    │                              │                              │                       │
    ├─ trigger(MoveEvent) ────────→ onMove()                      │                       │
    │                              └─ view->setWorldAnchor() ────→ prev = current         │
    │                              │                              │ current = new_pos      │
    │                              │                              │                       │
    │                              │         ── render(alpha) 阶段（插值相机下） ──         │
    │                              │                              │                       │
    │                              │                              │  render(ctx, alpha)    │
    │                              │                              │  resolveWorldAnchors() │
    │                              │                              │  mix(prev, cur, alpha) │
    │                              │                              │←─ worldToScreen()      │
    │                              │                              │←─ applyPosition()      │
    │                              │                              │   + 子树 invalidate    │
    │                              │                              │                       │
    │                              │                              │  root->render()        │
    │                              │                              │←─ ensureLayout()       │
    │                              │                              │   (子节点基于新父位置)   │
    │                              │                              │                       │
    │                     ── dispatcher_->update() 阶段（队列事件） ── │                     │
    ├─ enqueue(HideEvent) ────────→ onHide()                      │                       │
    │                              ├─ view->clearWorldAnchor() ──→ mode=Screen            │
    │                              └─ view->setVisible(false) ──→ setVisible()            │
```

### 10. 后续世界锚点 UI 使用示例

引擎层做好之后，未来新增世界锚点 UI 极其简单：

```cpp
// 示例：伤害飘字
auto damage_text = std::make_unique<engine::ui::UILabel>(text_renderer, "-42", ...);
damage_text->setWorldAnchor(enemy_world_pos, {0.0f, -20.0f});
ui_manager->addElement(std::move(damage_text));

// 示例：任务标记图标
auto quest_icon = std::make_unique<engine::ui::UIImage>(quest_icon_image, ...);
quest_icon->setWorldAnchor(npc_world_pos, {0.0f, -32.0f});
ui_manager->addElement(std::move(quest_icon));

// 不需要任何 controller、不需要注册 slot、不需要手动投影
// UIManager::render() 自动插值投影，跟随平滑无抖动
```

## 需要新增的文件
- `src/game/ui/dialogue_bubble_view.h`（重命名自 `dialogue_bubble.h`）
- `src/game/ui/dialogue_bubble_view.cpp`（重命名自 `dialogue_bubble.cpp`）
- `src/game/ui/dialogue_bubble_controller.h`
- `src/game/ui/dialogue_bubble_controller.cpp`
- `tests/engine/ui/ui_world_anchor_test.cpp`

## 需要删除的文件
- `src/game/ui/dialogue_bubble.h`
- `src/game/ui/dialogue_bubble.cpp`

## 预计改动文件
- `src/engine/ui/ui_element.h` — 新增 PositioningMode、world_anchor/previous_world_anchor 成员与接口
- `src/engine/ui/ui_element.cpp` — `ensureLayout()` 适配 WorldAnchor 模式、新增 `setWorldAnchor()`（含 previous 快照）/ `clearWorldAnchor()` / `applyWorldAnchorPosition()`
- `src/engine/ui/ui_manager.h` — `render()` 签名新增 `interpolation_alpha`、新增 `resolveWorldAnchors()` 私有方法
- `src/engine/ui/ui_manager.cpp` — `render()` 调用 `resolveWorldAnchors()`（含插值投影）、实现遍历
- `src/engine/scene/scene.h` — `Scene::render()` 中 UIManager 调用传入 alpha（无签名变更）
- `src/engine/scene/scene.cpp` — `Scene::render()` 传 `interpolation_alpha` 给 `ui_manager_->render()`
- `src/game/scene/game_scene.h` — 前置声明与成员类型变更
- `src/game/scene/game_scene.cpp` — `render()` 延迟相机恢复到 `Scene::render()` 之后；initUI / clean 接入
- `src/game/system/system_helpers.h` — `emitDialogueBubbleShow/Move` 由 `enqueue` 改为 `trigger`（零 tick 延迟）
- `src/game/script/tinyfarm_script_module.cpp` — `tf.dialogue.show` 同步改为 `trigger`（与运行时时序契约一致）
- `src/CMakeLists.txt` — 更新源文件列表（删旧增新）
- `tests/CMakeLists.txt` — 新增测试文件
- `docs/engine/layout-contract.md` — 新增 WorldAnchor 定位模式章节（含插值说明）

## 实现步骤

### Step 1：引擎层 UIElement 新增世界锚点支持（含插值）
- `ui_element.h`：新增 `PositioningMode` 枚举、`world_anchor_` / `previous_world_anchor_` / `world_anchor_offset_` 成员、公开接口（含 `getPreviousWorldAnchor()`）、UIManager 友元声明。
- `ui_element.cpp`：实现 `setWorldAnchor()`（含 previous 快照、首次进入 prev=current）/ `clearWorldAnchor()` / `applyWorldAnchorPosition()`（含子树脏标记传播与 diff guard）。
- `ui_element.cpp`：`ensureLayout()` 新增 WorldAnchor 分支（含非直接子元素 fallback warn）。

### Step 2：引擎层 UIManager 渲染阶段插值投影
- `ui_manager.h`：`render()` 签名新增 `float interpolation_alpha`；新增 `resolveWorldAnchors(camera, alpha)` 私有方法声明。
- `ui_manager.cpp`：`render()` 中在 `root->render()` 之前调用 `resolveWorldAnchors()`；`resolveWorldAnchors()` 使用 `glm::mix(prev, current, alpha)` 插值锚点坐标，遍历根节点直接子元素。
- `scene.cpp`：`Scene::render()` 传 `interpolation_alpha` 给 `ui_manager_->render()`。

### Step 3：新增引擎层单元测试（必做）
- 新增 `tests/engine/ui/ui_world_anchor_test.cpp`，覆盖：
  - `setWorldAnchor()` 后 `getPositioningMode()` 返回 `WorldAnchor`。
  - `clearWorldAnchor()` 后恢复 `Screen` 模式，`world_anchor_` 清零。
  - `applyWorldAnchorPosition()` 正确考虑 pivot 计算 `layout_position_`。
  - `applyWorldAnchorPosition()` 后子节点 `layout_dirty_` 为 true。
  - diff guard：同一位置连续 apply 不重复脏化子节点。
  - `resolveWorldAnchors()` 不影响 Screen 模式元素的 `layout_position_`。
  - 非根直接子元素设置 WorldAnchor 后 `ensureLayout()` fallback 到 Screen（warn 日志）。

### Step 4：更新布局契约文档（必做）
- `docs/engine/layout-contract.md`：新增"3.6 世界锚点定位模式"章节，描述 PositioningMode、约束 C1/C2/C3、applyWorldAnchorPosition 的脏标记传播语义。

### Step 5：重构 DialogueBubble 为 DialogueBubbleView
- 重命名文件和类。
- 移除 dispatcher/channel/world_position 相关成员和方法。
- 简化构造函数和 update()。

### Step 6：新增 DialogueBubbleController
- 创建 `dialogue_bubble_controller.h/.cpp`。
- 实现事件订阅/断开（RAII）。
- 实现 `registerBubble()/unregisterBubble()` 与事件处理（调用 view 的 `setWorldAnchor` / `setText` / `setVisible`）。
- slot 使用 `view_id` 而非 raw pointer，事件触发时动态解析 view；解析失败自动清理 slot。
- 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法。

### Step 7：事件发送时序优化（零 tick 跟随延迟）
- `system_helpers.h`：`emitDialogueBubbleShow/Move` 改用 `dispatcher.trigger(evt)`。
- `tinyfarm_script_module.cpp`：`tf.dialogue.show` 改用 `dispatcher.trigger(evt)`，避免脚本路径出现 1 tick 延迟。

### Step 8：GameScene 接入
- `game_scene.h`：更新前置声明与成员。
- `game_scene.cpp::render()`：延迟相机恢复——将 `camera.setPosition(camera_position_before)` 移到 `Scene::render()` 之后，确保 UI 在插值相机下渲染。
- `game_scene.cpp::initUI()`：创建 controller + views 并注册。
- `game_scene.cpp::clean()`：清理事件队列，先销毁 controller。

### Step 9：更新构建配置
- `src/CMakeLists.txt`：删除旧文件，添加新文件。
- `tests/CMakeLists.txt`：新增 `ui_world_anchor_test.cpp`。

### Step 10：编译验证与回归
- 全量编译通过。
- 运行 `ctest`，新增测试和既有布局测试全部通过。
- 手动验证三类 channel 气泡行为。
- 验证场景切换/暂停菜单无崩溃。

## 待办清单（用于追踪）
- [ ] T1 `ui_element.h` 新增 PositioningMode 枚举与 world_anchor/previous_world_anchor 成员/接口/友元
- [ ] T1.1 `ui_element.cpp` 实现 `setWorldAnchor()`（含 previous 快照、首次 prev=current）/ `clearWorldAnchor()`
- [ ] T1.2 `ui_element.cpp` 实现 `applyWorldAnchorPosition()`（含子树脏标记传播 + diff guard）
- [ ] T1.3 `ui_element.cpp` 适配 `ensureLayout()` WorldAnchor 分支（含非直接子元素 fallback warn）
- [ ] T2 `ui_manager.h/.cpp` `render()` 签名新增 alpha；新增 `resolveWorldAnchors(camera, alpha)` 含 `glm::mix` 插值
- [ ] T2.1 `scene.cpp` `Scene::render()` 传 `interpolation_alpha` 给 `ui_manager_->render()`
- [ ] T2.2 `game_scene.cpp::render()` 延迟相机恢复到 `Scene::render()` 之后
- [ ] T3 新增 `tests/engine/ui/ui_world_anchor_test.cpp`（必做）
- [ ] T3.1 测试：模式切换、previous 快照、首次进入 prev=current、pivot 计算、子树脏化传播、diff guard、插值投影、Screen 元素无副作用、非直接子元素 fallback
- [ ] T4 更新 `docs/engine/layout-contract.md` 新增世界锚点章节（含插值说明）（必做）
- [ ] T5 重命名 `DialogueBubble` → `DialogueBubbleView` 并瘦身
- [ ] T5.1 移除 dispatcher/channel/world_position 相关成员和方法
- [ ] T5.2 简化构造函数
- [ ] T6 新增 `DialogueBubbleController` 实现事件路由
- [ ] T6.1 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法
- [ ] T6.2 `DialogueBubbleController` 改为 `view_id` 注册（新增 `unregisterBubble`，动态解析失效自动清理）
- [ ] T7 `system_helpers.h`：`emitDialogueBubbleShow/Move` 改 `trigger`
- [ ] T7.1 `tinyfarm_script_module.cpp`：`tf.dialogue.show` 改 `trigger`
- [ ] T8 更新 `game_scene.h/.cpp` 接入（initUI / clean）
- [ ] T9 更新 `src/CMakeLists.txt` 和 `tests/CMakeLists.txt`
- [ ] T10 全量编译 + `ctest` 通过
- [ ] T11 手动回归验证三类 channel 气泡行为（含插值平滑性验证与零 tick 跟随）

## 测试计划

### 必做：引擎层单元测试
新增 `tests/engine/ui/ui_world_anchor_test.cpp`：
- `SetWorldAnchor_SetsPositioningMode`：`setWorldAnchor()` 后 mode 为 `WorldAnchor`，值正确存储。
- `SetWorldAnchor_FirstCall_PreviousEqualsCurrent`：首次调用 `setWorldAnchor(pos)` 后 `getPreviousWorldAnchor() == pos`（避免从原点插值飞入）。
- `SetWorldAnchor_SubsequentCall_SnapshotsPrevious`：第二次调用时 `previous` 等于第一次的 `current`。
- `ClearWorldAnchor_RestoresScreenMode`：`clearWorldAnchor()` 后 mode 为 `Screen`，anchor/previous 清零。
- `ApplyWorldAnchorPosition_ConsidersPivot`：pivot={0.5, 1.0} 时 `layout_position_` = screen_pos - size * pivot。
- `ApplyWorldAnchorPosition_InvalidatesChildren`：apply 后子节点 `layout_dirty_` 为 true。
- `ApplyWorldAnchorPosition_DiffGuard_SkipsWhenUnchanged`：相同位置连续 apply 不脏化子节点。
- `ResolveWorldAnchors_InterpolatesPosition`：`previous=(0,0)`, `current=(100,0)`, `alpha=0.5` → 投影世界坐标为 `(50,0)`。
- `ResolveWorldAnchors_IgnoresScreenElements`：Screen 模式元素的 `layout_position_` 不被修改。
- `WorldAnchor_OnNonRootChild_FallsBackToScreen`：非根直接子元素设置 WorldAnchor 后 `ensureLayout()` 走 Screen 路径。
- `ClearWorldAnchor_UsesExistingPosition`：clear 后使用 `position_` 参与正常布局。

### 必做：既有布局测试回归
- `tests/engine/ui/ui_layout_invalidation_test.cpp` — 确认新增代码不破坏现有脏标记语义。
- `tests/engine/ui/ui_stack_layout_test.cpp` / `ui_grid_layout_test.cpp` — 确认布局容器行为不变。

### 必做：手动回归
- NPC 对话（channel 0）：靠近 NPC 交互，气泡出现在头顶，跟随 NPC 移动，离开后消失。
- **零 tick 跟随**：NPC 移动时，气泡应与实体在同一渲染帧内同步更新，不出现“落后一个 fixed tick”的视觉延迟。
- **插值平滑性**：NPC 移动时，气泡应紧密跟随无抖动（对比重构前行为）；相机快速平移时气泡不应产生明显偏移。
- 开箱通知（channel 1）：打开宝箱，通知气泡正确显示。
- 物品使用提示（channel 2）：使用物品，提示气泡出现在正确偏移位置（-56px）。
- **首次显示无飞入**：气泡首次出现时不应从屏幕原点/角落飞入（`previous == current` 保障）。
- 未注册 channel（例如脚本发送 channel=255）：无崩溃、事件被安全忽略。
- 动态移除 BubbleView（运行时 removeChild）后，再收到该 channel 事件：无崩溃，controller 自动清理失效 slot。
- 场景切换：切换地图后无崩溃，旧气泡不残留。
- 暂停菜单：暂停/恢复后气泡行为正常。
- 屏幕固定 UI 不受影响：InventoryUI、HotbarUI、TimeClock 等位置和交互正常。

### 可选：DialogueBubbleController 单元测试
- 事件路由测试：不同 channel 事件仅影响对应 View。
- 未注册 channel 安全忽略。
- 已注册但 view_id 失效（视图已移除）时自动清理 slot，不发生野指针访问。
- `formatDialogueText()` 文本格式化测试。

## 验收标准（DoD）
- `UIElement` 支持 `PositioningMode::WorldAnchor`，根节点直接子元素可通过 `setWorldAnchor()` 锚定世界坐标，内置 `previous_world_anchor_` 自动快照。
- `UIManager::render()` 在渲染阶段统一处理所有世界锚点元素的插值投影（`glm::mix` + 插值相机），屏幕固定元素不受影响。
- `GameScene::render()` 延迟相机恢复，确保 UI 在插值相机下渲染，气泡与实体精灵位置一致无抖动。
- `DialogueShowEvent` / `DialogueMoveEvent` 使用 `trigger` 同步分发，气泡锚点更新不再落后一个 fixed tick。
- `applyWorldAnchorPosition()` 正确传播子树脏标记，子节点渲染/命中位置与父节点一致。
- `DialogueBubbleView` 不直接订阅业务事件，不持有 `channel` / `world_position` / `dispatcher`。
- `DialogueBubbleController` 仅做事件路由和文本格式化，不含投影逻辑；并通过 `view_id` 解析视图，支持动态移除安全退化。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem）零改动。
- 现有三类 channel（0/1/2）气泡行为与重构前一致（预期差异仅为修复“落后 1 tick”跟随延迟）。
- 新增引擎层单元测试全部通过，既有布局测试回归通过。
- `docs/engine/layout-contract.md` 同步更新。
- 全量编译通过，`ctest` 通过，手动回归通过。

## 风险与缓解
- **风险**：`applyWorldAnchorPosition()` 每帧对变动元素传播子树脏标记，开销是否可控。
  - **缓解**：(1) diff guard 确保静止元素跳过传播；(2) DialogueBubble 子树深度仅 2 层（panel + label），传播开销极小；(3) 与现有 `setPosition()` -> `invalidateLayout()` 路径开销完全一致。
- **风险**：友元关系 `UIManager` ↔ `UIElement`。
  - **缓解**：两者在同一引擎 UI 模块内，友元关系合理且范围最小（仅 `applyWorldAnchorPosition` 一个方法）。
- **风险**：场景销毁时 controller 的事件订阅未及时解绑，导致野指针。
  - **缓解**：controller 通过 RAII 管理订阅，`GameScene::clean()` 显式先销毁 controller。
- **风险**：场景切换时旧场景遗留队列事件污染新场景 UI。
  - **缓解**：`GameScene::clean()` 统一 `clear<DialogueShow/Move/HideEvent>()`。
- **风险**：未注册 channel 事件导致异常。
  - **缓解**：controller 使用 `unordered_map` + 查找守卫，未命中安全忽略。
- **风险**：运行时动态移除 BubbleView 后，controller 若持有 raw pointer 会触发野指针。
  - **缓解**：slot 存 `view_id`，事件触发时动态解析 view；解析失败自动清理 slot，并提供 `unregisterBubble()` 主动解绑。
- **风险**：`trigger` 同步分发可能引入系统间时序耦合（回调在 fixedUpdate 内立即执行）。
  - **缓解**：仅将位置相关事件（Show/Move）切为 `trigger`，回调逻辑保持纯 UI 状态写入（`setWorldAnchor/setText/setVisible`），不反向触发 gameplay 逻辑。
- **风险**：`GameScene::render()` 延迟相机恢复后，其他依赖 `Scene::render()` 后相机位置的逻辑受影响。
  - **缓解**：`Scene::render()` 之后紧接恢复相机；当前代码在 render 之后仅有 `dispatcher_->update()` 和帧结束，不依赖相机位置。其他场景（TitleScene、PauseMenu）无相机插值逻辑，不受影响。
- **风险**：`processMouseHover()` 在 update 阶段使用上一帧的投影位置做世界锚点元素的命中检测，可能导致 hover 判定偏移。
  - **缓解**：DialogueBubble 不是交互元素（非 UIInteractive），不参与 hover/click。未来若需要可交互的世界锚点 UI，可在 render 前额外做一次 hover 更新。

## 后续扩展方向（不在本次范围内）
- `FloatingText`（伤害飘字）：UILabel + `setWorldAnchor()` + fixedUpdate 中驱动 offset 动画。
- `QuestMarker`（任务标记）：UIImage + `setWorldAnchor()`。
- `HealthBar`（头顶血条）：UIPanel + UIProgressBar + `setWorldAnchor()`。
- `BuffIcon`（状态图标）：UIImage 组 + `setWorldAnchor()`。
- 以上全部只需 `setWorldAnchor()` 一行调用，无需新建 controller 或注册 slot。
- 性能优化：当世界锚点元素超过 100 个时，在 UIManager 中维护 `world_anchor_elements_` 列表替代遍历。
- 文本自动换行升级为基于像素宽度测量。
- 气泡显隐动画（淡入淡出）。
