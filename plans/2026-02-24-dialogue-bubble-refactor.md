# DialogueBubble 世界锚点 UI 解耦重构

## 背景与问题

`DialogueBubble` 当前由 `UIManager` 管理，作为 UIElement 子树挂在 Scene 级 UI 树中。但它与其他屏幕固定 UI（InventoryUI、HotbarUI、TimeClock 等）有本质区别：

1. **世界坐标依赖** — 每帧在 `update()` 中执行 `camera.worldToScreen(world_position_)` 将世界坐标投影到屏幕坐标（`dialogue_bubble.cpp:106`），是 UIManager 中唯一依赖世界坐标的元素。
2. **事件订阅耦合** — 自身直接订阅 `DialogueShow/Move/HideEvent`（`dialogue_bubble.cpp:71-74`），承担了事件路由与 channel 分发的控制逻辑。
3. **文本排版逻辑** — `onShowEvent()` 内含 28 字符行宽的自动换行逻辑（`dialogue_bubble.cpp:117-146`），这是业务逻辑而非视图职责。
4. **职责过重** — 同一个类承担了事件订阅、世界坐标跟随、文本排版、UI 渲染四项职责，难以复用。

这导致 `UIManager` 的边界模糊：它本应专注于屏幕空间 UI 树管理（布局、输入、渲染调度），却隐式承载了一个世界锚点 UI 的全部生命周期。未来若新增类似的世界锚点 UI（任务标记、伤害飘字、头顶状态图标等），将不得不复制相同的模式。

## 目标
- `UIManager` 只负责屏幕空间 UI 树的布局、输入命中与渲染调度，不感知世界坐标。
- `DialogueBubble` 退化为纯视图组件（View），只提供 `setText()` / `setScreenPosition()` / `setVisible()` 等展示接口，不直接持有 `dispatcher`、`channel`、`world_position_`。
- 新增 `WorldAnchorUIController` 统一承担世界锚点 UI 的事件路由、世界坐标缓存与屏幕投影职责。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem 等）无需任何改动。
- 为后续世界锚点 UI 复用预留基础能力（同一机制可承载 `QuestMarker`、`FloatingText`、`BuffIcon` 等）。
- `channel` 路由机制支持后续扩展（不再固化为 0/1/2），未注册 channel 事件可安全忽略。

## 非目标
- 不修改 `DialogueShowEvent` / `DialogueMoveEvent` / `DialogueHideEvent` 事件结构。
- 不修改任何业务系统（DialogueSystem、ChestSystem、ItemUseSystem）的发送逻辑。
- 不在本次重构中解决文本自动换行的像素级测量问题（可后续独立推进）。
- 不引入新的渲染通道或改变渲染管线顺序。

## 方案对比

### 方案 A：保持现状，仅修正文档
- 做法：保留 `DialogueBubble` 结构不变，在文档中明确 UIManager 也管理世界投影 UI。
- 优点：零改动，零风险。
- 缺点：`DialogueBubble` 仍是 God Class 趋势，新增世界锚点 UI 时会复制粘贴同类模式。

### 方案 B：双管理器（ScreenUIManager + WorldUIManager）
- 做法：Scene 同时持有两个 UIManager，分别管理屏幕固定 UI 和世界锚点 UI。
- 优点：边界彻底隔离。
- 缺点：增加输入路由、排序与生命周期管理的复杂度；需要重构 Scene 框架，投入偏大。

### 方案 C（推荐）：单 UIManager + WorldAnchorUIController
- 做法：保留单 UIManager 作为 Scene 唯一 UI 树管理器；新增 `WorldAnchorUIController` 统一处理世界锚点 UI 的事件订阅、坐标投影与视图驱动。`DialogueBubble` 瘦身为纯 View。
- 优点：
  - 不破坏现有 Scene/UIManager 框架，改动可控。
  - 世界锚点协调逻辑从具体控件中抽离，复用性高。
  - DialogueBubble 的渲染路径完全不变（仍走 UIPanel 九宫格 + UILabel 文本），无需复制渲染逻辑。
- 缺点：比方案 A 多一层抽象，需补充测试。

