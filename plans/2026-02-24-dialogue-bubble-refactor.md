# DialogueBubble 世界锚点 UI 解耦重构

## 背景与问题

`DialogueBubble` 当前由 `UIManager` 管理，作为 UIElement 子树挂在 Scene 级 UI 树中。但它与其他屏幕固定 UI（InventoryUI、HotbarUI、TimeClock 等）有本质区别：

1. **世界坐标依赖** — 每帧在 `update()` 中执行 `camera.worldToScreen(world_position_)` 将世界坐标投影到屏幕坐标（`dialogue_bubble.cpp:106`），是 UIManager 中唯一依赖世界坐标的元素。
2. **事件订阅耦合** — 自身直接订阅 `DialogueShow/Move/HideEvent`（`dialogue_bubble.cpp:71-74`），承担了事件路由与 channel 分发的控制逻辑。
3. **文本排版逻辑** — `onShowEvent()` 内含 28 字符行宽的自动换行逻辑（`dialogue_bubble.cpp:117-146`），这是业务逻辑而非视图职责。
4. **职责过重** — 同一个类承担了事件订阅、世界坐标跟随、文本排版、UI 渲染四项职责，难以复用。

这导致 `UIManager` 的边界模糊：它本应专注于屏幕空间 UI 树管理（布局、输入、渲染调度），却隐式承载了一个世界锚点 UI 的全部生命周期。未来将新增大量世界锚点 UI（飘字、血条、任务标记、头顶状态图标等），如果不在引擎层提供通用支持，每个都要复制相同的 `worldToScreen` 模式。

## 目标
- 在引擎层 `UIElement` 中内置世界锚点定位模式，任何 UIElement 只需设置 `PositioningMode::WorldAnchor` + 世界坐标即可自动完成每帧投影。
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
- 未来任何 UIElement 设一个 mode + 世界坐标就能锚定，无需额外注册。
- 投影逻辑在引擎层统一维护，游戏层零重复。
- `UIElement::update()` 已接收 `Context&`，`Context` 已持有 `Camera&`，引擎层不引入新的外部依赖。

## 当前基线（关键代码）

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/engine/ui/ui_element.h:36-144` | UIElement 基类 | `position_`（屏幕局部坐标）、`ensureLayout()`、`update(Context&)` |
| `src/engine/ui/ui_element.cpp:302-376` | `ensureLayout()` | 锚点/pivot/margin 屏幕空间布局计算 |
| `src/engine/ui/ui_manager.cpp:67-80` | `UIManager::update()` | `processMouseHover()` + 递归 `root->update()` |
| `src/game/ui/dialogue_bubble.h:19-55` | DialogueBubble | 同时持有 dispatcher、world_position、channel |
| `src/game/ui/dialogue_bubble.cpp:104-108` | `update()` | 每帧 `worldToScreen` + `setPosition` |
| `src/game/ui/dialogue_bubble.cpp:110-148` | `onShowEvent()` | 事件过滤 + 文本换行 + setText + setVisible |
| `src/game/scene/game_scene.cpp:342-369` | `initUI()` | 创建 3 个 DialogueBubble 加入 UIManager |
| `src/game/system/system_helpers.h:60-91` | `emitDialogueBubble*` | 业务系统发送事件的 helper（不改） |

## 详细设计

### 1. 引擎层：UIElement 新增世界锚点定位模式

**改动文件**：`src/engine/ui/ui_element.h`

```cpp
// 新增枚举
enum class PositioningMode : std::uint8_t {
    Screen,      // 默认：position_ 是相对父元素的屏幕局部坐标
    WorldAnchor  // position_ 被忽略，由 world_anchor_ 投影后覆盖 layout_position_
};
```

**UIElement 新增成员**：

```cpp
protected:
    PositioningMode positioning_mode_{PositioningMode::Screen};  // 定位模式
    glm::vec2 world_anchor_{0.0f, 0.0f};                         // 世界坐标锚点
    glm::vec2 world_anchor_offset_{0.0f, 0.0f};                  // 投影后的屏幕偏移
```

**UIElement 新增公开接口**：

```cpp
public:
    void setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset = {0.0f, 0.0f});
    void clearWorldAnchor();
    PositioningMode getPositioningMode() const { return positioning_mode_; }
    const glm::vec2& getWorldAnchor() const { return world_anchor_; }
    const glm::vec2& getWorldAnchorOffset() const { return world_anchor_offset_; }
