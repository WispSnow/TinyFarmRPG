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
- `UIManager` 在 update 阶段统一处理所有世界锚点元素的坐标投影，屏幕固定元素不受影响。
- `DialogueBubble` 退化为纯视图组件（View），不直接持有 `dispatcher`、`channel`、`world_position_`。
- 新增游戏层 `DialogueBubbleController` 处理事件订阅与业务逻辑路由（精简的 controller，不含投影逻辑）。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem 等）无需任何改动。
- 为后续世界锚点 UI（`FloatingText`、`DamageNumber`、`QuestMarker`、`BuffIcon`、`HealthBar`）打好引擎级地基。

## 非目标
- 不修改 `DialogueShowEvent` / `DialogueMoveEvent` / `DialogueHideEvent` 事件结构。
- 不修改任何业务系统（DialogueSystem、ChestSystem、ItemUseSystem）的发送逻辑。
- 不在本次重构中解决文本自动换行的像素级测量问题（可后续独立推进）。
- 不引入新的渲染通道或改变渲染管线顺序。

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
- 未来根节点直接子元素中的世界 UI 设一个 mode + 世界坐标就能锚定，无需额外注册。
- 投影逻辑在引擎层统一维护，游戏层零重复。
- `UIElement::update()` 已接收 `Context&`，`Context` 已持有 `Camera&`，引擎层不引入新的外部依赖。

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
| `src/game/system/system_helpers.h:60-91` | `emitDialogueBubble*` | 业务系统发送事件的 helper（不改） |

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
  1. fixedUpdate()        — 业务系统/事件回调可修改 world_anchor_ / world_anchor_offset_
  2. UIManager::update()
     2a. resolveWorldAnchors()  — 对 WorldAnchor 元素执行 worldToScreen 投影
     2b. processMouseHover()
     2c. root->update()         — 递归更新 UI 树（含 ensureLayout）
  3. UIManager::render()
```

**规则**：
- 世界锚点的坐标和偏移应在 `fixedUpdate()` 或事件回调（`dispatcher.update()` 触发的同步回调）中修改，确保在同帧 `resolveWorldAnchors()` 中生效。
- 如果在 UI 元素的 `update()` 中修改 `world_anchor_offset_`（例如飘字动画），改动将在**下一帧**的 `resolveWorldAnchors()` 中生效（1 帧延迟）。这对于平滑动画通常可接受；若需要零延迟，应在 `fixedUpdate` 或专用动画系统中驱动。

### C4. WorldAnchor 与 onLayout 约束（本次范围）

- 本次 `WorldAnchor` 仅用于**非布局容器**元素（如 `DialogueBubbleView`、`UILabel`、`UIImage`、简单 `UIPanel`）。
- 不将 `UIStackLayout`、`UIGridLayout`、`UIProgressBar` 等依赖 `onLayout()` 驱动子布局的容器直接设为 `WorldAnchor`。
- 这样可以避免 `applyWorldAnchorPosition()` 先写入布局缓存后导致容器 `onLayout()` 时机不明确的问题。
- 若后续确实需要“WorldAnchor + 布局容器”组合，需在引擎层补充专门语义（例如显式触发一次 `onLayout()` 或拆分容器职责）后再放开约束。

### C3. clearWorldAnchor() 后的位置恢复

调用 `clearWorldAnchor()` 后：
- `positioning_mode_` 恢复为 `Screen`。
- 元素使用调用 `clearWorldAnchor()` 前最后一次 `setPosition()` 设置的 `position_` 值重新参与正常布局计算。
- 如果从未调用过 `setPosition()`，则使用构造时的初始 `position_`（通常为 `{0, 0}`）。
- `world_anchor_` 和 `world_anchor_offset_` 被清零。

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
    glm::vec2 world_anchor_{0.0f, 0.0f};          // 世界坐标锚点
    glm::vec2 world_anchor_offset_{0.0f, 0.0f};   // 投影后的屏幕偏移
```

**UIElement 新增公开接口**：

```cpp
public:
    void setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset = {0.0f, 0.0f});
    void clearWorldAnchor();
    [[nodiscard]] PositioningMode getPositioningMode() const { return positioning_mode_; }
    [[nodiscard]] const glm::vec2& getWorldAnchor() const { return world_anchor_; }
    [[nodiscard]] const glm::vec2& getWorldAnchorOffset() const { return world_anchor_offset_; }
```