## 当前基线（关键代码）

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/game/ui/dialogue_bubble.h:19-55` | 类定义 | 同时持有 dispatcher、world_position、channel、text_renderer |
| `src/game/ui/dialogue_bubble.cpp:71-74` | `subscribeEvents()` | 在构造函数中直接订阅 3 个事件 |
| `src/game/ui/dialogue_bubble.cpp:104-108` | `update()` | 每帧 `worldToScreen` + `setPosition` |
| `src/game/ui/dialogue_bubble.cpp:110-148` | `onShowEvent()` | 事件过滤 + 文本换行 + setText + setVisible |
| `src/game/scene/game_scene.cpp:342-369` | `initUI()` | 创建 3 个 DialogueBubble（channel 0/1/2）加入 UIManager |
| `src/game/scene/game_scene.h:54` | `dialogue_bubble_` | GameScene 持有 channel 0 的裸指针 |
| `src/game/system/system_helpers.h:60-91` | `emitDialogueBubble*` | 业务系统发送事件的 helper（不改） |

## 推荐方案详细设计（方案 C）

### 1. 模块职责划分

```
┌────────────────────────────────────────────────────────────┐
│ GameScene                                                  │
│                                                            │
│  ┌──────────────────────┐  ┌────────────────────────────┐  │
│  │ WorldAnchorUI        │  │ UIManager                  │  │
│  │ Controller            │  │  ┌─ TimeClockUI           │  │
│  │                      │  │  ├─ InventoryUI            │  │
│  │  订阅 Dialogue*Event │  │  ├─ HotbarUI               │  │
│  │  缓存 world_position │  │  ├─ ItemTooltipUI          │  │
│  │  每帧 worldToScreen  │──│──├─ DialogueBubbleView ×3  │  │
│  │  驱动 View 状态      │  │  ├─ MenuButton             │  │
│  └──────────────────────┘  │  └─ UIScreenFade           │  │
│                            └────────────────────────────┘  │
│                                                            │
│  ┌──────────────────────┐                                  │
│  │ SystemScheduler      │   业务系统照常发                  │
│  │  DialogueSystem ─────│── DialogueShow/Move/HideEvent   │
│  │  ChestSystem ────────│── DialogueShowEvent (channel 1) │
│  │  ItemUseSystem ──────│── DialogueShowEvent (channel 2) │
│  └──────────────────────┘                                  │
└────────────────────────────────────────────────────────────┘
```

### 2. 新增：WorldAnchorUIController

**位置**：`src/game/ui/world_anchor_ui_controller.h` / `.cpp`
**命名空间**：`game::ui`

```cpp
// world_anchor_ui_controller.h（概念接口）
class WorldAnchorUIController {
public:
    WorldAnchorUIController(entt::dispatcher& dispatcher, engine::core::Context& context);
    ~WorldAnchorUIController(); // 析构时 disconnect 所有事件

    /// 注册 dialogue bubble view（按 channel 索引）
    void registerDialogueBubble(std::uint8_t channel, DialogueBubbleView* view,
                                glm::vec2 screen_offset = {0.0f, -4.0f});

    /// 每帧调用：将所有活跃锚点的世界坐标投影到屏幕坐标，写入对应 View
    void update();

private:
    struct AnchorSlot {
        DialogueBubbleView* view{nullptr};
        glm::vec2 world_position{0.0f};
        glm::vec2 screen_offset{0.0f, -4.0f};
        bool active{false};
    };

    entt::dispatcher& dispatcher_;
    engine::core::Context& context_;
    std::unordered_map<std::uint8_t, AnchorSlot> dialogue_slots_;

    void onDialogueShow(const game::defs::DialogueShowEvent& evt);
    void onDialogueMove(const game::defs::DialogueMoveEvent& evt);
    void onDialogueHide(const game::defs::DialogueHideEvent& evt);