```

**接口语义**：
- `setWorldAnchor(pos, offset)`：设置 `positioning_mode_ = WorldAnchor`，存储世界坐标和屏幕偏移，`invalidateLayout()`。
- `clearWorldAnchor()`：设置 `positioning_mode_ = Screen`，`invalidateLayout()`。
- 调用 `setWorldAnchor()` 后，`position_` 不再参与布局计算；调用 `clearWorldAnchor()` 后恢复正常屏幕定位。

### 2. 引擎层：UIElement::ensureLayout() 适配

**改动文件**：`src/engine/ui/ui_element.cpp`

在 `ensureLayout()` 中，当 `positioning_mode_ == WorldAnchor` 且 `parent_` 存在时，跳过锚点/margin 计算，直接使用缓存的投影结果：

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

    // 世界锚点模式：size 仍走正常计算，但 position 由投影结果决定
    if (positioning_mode_ == PositioningMode::WorldAnchor) {
        layout_size_ = layout_override_size_.value_or(size_);
        // layout_position_ 已由 resolveWorldAnchors() 写入，此处保持不变
        layout_dirty_ = false;
        const_cast<UIElement*>(this)->onLayout();
        return;
    }

    // 原有屏幕定位逻辑完全不变 ...
}
```

### 3. 引擎层：UIManager 统一投影

**改动文件**：`src/engine/ui/ui_manager.h` / `ui_manager.cpp`

```cpp
// ui_manager.h 新增私有方法
private:
    void resolveWorldAnchors(const engine::render::Camera& camera);
```

```cpp
// ui_manager.cpp
void UIManager::update(float delta_time, engine::core::Context& context) {
    UIElement::resetLayoutRecomputeCounter();

    // 新增：统一处理所有世界锚点元素的坐标投影
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
    resolveWorldAnchorsRecursive(root_element_.get(), camera);
}
```

**递归遍历实现**（UIManager 私有方法或自由函数）：

```cpp
void UIManager::resolveWorldAnchorsRecursive(UIElement* element,
                                              const engine::render::Camera& camera) {
    if (!element || !element->isVisible()) return;

    if (element->getPositioningMode() == PositioningMode::WorldAnchor) {
        const glm::vec2 screen_pos =
            camera.worldToScreen(element->getWorldAnchor()) + element->getWorldAnchorOffset();
        // 直接写入 layout_position_，绕过 position_（需要友元或 protected 方法）
        element->applyWorldAnchorPosition(screen_pos);
    }

    for (auto& child : element->getChildren()) {
        resolveWorldAnchorsRecursive(child.get(), camera);
    }
}
```

**注意**：`applyWorldAnchorPosition()` 需要能直接写入 `layout_position_` 而不触发 `invalidateLayout()` 的无限循环。方案有两种：

- **方案 D-1**：新增 `UIElement::applyWorldAnchorPosition(glm::vec2 screen_pos)` protected 方法，`UIManager` 通过友元调用。
- **方案 D-2**（推荐）：在 `ensureLayout()` 内完成投影（不需要 UIManager 遍历），但这需要 `ensureLayout()` 访问 Camera，破坏 const 纯计算语义。

**推荐 D-1**：

```cpp
// ui_element.h
protected:
    friend class UIManager;
    void applyWorldAnchorPosition(glm::vec2 screen_pos);
```

```cpp
// ui_element.cpp
void UIElement::applyWorldAnchorPosition(glm::vec2 screen_pos) {
    // 考虑 pivot：screen_pos 是锚点的屏幕位置，减去 pivot * size 得到左上角
    const glm::vec2 final_size = layout_override_size_.value_or(size_);
    layout_position_ = screen_pos - final_size * pivot_;
    layout_size_ = final_size;
    layout_dirty_ = false;
}
```

### 4. 模块职责划分（重构后）