**UIElement 新增 protected 友元方法**：

```cpp
protected:
    friend class UIManager;
    void applyWorldAnchorPosition(glm::vec2 screen_pos);
```

**接口语义**：
- `setWorldAnchor(pos, offset)`：设置 `positioning_mode_ = WorldAnchor`，存储世界坐标和屏幕偏移，调用 `invalidateLayout()` 传播脏标记。
- `clearWorldAnchor()`：设置 `positioning_mode_ = Screen`，清零 `world_anchor_` 和 `world_anchor_offset_`，调用 `invalidateLayout()` 恢复正常布局。
- `applyWorldAnchorPosition(screen_pos)`：由 UIManager 在投影阶段调用，写入 `layout_position_` 和 `layout_size_`，然后**向子节点传播脏标记**确保子树重新布局。

### 2. 引擎层：applyWorldAnchorPosition() 实现（含子树脏标记传播）

**改动文件**：`src/engine/ui/ui_element.cpp`

```cpp
void UIElement::setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset) {
    positioning_mode_ = PositioningMode::WorldAnchor;
    world_anchor_ = world_pos;
    world_anchor_offset_ = screen_offset;
    invalidateLayout();  // 传播到子节点
}

void UIElement::clearWorldAnchor() {
    positioning_mode_ = PositioningMode::Screen;
    world_anchor_ = {0.0f, 0.0f};
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

### 4. 引擎层：UIManager 统一投影

**改动文件**：`src/engine/ui/ui_manager.h` / `ui_manager.cpp`

```cpp
// ui_manager.h 新增私有方法
private:
    void resolveWorldAnchors(const engine::render::Camera& camera);
```

```cpp
// ui_manager.cpp
#include "engine/render/camera.h"

void UIManager::update(float delta_time, engine::core::Context& context) {
    UIElement::resetLayoutRecomputeCounter();

    // 新增：统一处理所有世界锚点元素的坐标投影（仅遍历根的直接子节点）
    resolveWorldAnchors(context.getCamera());

    processMouseHover();

    if (root_element_ && root_element_->isVisible()) {
        root_element_->update(delta_time, context);
    }

    spdlog::trace("UIManager::update layout_recompute_count={}",
                  UIElement::consumeLayoutRecomputeCounter());
}

void UIManager::resolveWorldAnchors(const engine::render::Camera& camera) {
    if (!root_element_) return;

    // 约束 C1：只遍历根节点的直接子元素，不递归
    for (auto& child : root_element_->getChildren()) {
        if (!child || !child->isVisible()) continue;
        if (child->getPositioningMode() != PositioningMode::WorldAnchor) continue;

        const glm::vec2 screen_pos =
            camera.worldToScreen(child->getWorldAnchor()) + child->getWorldAnchorOffset();
        child->applyWorldAnchorPosition(screen_pos);
    }
}
```

### 5. 模块职责划分（重构后）

```
┌──────────────────────────────────────────────────────────────────┐
│ 引擎层 (engine::ui)                                              │
│                                                                  │
│  UIElement                                                       │
│    ├─ positioning_mode_: Screen | WorldAnchor                    │
│    ├─ world_anchor_: glm::vec2        (世界坐标)                  │
│    ├─ world_anchor_offset_: glm::vec2 (屏幕偏移)                  │
│    ├─ setWorldAnchor() / clearWorldAnchor()                      │
│    └─ applyWorldAnchorPosition()      (UIManager 友元调用)        │
│         写入 layout_position_ + 向子树传播脏标记                   │
│                                                                  │
│  UIManager                                                       │
│    └─ update() 中调用 resolveWorldAnchors()                       │
│       仅遍历根节点直接子元素（约束 C1）                             │
│       对 WorldAnchor 模式的元素执行 worldToScreen + apply          │
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
│    按 channel 查找 View，调用:                                     │
│      view->setWorldAnchor(world_pos, offset)                      │
│      view->setText(formatted_text)                                │
│      view->setVisible(true/false)                                 │
│    不做坐标投影（由引擎层 UIManager 统一处理）                       │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 业务系统 (零改动)                                                 │
│  DialogueSystem ──── enqueue(DialogueShowEvent)                  │
│  ChestSystem ─────── enqueue(DialogueShowEvent, channel=1)       │
│  ItemUseSystem ───── enqueue(DialogueShowEvent, channel=2)       │
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
    DialogueBubbleController(entt::dispatcher& dispatcher);
    ~DialogueBubbleController(); // RAII disconnect

    void registerBubble(std::uint8_t channel, DialogueBubbleView* view,
                        glm::vec2 screen_offset = {0.0f, -4.0f});