    /// 文本排版（从 DialogueBubble::onShowEvent 迁移而来）
    static std::string formatDialogueText(std::string_view speaker, std::string_view text);
    AnchorSlot* findSlot(std::uint8_t channel);
};
```

**核心逻辑**：
- 构造时订阅 `DialogueShow/Move/HideEvent`，析构时断开（RAII）。
- `onDialogueShow`：按 `channel` 查找已注册 slot；若未注册则安全忽略。命中后缓存 `world_position`，调用 `formatDialogueText()` 格式化文本，再调用 `view->setText()`、`view->setVisible(true)`。
- `onDialogueMove`：按 `channel` 查找 slot，命中后更新 `world_position`。
- `onDialogueHide`：按 `channel` 查找 slot，命中后标记 inactive 并调用 `view->setVisible(false)`。
- `update()`：遍历所有活跃 slot，执行 `camera.worldToScreen(world_position) + screen_offset`，写入 `view->setPosition()`。

### 3. 重构：DialogueBubble → DialogueBubbleView

**位置**：`src/game/ui/dialogue_bubble_view.h` / `.cpp`（重命名）
**命名空间**：`game::ui`

**移除的成员与方法**：
- `dispatcher_` — 不再直接持有
- `world_position_` — 由 controller 管理
- `offset_` — 由 controller 管理
- `channel_` — 由 controller 管理
- `subscribeEvents()` / `onShowEvent()` / `onMoveEvent()` / `onHideEvent()` — 全部移除
- `setWorldPosition()` / `setOffset()` — 移除

**保留的成员与方法**：
- `text_renderer_`、`panel_`、`label_`、`bubble_image_`、`padding_`、`font_id_`、`font_size_`
- `setText(std::string_view text)` — 更新标签文本并刷新布局
- `buildSkin()` / `buildLayout()` / `refreshLayoutFromText()` — 视图构建逻辑不变

**新增**：
- 移除 `update()` 中的 `worldToScreen` 逻辑，回退为纯 `UIElement::update()` 调用

**重命名理由**：文件名从 `dialogue_bubble` 改为 `dialogue_bubble_view`，明确其"纯视图"定位。旧文件删除（不考虑向后兼容）。

### 4. 重构：GameScene 接入

**`game_scene.h` 变更**：
```diff
- class DialogueBubble;
+ class DialogueBubbleView;
+ class WorldAnchorUIController;

+ std::unique_ptr<game::ui::WorldAnchorUIController> world_anchor_controller_;
```

**`game_scene.cpp::initUI()` 变更**：
```cpp
// 1. 创建 controller
world_anchor_controller_ = std::make_unique<game::ui::WorldAnchorUIController>(
    dispatcher_ref, context_);

// 2. 创建 DialogueBubbleView（不再传 dispatcher 和 channel）
auto bubble_0 = std::make_unique<game::ui::DialogueBubbleView>(
    context_, text_renderer);
auto* bubble_0_ptr = bubble_0.get();
ui_manager_->addElement(std::move(bubble_0));

auto bubble_1 = std::make_unique<game::ui::DialogueBubbleView>(
    context_, text_renderer);
auto* bubble_1_ptr = bubble_1.get();
ui_manager_->addElement(std::move(bubble_1));

auto bubble_2 = std::make_unique<game::ui::DialogueBubbleView>(
    context_, text_renderer);
auto* bubble_2_ptr = bubble_2.get();
ui_manager_->addElement(std::move(bubble_2));

// 3. 注册到 controller
world_anchor_controller_->registerDialogueBubble(0, bubble_0_ptr);
world_anchor_controller_->registerDialogueBubble(1, bubble_1_ptr);
world_anchor_controller_->registerDialogueBubble(2, bubble_2_ptr, {0.0f, -56.0f});
```

**`game_scene.cpp::update()` 变更**：
```cpp
void GameScene::update(float delta_time) {
    // controller 在 UIManager update 之前执行，确保同帧位置生效
    if (world_anchor_controller_) {
        world_anchor_controller_->update();
    }
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
    world_anchor_controller_.reset();
    // ... 其余清理逻辑（保持原有顺序）
}
```

### 5. 数据流（重构后）

```
DialogueSystem                   WorldAnchorUIController         DialogueBubbleView
    │                                   │                              │
    ├─ enqueue(DialogueShowEvent) ──────→ onDialogueShow()             │
    │                                   ├─ formatDialogueText()        │
    │                                   ├─ cache world_position        │
    │                                   ├─ view->setText(formatted) ──→ setText()
    │                                   └─ view->setVisible(true) ───→ setVisible()
    │                                   │                              │
    ├─ enqueue(DialogueMoveEvent) ──────→ onDialogueMove()             │
    │                                   └─ update world_position       │
    │                                   │                              │
    │                     每帧 update() → worldToScreen(cached_pos)    │
    │                                   └─ view->setPosition(screen) ─→ setPosition()
    │                                   │                              │
    ├─ enqueue(DialogueHideEvent) ──────→ onDialogueHide()             │
    │                                   └─ view->setVisible(false) ──→ setVisible()