```
┌──────────────────────────────────────────────────────────────────┐
│ 引擎层 (engine::ui)                                              │
│                                                                  │
│  UIElement                                                       │
│    ├─ positioning_mode_: Screen | WorldAnchor                    │
│    ├─ world_anchor_: glm::vec2        (世界坐标)                  │
│    ├─ world_anchor_offset_: glm::vec2 (屏幕偏移)                  │
│    └─ applyWorldAnchorPosition()      (被 UIManager 调用)         │
│                                                                  │
│  UIManager                                                       │
│    └─ update() 中调用 resolveWorldAnchors()                       │
│       遍历 UI 树，对 WorldAnchor 模式的元素执行 worldToScreen      │
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

### 5. 游戏层：DialogueBubble → DialogueBubbleView

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

### 6. 游戏层：DialogueBubbleController

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

**核心逻辑**（比方案 C 的 controller 更简洁，因为投影交给引擎了）：
- `onShow`：按 channel 查找 slot，命中后调用 `view->setWorldAnchor(world_pos, offset)` + `view->setText(formatted)` + `view->setVisible(true)`。未注册 channel 安全忽略。
- `onMove`：按 channel 查找 slot，调用 `view->setWorldAnchor(new_pos, offset)` 更新世界坐标。
- `onHide`：按 channel 查找 slot，调用 `view->clearWorldAnchor()` + `view->setVisible(false)`。
- **不需要 `update()` 方法** — 投影由 UIManager 统一处理。

### 7. GameScene 接入

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

**`game_scene.cpp::update()` 变更**：
```cpp
void GameScene::update(float delta_time) {
    // 不再需要手动调用 controller->update()
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

### 8. 数据流（重构后）

```
DialogueSystem              DialogueBubbleController         DialogueBubbleView        UIManager
    │                              │                              │                       │
    ├─ enqueue(ShowEvent) ────────→ onShow()                      │                       │
    │                              ├─ formatDialogueText()        │                       │
    │                              ├─ view->setWorldAnchor() ────→ positioning_mode_=WA   │
    │                              ├─ view->setText(formatted) ──→ setText()              │
    │                              └─ view->setVisible(true) ───→ setVisible()            │
    │                              │                              │                       │
    ├─ enqueue(MoveEvent) ────────→ onMove()                      │                       │
    │                              └─ view->setWorldAnchor() ────→ 更新 world_anchor_     │
    │                              │                              │                       │
    │                              │                              │    每帧 update()       │
    │                              │                              │←── resolveWorldAnchors │
    │                              │                              │    worldToScreen()     │
    │                              │                              │    applyPosition()     │
    │                              │                              │                       │
    ├─ enqueue(HideEvent) ────────→ onHide()                      │                       │
    │                              ├─ view->clearWorldAnchor() ──→ positioning_mode_=Scr  │
    │                              └─ view->setVisible(false) ──→ setVisible()            │
```

### 9. 后续世界锚点 UI 使用示例

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

## 需要删除的文件
- `src/game/ui/dialogue_bubble.h`
- `src/game/ui/dialogue_bubble.cpp`

## 预计改动文件
- `src/engine/ui/ui_element.h` — 新增 PositioningMode、world_anchor 成员与接口
- `src/engine/ui/ui_element.cpp` — `ensureLayout()` 适配 WorldAnchor 模式、新增 `applyWorldAnchorPosition()`
- `src/engine/ui/ui_manager.h` — 新增 `resolveWorldAnchors()` 私有方法
- `src/engine/ui/ui_manager.cpp` — `update()` 调用 `resolveWorldAnchors()`、实现递归遍历
- `src/game/scene/game_scene.h` — 前置声明与成员类型变更
- `src/game/scene/game_scene.cpp` — initUI / update / clean 接入
- `src/CMakeLists.txt` — 更新源文件列表（删旧增新）

## 实现步骤

### Step 1：引擎层 UIElement 新增世界锚点支持
- `ui_element.h`：新增 `PositioningMode` 枚举、`world_anchor_` / `world_anchor_offset_` 成员、`setWorldAnchor()` / `clearWorldAnchor()` / `applyWorldAnchorPosition()` 接口。
- `ui_element.cpp`：`ensureLayout()` 中 WorldAnchor 分支直接使用已缓存的 `layout_position_`；实现 `applyWorldAnchorPosition()` 考虑 pivot。

### Step 2：引擎层 UIManager 统一投影
- `ui_manager.h`：新增 `resolveWorldAnchors()` 私有方法声明。
- `ui_manager.cpp`：在 `update()` 开头（`processMouseHover()` 之前）调用 `resolveWorldAnchors()`；实现递归遍历，对 WorldAnchor 元素调用 `applyWorldAnchorPosition()`。

### Step 3：重构 DialogueBubble 为 DialogueBubbleView
- 重命名文件和类。
- 移除 dispatcher/channel/world_position 相关成员和方法。
- 简化构造函数和 update()。

### Step 4：新增 DialogueBubbleController
- 创建 `dialogue_bubble_controller.h/.cpp`。
- 实现事件订阅/断开（RAII）。
- 实现 `registerBubble()` 和事件处理（调用 view 的 `setWorldAnchor` / `setText` / `setVisible`）。
- 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法。

### Step 5：GameScene 接入
- `game_scene.h`：更新前置声明与成员。
- `game_scene.cpp::initUI()`：创建 controller + views 并注册。
- `game_scene.cpp::update()`：移除手动投影调用（引擎自动处理）。
- `game_scene.cpp::clean()`：清理事件队列，先销毁 controller。

### Step 6：更新构建配置
- `src/CMakeLists.txt`：删除旧文件，添加新文件。

### Step 7：编译验证与手动回归
- 全量编译通过。
- 手动验证三类 channel 气泡行为。
- 验证场景切换/暂停菜单无崩溃。

## 待办清单（用于追踪）
- [ ] T1 `ui_element.h` 新增 PositioningMode 枚举与 world_anchor 成员/接口
- [ ] T1.1 `ui_element.cpp` 实现 `setWorldAnchor()` / `clearWorldAnchor()` / `applyWorldAnchorPosition()`
- [ ] T1.2 `ui_element.cpp` 适配 `ensureLayout()` WorldAnchor 分支
- [ ] T2 `ui_manager.h/.cpp` 新增 `resolveWorldAnchors()` 并在 `update()` 中调用
- [ ] T3 重命名 `DialogueBubble` → `DialogueBubbleView` 并瘦身
- [ ] T3.1 移除 dispatcher/channel/world_position 相关成员和方法
- [ ] T3.2 简化构造函数
- [ ] T4 新增 `DialogueBubbleController` 实现事件路由
- [ ] T4.1 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法
- [ ] T5 更新 `game_scene.h/.cpp` 接入
- [ ] T6 更新 `src/CMakeLists.txt` 源文件列表
- [ ] T7 全量编译通过
- [ ] T8 手动回归验证三类 channel 气泡行为

## 测试计划
- 编译期验证：
  - 全量编译通过（`cmake --build build -j`），无 warning 无 error。
- 手动回归（必须）：
  - NPC 对话（channel 0）：靠近 NPC 交互，气泡出现在头顶，跟随 NPC 移动，离开后消失。
  - 开箱通知（channel 1）：打开宝箱，通知气泡正确显示。
  - 物品使用提示（channel 2）：使用物品，提示气泡出现在正确偏移位置（-56px）。
  - 未注册 channel（例如脚本发送 channel=255）：无崩溃、事件被安全忽略。
  - 场景切换：切换地图后无崩溃，旧气泡不残留。
  - 暂停菜单：暂停/恢复后气泡行为正常。
  - 屏幕固定 UI 不受影响：InventoryUI、HotbarUI、TimeClock 等位置和交互正常。
- 可选单元测试：
  - `UIElement::setWorldAnchor()` / `clearWorldAnchor()` 模式切换正确。
  - `resolveWorldAnchors()` 对 Screen 模式元素不产生副作用。
  - `DialogueBubbleController` 事件路由测试：不同 channel 事件仅影响对应 View。
  - `formatDialogueText()` 文本格式化测试。

## 验收标准（DoD）
- `UIElement` 支持 `PositioningMode::WorldAnchor`，任何元素均可通过 `setWorldAnchor()` 锚定世界坐标。
- `UIManager::update()` 统一处理所有世界锚点元素的投影，屏幕固定元素不受影响。
- `DialogueBubbleView` 不直接订阅业务事件，不持有 `channel` / `world_position` / `dispatcher`。
- `DialogueBubbleController` 仅做事件路由和文本格式化，不含投影逻辑。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem）零改动。
- 现有三类 channel（0/1/2）气泡行为与重构前完全一致。
- 全量编译通过，手动回归通过。

## 风险与缓解
- **风险**：`resolveWorldAnchors()` 递归遍历整棵 UI 树，如果树很大会有性能开销。
  - **缓解**：当前 UI 树节点数约 30-50 个，遍历开销可忽略。若未来 UI 树膨胀，可改为维护一个 `world_anchor_elements_` 列表（注册时加入，clearWorldAnchor 时移除），避免全树遍历。
- **风险**：`applyWorldAnchorPosition()` 需要友元访问 `layout_position_`。
  - **缓解**：UIManager 已经是引擎内部类，与 UIElement 在同一模块中，友元关系合理。
- **风险**：场景销毁时 controller 的事件订阅未及时解绑，导致野指针。
  - **缓解**：controller 通过 RAII 管理订阅，`GameScene::clean()` 显式先销毁 controller。
- **风险**：场景切换时旧场景遗留队列事件污染新场景 UI。
  - **缓解**：`GameScene::clean()` 统一 `clear<DialogueShow/Move/HideEvent>()`。
- **风险**：未注册 channel 事件导致异常。
  - **缓解**：controller 使用 `unordered_map` + 查找守卫，未命中安全忽略。

## 后续扩展方向（不在本次范围内）
- `FloatingText`（伤害飘字）：直接用 UILabel + `setWorldAnchor()` + 每帧 offset 动画。
- `QuestMarker`（任务标记）：UIImage + `setWorldAnchor()`。
- `HealthBar`（头顶血条）：UIPanel + UIProgressBar + `setWorldAnchor()`。
- `BuffIcon`（状态图标）：UIImage 组 + `setWorldAnchor()`。
- 以上全部只需 `setWorldAnchor()` 一行调用，无需新建 controller 或注册 slot。
- 性能优化：当世界锚点元素超过 100 个时，引入 dirty list 替代全树遍历。
- 文本自动换行升级为基于像素宽度测量。
- 气泡显隐动画（淡入淡出）。