private:
    entt::dispatcher& dispatcher_;
    struct BubbleSlot {
        DialogueBubbleView* view{nullptr};
        glm::vec2 screen_offset{0.0f, -4.0f};
    };
    std::unordered_map<std::uint8_t, BubbleSlot> slots_;

    void onShow(const game::defs::DialogueShowEvent& evt);
    void onMove(const game::defs::DialogueMoveEvent& evt);
    void onHide(const game::defs::DialogueHideEvent& evt);

    static std::string formatDialogueText(std::string_view speaker, std::string_view text);
};
```

**核心逻辑**：
- `onShow`：按 channel 查找 slot，命中后调用 `view->setWorldAnchor(world_pos, offset)` + `view->setText(formatted)` + `view->setVisible(true)`。未注册 channel 安全忽略。
- `onMove`：按 channel 查找 slot，调用 `view->setWorldAnchor(new_pos, offset)` 更新世界坐标。
- `onHide`：按 channel 查找 slot，调用 `view->clearWorldAnchor()` + `view->setVisible(false)`。
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
// 1. 创建 controller（仅需 dispatcher）
dialogue_controller_ = std::make_unique<game::ui::DialogueBubbleController>(dispatcher_ref);

// 2. 创建 DialogueBubbleView（不传 dispatcher/channel）
auto bubble_0 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
auto* bubble_0_ptr = bubble_0.get();
ui_manager_->addElement(std::move(bubble_0));

auto bubble_1 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
auto* bubble_1_ptr = bubble_1.get();
ui_manager_->addElement(std::move(bubble_1));

auto bubble_2 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
auto* bubble_2_ptr = bubble_2.get();
ui_manager_->addElement(std::move(bubble_2));

// 3. 注册到 controller（offset 由 controller 传给 setWorldAnchor）
dialogue_controller_->registerBubble(0, bubble_0_ptr);
dialogue_controller_->registerBubble(1, bubble_1_ptr);
dialogue_controller_->registerBubble(2, bubble_2_ptr, {0.0f, -56.0f});
```

**`game_scene.cpp::update()` 不变**：
```cpp
void GameScene::update(float delta_time) {
    // 不需要手动调用 controller->update()
    // UIManager::update() 内部已统一处理世界锚点投影
    Scene::update(delta_time);
}
```