```

### 6. 排序与层级

保持现有行为不变：
- DialogueBubbleView 的 `order_index` 保持默认（0），位于 HotbarUI/InventoryUI 同层。
- `UIScreenFade`（`order_index=10000`）仍然在最顶层。
- 三个 channel 的气泡互不覆盖，由 controller 独立管理各自的显隐状态。

## 需要新增的文件
- `src/game/ui/world_anchor_ui_controller.h`
- `src/game/ui/world_anchor_ui_controller.cpp`
- `src/game/ui/dialogue_bubble_view.h`（重命名自 `dialogue_bubble.h`）
- `src/game/ui/dialogue_bubble_view.cpp`（重命名自 `dialogue_bubble.cpp`）

## 需要删除的文件
- `src/game/ui/dialogue_bubble.h`
- `src/game/ui/dialogue_bubble.cpp`

## 预计改动文件
- `src/game/scene/game_scene.h` — 前置声明与成员类型变更
- `src/game/scene/game_scene.cpp` — initUI / update / clean 接入 controller
- `src/CMakeLists.txt` — 更新源文件列表（删旧增新）
- `docs/overview.md` — 目录结构说明更新（如有必要）
- `docs/game/interaction_and_dialogue.md` — channel 路由与组件命名说明同步（如有必要）

## 实现步骤

### Step 1：新增 WorldAnchorUIController 骨架
- 创建 `world_anchor_ui_controller.h/.cpp`。
- 实现构造/析构（事件订阅与断开）。
- 实现 `registerDialogueBubble()`、`update()`。
- 实现 `onDialogueShow/Move/Hide` 事件处理。
- `channel` 路由使用可扩展容器（`unordered_map`）；未注册 channel 事件安全忽略。
- 将 `DialogueBubble::onShowEvent` 中的文本格式化逻辑迁移为 `formatDialogueText()` 静态方法。

### Step 2：重构 DialogueBubble 为 DialogueBubbleView
- 重命名文件：`dialogue_bubble.h/.cpp` → `dialogue_bubble_view.h/.cpp`。
- 重命名类：`DialogueBubble` → `DialogueBubbleView`。
- 移除成员：`dispatcher_`、`world_position_`、`offset_`、`channel_`。
- 移除方法：`subscribeEvents()`、`onShowEvent()`、`onMoveEvent()`、`onHideEvent()`、`setWorldPosition()`、`setOffset()`。
- 简化构造函数：不再接受 `dispatcher` 和 `channel` 参数。
- 简化 `update()`：移除 `worldToScreen` 逻辑，仅保留 `UIElement::update()` 调用（或直接删除 override）。

### Step 3：在 GameScene 中接入 Controller
- `game_scene.h`：更新前置声明与成员（移除 `dialogue_bubble_` 成员）。
- `game_scene.cpp::initUI()`：创建 controller，创建 3 个 DialogueBubbleView 并注册到 controller。
- `game_scene.cpp::update()`：在 `Scene::update()` 前调用 `controller->update()`。
- `game_scene.cpp::clean()`：统一清理 `DialogueShow/Move/HideEvent` 队列，并在其他清理前 `world_anchor_controller_.reset()`。

### Step 4：更新构建配置
- `src/CMakeLists.txt`：删除旧文件，添加新文件。

### Step 5：编译验证与手动回归
- 全量编译通过。
- 手动验证：NPC 对话、开箱通知、物品使用提示三类路径气泡显示正常。
- 验证场景切换/暂停菜单无崩溃。

## 待办清单（用于追踪）
- [ ] T1 新增 `world_anchor_ui_controller.h/.cpp` 并实现事件订阅、投影与视图驱动
- [ ] T1.1 迁移文本格式化逻辑为 `formatDialogueText()` 静态方法
- [ ] T1.2 `channel` 路由改为可扩展容器，未注册 channel 事件安全忽略
- [ ] T2 重命名 `DialogueBubble` → `DialogueBubbleView` 并瘦身
- [ ] T2.1 移除 dispatcher/channel/world_position 相关成员和方法
- [ ] T2.2 简化构造函数（去掉 dispatcher、channel 参数）
- [ ] T2.3 简化或移除 `update()` override
- [ ] T3 更新 `game_scene.h` 前置声明与成员类型（移除 `dialogue_bubble_`）
- [ ] T3.1 更新 `game_scene.cpp::initUI()` — 创建 controller + view 并注册
- [ ] T3.2 更新 `game_scene.cpp::update()` — 调用 controller->update()
- [ ] T3.3 更新 `game_scene.cpp::clean()` — 统一 clear `Show/Move/Hide` 队列后先销毁 controller
- [ ] T4 更新 `src/CMakeLists.txt` 源文件列表
- [ ] T5 全量编译通过
- [ ] T6 手动回归验证三类 channel 气泡行为

## 测试计划
- 编译期验证：
  - 全量编译通过（`cmake --build build -j`），无 warning 无 error。
- 手动回归（必须）：
  - NPC 对话（channel 0）：靠近 NPC 交互，气泡出现在头顶，跟随 NPC 移动，离开后消失。
  - 开箱通知（channel 1）：打开宝箱，通知气泡正确显示。
  - 物品使用提示（channel 2）：使用物品，提示气泡出现在正确偏移位置（-56px）。
  - 非注册 channel（例如脚本发送 channel=255）：无崩溃、无越界访问、事件被安全忽略。
  - 场景切换：切换地图后无崩溃，旧气泡不残留。
  - 场景切换事件清理：切换前残留在队列中的 `DialogueShow/Move/HideEvent` 不影响新场景 UI。
  - 暂停菜单：暂停/恢复后气泡行为正常。
- 可选单元测试：
  - `WorldAnchorUIController` 事件路由测试：不同 channel 事件仅影响对应 View。
  - `WorldAnchorUIController` 越界/未注册 channel 测试：输入任意 `std::uint8_t` 均不崩溃。
  - `formatDialogueText()` 文本格式化测试：验证换行、speaker 前缀行为。

## 验收标准（DoD）
- `UIManager` 不承载任何世界坐标投影逻辑。
- `DialogueBubbleView` 不直接订阅业务事件，不持有 `channel` / `world_position` / `dispatcher` 状态。
- 业务系统（DialogueSystem、ChestSystem、ItemUseSystem）零改动。
- 现有三类 channel（0/1/2）气泡行为与重构前完全一致；新增/未注册 channel 输入不会导致崩溃或越界。
- 场景清理阶段统一清理 `DialogueShow/Move/HideEvent` 队列，避免旧场景队列事件污染新场景。
- 全量编译通过，手动回归通过。

## 风险与缓解
- **风险**：controller `update()` 与 UIManager `update()` 的调用顺序不当，导致气泡位置晚一帧。
  - **缓解**：固定 controller 在 `Scene::update()` 前执行（`game_scene.cpp::update()` 中已明确）。
- **风险**：场景销毁时 controller 的事件订阅未及时解绑，导致野指针。
  - **缓解**：controller 通过 RAII 管理订阅（析构函数 disconnect），且 `GameScene::clean()` 显式先销毁 controller。
- **风险**：外部（脚本或未来系统）发出未注册 channel 事件时出现越界访问。
  - **缓解**：controller 使用 `unordered_map` + 查找守卫，未命中直接返回并记录 trace（可选）。
- **风险**：场景切换时旧场景遗留队列事件污染新场景 UI。
  - **缓解**：`GameScene::clean()` 统一 `clear<DialogueShow/Move/HideEvent>()`，并在 `Scene::clean()` 前 reset controller。
- **风险**：后续新增世界锚点 UI（飘字、标记等）需要 controller 接口扩展。
  - **缓解**：controller 内部按 `AnchorSlot` 模型组织，新增类型只需添加新的 slot 集合和对应事件处理，无需改动已有 dialogue 逻辑。

## 后续扩展方向（不在本次范围内）
- 将 `WorldAnchorUIController` 泛化为支持多种锚点类型的框架（`FloatingText`、`QuestMarker` 等），每种类型注册独立的 View 工厂和事件处理器。
- 文本自动换行升级为基于像素宽度测量（`TextRenderer::getTextSize`），解决 CJK 混排溢出问题。
- 气泡显隐动画（淡入淡出）支持。