**`game_scene.cpp::clean()` 变更**：
```cpp
void GameScene::clean() {
    auto& dispatcher = context_.getDispatcher();
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
    ├─ enqueue(ShowEvent) ────────→ onShow()                      │                       │
    │                              ├─ formatDialogueText()        │                       │
    │                              ├─ view->setWorldAnchor() ────→ mode=WorldAnchor       │
    │                              │                              │ invalidateLayout()     │
    │                              ├─ view->setText(formatted) ──→ setText()              │
    │                              └─ view->setVisible(true) ───→ setVisible()            │
    │                              │                              │                       │
    ├─ enqueue(MoveEvent) ────────→ onMove()                      │                       │
    │                              └─ view->setWorldAnchor() ────→ 更新 world_anchor_     │
    │                              │                              │                       │
    │                              │                              │    每帧 update()       │
    │                              │                              │  resolveWorldAnchors() │
    │                              │                              │←─ applyPosition()      │
    │                              │                              │   + 子树 invalidate    │
    │                              │                              │                       │
    │                              │                              │  root->update()        │
    │                              │                              │←─ ensureLayout()       │
    │                              │                              │   (子节点基于新父位置)   │
    │                              │                              │                       │
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
// UIManager::update() 自动处理
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
- `src/engine/ui/ui_element.h` — 新增 PositioningMode、world_anchor 成员与接口
- `src/engine/ui/ui_element.cpp` — `ensureLayout()` 适配 WorldAnchor 模式、新增 `setWorldAnchor()` / `clearWorldAnchor()` / `applyWorldAnchorPosition()`
- `src/engine/ui/ui_manager.h` — 新增 `resolveWorldAnchors()` 私有方法
- `src/engine/ui/ui_manager.cpp` — `update()` 调用 `resolveWorldAnchors()`、实现遍历
- `src/game/scene/game_scene.h` — 前置声明与成员类型变更
- `src/game/scene/game_scene.cpp` — initUI / clean 接入
- `src/CMakeLists.txt` — 更新源文件列表（删旧增新）
- `tests/CMakeLists.txt` — 新增测试文件
- `docs/engine/layout-contract.md` — 新增 WorldAnchor 定位模式章节

## 实现步骤

### Step 1：引擎层 UIElement 新增世界锚点支持
- `ui_element.h`：新增 `PositioningMode` 枚举、`world_anchor_` / `world_anchor_offset_` 成员、公开接口、UIManager 友元声明。
- `ui_element.cpp`：实现 `setWorldAnchor()` / `clearWorldAnchor()` / `applyWorldAnchorPosition()`（含子树脏标记传播与 diff guard）。
- `ui_element.cpp`：`ensureLayout()` 新增 WorldAnchor 分支（含非直接子元素 fallback warn）。

### Step 2：引擎层 UIManager 统一投影
- `ui_manager.h`：新增 `resolveWorldAnchors()` 私有方法声明，`#include "engine/render/camera.h"` 前置声明。
- `ui_manager.cpp`：在 `update()` 的 `processMouseHover()` 之前调用 `resolveWorldAnchors()`；实现只遍历根节点直接子元素。

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
- 实现 `registerBubble()` 和事件处理（调用 view 的 `setWorldAnchor` / `setText` / `setVisible`）。
- 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法。

### Step 7：GameScene 接入
- `game_scene.h`：更新前置声明与成员。
- `game_scene.cpp::initUI()`：创建 controller + views 并注册。
- `game_scene.cpp::clean()`：清理事件队列，先销毁 controller。

### Step 8：更新构建配置
- `src/CMakeLists.txt`：删除旧文件，添加新文件。
- `tests/CMakeLists.txt`：新增 `ui_world_anchor_test.cpp`。

### Step 9：编译验证与回归
- 全量编译通过。
- 运行 `ctest`，新增测试和既有布局测试全部通过。
- 手动验证三类 channel 气泡行为。
- 验证场景切换/暂停菜单无崩溃。

## 待办清单（用于追踪）
- [ ] T1 `ui_element.h` 新增 PositioningMode 枚举与 world_anchor 成员/接口/友元
- [ ] T1.1 `ui_element.cpp` 实现 `setWorldAnchor()` / `clearWorldAnchor()`
- [ ] T1.2 `ui_element.cpp` 实现 `applyWorldAnchorPosition()`（含子树脏标记传播 + diff guard）
- [ ] T1.3 `ui_element.cpp` 适配 `ensureLayout()` WorldAnchor 分支（含非直接子元素 fallback warn）
- [ ] T2 `ui_manager.h/.cpp` 新增 `resolveWorldAnchors()` 并在 `update()` 中调用
- [ ] T3 新增 `tests/engine/ui/ui_world_anchor_test.cpp`（必做）
- [ ] T3.1 测试：模式切换、pivot 计算、子树脏化传播、diff guard、Screen 元素无副作用、非直接子元素 fallback
- [ ] T4 更新 `docs/engine/layout-contract.md` 新增世界锚点章节（必做）
- [ ] T5 重命名 `DialogueBubble` → `DialogueBubbleView` 并瘦身
- [ ] T5.1 移除 dispatcher/channel/world_position 相关成员和方法
- [ ] T5.2 简化构造函数
- [ ] T6 新增 `DialogueBubbleController` 实现事件路由
- [ ] T6.1 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法
- [ ] T7 更新 `game_scene.h/.cpp` 接入
- [ ] T8 更新 `src/CMakeLists.txt` 和 `tests/CMakeLists.txt`
- [ ] T9 全量编译 + `ctest` 通过
- [ ] T10 手动回归验证三类 channel 气泡行为

## 测试计划

### 必做：引擎层单元测试
新增 `tests/engine/ui/ui_world_anchor_test.cpp`：
- `SetWorldAnchor_SetsPositioningMode`：`setWorldAnchor()` 后 mode 为 `WorldAnchor`，值正确存储。
- `ClearWorldAnchor_RestoresScreenMode`：`clearWorldAnchor()` 后 mode 为 `Screen`，anchor 清零。
- `ApplyWorldAnchorPosition_ConsidersPivot`：pivot={0.5, 1.0} 时 `layout_position_` = screen_pos - size * pivot。
- `ApplyWorldAnchorPosition_InvalidatesChildren`：apply 后子节点 `layout_dirty_` 为 true。
- `ApplyWorldAnchorPosition_DiffGuard_SkipsWhenUnchanged`：相同位置连续 apply 不脏化子节点。
- `ResolveWorldAnchors_IgnoresScreenElements`：Screen 模式元素的 `layout_position_` 不被修改。
- `WorldAnchor_OnNonRootChild_FallsBackToScreen`：非根直接子元素设置 WorldAnchor 后 `ensureLayout()` 走 Screen 路径。
- `ClearWorldAnchor_UsesExistingPosition`：clear 后使用 `position_` 参与正常布局。

### 必做：既有布局测试回归
- `tests/engine/ui/ui_layout_invalidation_test.cpp` — 确认新增代码不破坏现有脏标记语义。
- `tests/engine/ui/ui_stack_layout_test.cpp` / `ui_grid_layout_test.cpp` — 确认布局容器行为不变。

### 必做：手动回归
- NPC 对话（channel 0）：靠近 NPC 交互，气泡出现在头顶，跟随 NPC 移动，离开后消失。
- 开箱通知（channel 1）：打开宝箱，通知气泡正确显示。
- 物品使用提示（channel 2）：使用物品，提示气泡出现在正确偏移位置（-56px）。
- 未注册 channel（例如脚本发送 channel=255）：无崩溃、事件被安全忽略。
- 场景切换：切换地图后无崩溃，旧气泡不残留。
- 暂停菜单：暂停/恢复后气泡行为正常。
- 屏幕固定 UI 不受影响：InventoryUI、HotbarUI、TimeClock 等位置和交互正常。

### 可选：DialogueBubbleController 单元测试
- 事件路由测试：不同 channel 事件仅影响对应 View。
- 未注册 channel 安全忽略。
- `formatDialogueText()` 文本格式化测试。

## 验收标准（DoD）
- `UIElement` 支持 `PositioningMode::WorldAnchor`，根节点直接子元素可通过 `setWorldAnchor()` 锚定世界坐标。
- `UIManager::update()` 统一处理所有世界锚点元素的投影，屏幕固定元素不受影响。
- `applyWorldAnchorPosition()` 正确传播子树脏标记，子节点渲染/命中位置与父节点一致。
- `DialogueBubbleView` 不直接订阅业务事件，不持有 `channel` / `world_position` / `dispatcher`。
- `DialogueBubbleController` 仅做事件路由和文本格式化，不含投影逻辑。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem）零改动。
- 现有三类 channel（0/1/2）气泡行为与重构前完全一致。
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
- **风险**：UI `update()` 中修改 world_anchor_offset_ 导致 1 帧滞后。
  - **缓解**：时序契约 C2 已明确定义此行为；若需零延迟，在 fixedUpdate 或事件回调中驱动。

## 后续扩展方向（不在本次范围内）
- `FloatingText`（伤害飘字）：UILabel + `setWorldAnchor()` + fixedUpdate 中驱动 offset 动画。
- `QuestMarker`（任务标记）：UIImage + `setWorldAnchor()`。
- `HealthBar`（头顶血条）：外层 `UIPanel`（WorldAnchor）+ 内层 `UIProgressBar`（Screen 子节点）。
- `BuffIcon`（状态图标）：UIImage 组 + `setWorldAnchor()`。
- 以上全部只需 `setWorldAnchor()` 一行调用，无需新建 controller 或注册 slot。
- 性能优化：当世界锚点元素超过 100 个时，在 UIManager 中维护 `world_anchor_elements_` 列表替代遍历。
- 文本自动换行升级为基于像素宽度测量。
- 气泡显隐动画（淡入淡出）。
