# RmlUi 全面迁移方案：替代现有游戏 UI 体系

## Context

RmlUi 6.2 基础集成已完成（渲染后端、SDL 输入、GL 状态恢复、事件路由），目前仅有 demo 文档。现有游戏 UI 基于自建的 `engine::ui` 框架（UIElement 层级树 + UIManager + 行为/布局系统 + UIPresetManager），已支撑物品栏、快捷栏、对话气泡、时钟 HUD 等功能。

本方案目标：**逐步将所有游戏 UI 迁移到 RmlUi，最终完全移除 `engine::ui` 框架（ImGui 调试面板不受影响）。**

采用最优方案，不考虑向后兼容。

---

## 迁移范围总览

### 需要迁移的 UI 组件

| 组件 | 复杂度 | 关键难点 |
|------|--------|----------|
| TimeClockUI | 低 | 只读 HUD，sprite 动画（时钟指针） |
| TitleScene UI | 低 | 纯按钮 + 背景 + Logo 动画 |
| PauseMenuScene UI | 低 | 按钮 + 音量/时间刻度调节 |
| RestDialogScene UI | 低 | 按钮 + 数值加减 |
| SaveSlotSelectScene UI | 中 | 按钮列表 + 确认对话框（模态） |
| DialogueBubbleView | 中 | 世界坐标锚定、动态文本、多通道 |
| ItemTooltipUI | 中 | 鼠标跟随定位、动态内容 |
| BattleScene UI | 中 | 状态机驱动的按钮启禁、战斗信息展示 |
| HotbarUI | 高 | 拖拽、活跃槽高亮、与 InventoryUI 联动 |
| InventoryUI | 高 | 多页分页、拖拽排序、跨 UI 拖拽 |
| UIScreenFade | 低 | 全屏淡入淡出 overlay |

### 不迁移

- **ImGui 调试面板**（`TF_ENABLE_DEBUG_UI`）：保持现状
- **TextRenderer**：保留世界文字渲染路径（scene pass）；移除 UI 文字路径（`use_ui_pass` 分支 + `drawScreenText()`），屏幕文字全部由 RmlUi 承担
- **tools/visual_tester**：保留，但移除其中的 UI pass 测试用例（`drawUIFilledRect`/`drawUIImage` 调用）

### 删除

- **tools/ui_tester**：整个删除（依赖旧 UI 框架，不再需要）
- **UIPass**：在迁移完成后删除（Phase 8）

---

## 架构决策

### 1) RmlUi 应用层架构：多文档 + 场景归属

当前 `RmlUILayer` 只支持单文档。迁移需要多文档并存（HUD 常驻 + 弹出面板叠加），且必须与引擎场景栈语义对齐。

**问题**：`Rml::Context::Update()` 是全局的——它会更新所有已加载文档的动画/布局/事件，而引擎 `SceneManager` 仅更新栈顶场景（非栈顶场景被冻结）。如果不做隔离，被覆盖场景的 RML 文档仍会响应输入和执行动画。

**方案**：引入 **文档归属（Document Ownership）** 机制：

- 每个 `Rml::ElementDocument` 关联一个 owner 标识（`std::string_view scene_name`）
- `RmlUILayer` 新增 `setActiveScene(scene_name)` 方法，由 `SceneManager` 在场景切换时调用
- **交互隔离**：非活跃场景的文档自动设为 `Rml::ElementDocument::Show(ModalFlag::None, FocusFlag::None)` 并禁用事件（通过 `element->SetProperty("pointer-events", "none")` 或卸载事件监听器）
- **更新隔离**：`RmlUILayer::update()` 仍调用全局 `Context::Update()`（RmlUi 要求），但非活跃文档的 data model 不做 `DirtyVariable()` 标记，从而不触发实际更新
- **渲染**：全栈渲染时所有可见文档仍参与渲染（与 SceneManager 渲染全栈行为一致），但仅栈顶场景文档可交互
- 场景 `clean()` 时自动卸载其名下所有文档

### 2) 数据绑定：RmlUi Data Model

RmlUi 6.x 提供 `Rml::DataModelConstructor` 数据绑定系统，适合将游戏数据（物品栏、时间、对话文本）绑定到 RML 文档：

- 游戏侧维护 data model struct，通过 `DataModelHandle::DirtyVariable()` 标记变更
- RML 模板通过 `data-for` / `data-if` / `data-attr` 实现数据驱动渲染
- 避免手动 DOM 操作，保持声明式风格

### 3) 事件桥接：RML Event → Game Command

RML 元素通过 `data-event-click` 或 `Rml::EventListener` 触发回调：

- 新建 `RmlEventBridge` 类，将 RML 事件映射到 `entt::dispatcher` 的 game command/event
- 保持现有的事件/命令契约不变（`InventoryMoveCommand`、`HotbarBindCommand` 等）

### 4) 拖拽：RmlUi 原生 drag-and-drop

RmlUi 原生支持 `drag` / `dragdrop` 属性和 `dragstart` / `dragover` / `dragdrop` 事件，直接替代现有 `DragBehavior` + `UIDragPreview`：

- 在 `.rml` 中为 item slot 设置 `drag="drag-drop"` 属性
- 通过 RML 事件回调处理拖拽逻辑
- 拖拽预览由 RmlUi 自动生成（可通过 CSS 自定义 `drag` pseudo-class）

### 5) 世界坐标锚定（对话气泡）

RmlUi 文档在屏幕空间渲染。对话气泡需要跟随世界实体：

- 每帧在 C++ 侧计算世界→屏幕坐标，通过 data binding 更新 RML 元素的 `left` / `top` 属性
- 或直接通过 `element->SetProperty("left", ...)` 设置绝对定位

### 6) Sprite 图集复用

现有 UI 使用 sprite atlas（`button.png`、`HUD.png`、`Slots.png` 等），通过 source rect 切片。RmlUi 的 `<img>` 标签原生不支持 sprite rect，两种方案：

- **方案 A**：自定义 `decorator`，在 RCSS 中指定 sprite rect（RmlUi 支持 sprite sheet decorator）
- **方案 B**：将 sprite atlas 预切割为独立 PNG 文件

采用 **方案 A**（RmlUi sprite sheet），利用 RCSS sprite 定义直接引用图集，无需拆分资源：

```rcss
@spritesheet ui-buttons {
    src: /assets/farm-rpg/UI/button.png;
    btn-normal: 0px 0px 48px 16px;
    btn-hover:  0px 16px 48px 16px;
    btn-pressed: 0px 32px 48px 16px;
}
```

### 7) 字体复用

RmlUi 已在 `RmlUILayer::init()` 中加载项目字体。后续文档直接通过 CSS `font-family` 引用即可。

### 8) ScreenFade 抽象接口

`MapTransitionSystem` 通过轮询 `UIScreenFade::phase()` 驱动异步地图加载（`FadingOut → Holding → fadeIn() → Idle`）。直接删除 `UIScreenFade` 会断裂这条依赖链。

**方案**：抽取纯接口 `IScreenFade`，保留状态机契约：

```cpp
// src/engine/ui/screen_fade_interface.h
class IScreenFade {
public:
    enum class Phase : uint8_t { Idle, FadingOut, Holding, FadingIn };
    virtual ~IScreenFade() = default;
    virtual void fadeOut(float seconds) = 0;
    virtual void fadeIn(float seconds) = 0;
    virtual Phase phase() const = 0;
};
```

- Phase 2 中新建 `RmlScreenFade : public IScreenFade`，内部通过 RML 文档 + data binding 驱动视觉效果，同时维护 Phase 状态机
- `MapTransitionSystem` 改为持有 `IScreenFade*` 而非 `engine::ui::UIScreenFade*`
- 迁移后旧 `UIScreenFade` 可安全删除

### 9) TextRenderer UI 通道迁移

`TextRenderer::renderGlyph()` 在 `use_ui_pass == true` 时调用 `gl_renderer_->drawUITexture()` 绘制 UI 文字（阴影 + 字形）。移除 `drawUITexture()` 会断裂此路径。

**方案**：迁移完成后，所有屏幕空间文字由 RmlUi 负责渲染。`TextRenderer` 仅保留世界文字路径（scene pass）。Phase 8 中移除 `drawUITexture` 前，同步移除 `TextRenderer` 的 `use_ui_pass` 分支及 `drawScreenText()` 方法。

---

## 实施阶段

### Phase 0: RmlUi 应用层基础设施

**目标**：为后续迁移建立可复用的基础设施。

#### Step 0.1: 多文档管理 + 场景归属

**修改** `src/engine/ui/rmlui/rml_ui_layer.h/cpp`

- 新增 `loadDocument(path, owner_scene_name)` 返回 `Rml::ElementDocument*`（不再替换唯一文档）
- 新增 `unloadDocument(Rml::ElementDocument*)`
- 新增 `unloadDocumentsByOwner(scene_name)` — 场景 `clean()` 时批量卸载
- 新增 `showDocument(doc)` / `hideDocument(doc)` 便捷方法
- 新增 `setActiveScene(scene_name)` — SceneManager 在 push/pop/replace 时调用：
  - 活跃场景的文档：恢复事件响应（`pointer-events: auto`）
  - 非活跃场景的文档：禁止事件响应（`pointer-events: none`），但保持可见（支持全栈渲染）
- 内部维护 `struct DocumentEntry { Rml::ElementDocument* doc; std::string owner; }` 列表
- 移除 `current_document_` 单文档限制

**修改** `src/engine/scene/scene_manager.cpp`

- 在 `pushScene()` / `popScene()` / `replaceScene()` 执行后，调用 `rmlui_layer->setActiveScene(top_scene->getName())`
- 确保场景 `clean()` 前先调用 `rmlui_layer->unloadDocumentsByOwner(scene_name)`

#### Step 0.2: Data Model 辅助层

**新建** `src/engine/ui/rmlui/rml_data_bridge.h/cpp`

- 封装 `Rml::DataModelConstructor` 使用模式
- 提供模板工具：`bindScalar<T>(name, getter, setter)` / `bindArray(name, ...)` / `bindStruct(...)`
- 提供 `markDirty(variable_name)` 便捷方法
- 为常见模式（物品列表、标量值显示）提供辅助函数

#### Step 0.3: Event Bridge

**新建** `src/engine/ui/rmlui/rml_event_bridge.h/cpp`

- `RmlEventBridge` 持有 `entt::dispatcher&` 引用
- 注册为 `Rml::EventListener`，解析 RML 事件参数并分发对应的游戏命令
- 支持通过 RML 属性配置事件映射（例如 `data-command="use_item"` `data-slot="3"`）

#### Step 0.4: RCSS 基础主题

**新建** `assets/ui/rmlui/theme/`

- `base.rcss`：全局字体、颜色变量、通用 class（`.panel`、`.button`、`.label`、`.slot`）
- `spritesheet.rcss`：统一的 sprite sheet 定义（引用现有 UI 图集）
- `animation.rcss`：通用动画/过渡定义（fade、slide 等）

#### Step 0.5: Scene 集成接口

在 `engine::core::Context` 中注册 `RmlUILayer*` 引用，供 Scene 直接访问。Scene 基类新增便捷方法：

- `loadRmlDocument(path)` → 自动以 `scene_name_` 为 owner 调用 `rmlui_layer->loadDocument(path, scene_name_)`
- Scene::clean() 中自动调用 `rmlui_layer->unloadDocumentsByOwner(scene_name_)`

#### Step 0.6: ScreenFade 抽象接口

**新建** `src/engine/ui/screen_fade_interface.h`

- 纯接口 `IScreenFade`，声明 `Phase` 枚举 + `fadeOut()` / `fadeIn()` / `phase()` 方法
- `MapTransitionSystem` 改为持有 `IScreenFade*`（此步仅改接口引用，旧 `UIScreenFade` 实现 `IScreenFade` 作为过渡）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 修改 | `src/engine/ui/rmlui/rml_ui_layer.h/cpp` |
| 新建 | `src/engine/ui/rmlui/rml_data_bridge.h/cpp` |
| 新建 | `src/engine/ui/rmlui/rml_event_bridge.h/cpp` |
| 新建 | `src/engine/ui/screen_fade_interface.h` |
| 新建 | `assets/ui/rmlui/theme/base.rcss` |
| 新建 | `assets/ui/rmlui/theme/spritesheet.rcss` |
| 新建 | `assets/ui/rmlui/theme/animation.rcss` |
| 修改 | `src/engine/core/context.h/cpp`（注册 RmlUILayer） |
| 修改 | `src/engine/scene/scene.h/cpp`（RML 便捷方法 + clean 自动卸载） |
| 修改 | `src/engine/scene/scene_manager.cpp`（场景切换时通知 setActiveScene） |
| 修改 | `src/game/system/map_transition_system.h/cpp`（UIScreenFade* → IScreenFade*） |

**验证**：
- 加载多个 RML 文档并行显示 + 隐藏
- data binding 驱动简单文本标签更新
- 场景 push/pop 后，仅栈顶场景的 RML 文档可交互（被覆盖场景文档不响应输入）
- 场景 clean 后其文档自动卸载
- MapTransitionSystem 通过 IScreenFade 接口仍正常工作（旧实现兼容）

---

### Phase 1: 静态 HUD — TimeClockUI

**目标**：用 RmlUi 重写时钟 HUD，验证 data binding + sprite 图集方案。

#### Step 1.1: RML 文档

**新建** `assets/ui/rmlui/hud/time_clock.rml` + `time_clock.rcss`

- 布局：左侧时钟 sprite + 右侧 Day / Time 文本
- 时钟指针通过 data binding 选择 sprite frame（`data-attr-class` 切换 CSS class 控制背景 sprite）
- 日/时文本通过 data binding 绑定 `{{day}}` / `{{time}}`

#### Step 1.2: Data Model

在 `GameScene` 中创建 time clock data model：

```cpp
struct TimeClockModel {
    int day;
    std::string time_text;     // "HH:MM"
    int clock_hand_frame;      // 0-7
};
```

每帧从 `GameTime` 读取并 `DirtyVariable()` 通知 RmlUi。

#### Step 1.3: 替换

- 从 `GameScene::initUI()` 中移除 `TimeClockUI` 创建代码
- 改为加载 `time_clock.rml` 文档
- 删除 `src/game/ui/time_clock_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/hud/time_clock.rml` |
| 新建 | `assets/ui/rmlui/hud/time_clock.rcss` |
| 修改 | `src/game/scene/game_scene.cpp`（替换 TimeClockUI） |
| 删除 | `src/game/ui/time_clock_ui.h/cpp` |

**验证**：时钟 HUD 显示正常、时间动态更新、指针随时间旋转。

---

### Phase 2: 全屏 Overlay — UIScreenFade（含 MapTransitionSystem 适配）

**目标**：用 RmlUi 实现全屏淡入淡出效果，同时保证 `MapTransitionSystem` 的异步加载流程不断裂。

#### Step 2.1: RML ScreenFade 实现

**新建** `assets/ui/rmlui/overlay/screen_fade.rml` + `screen_fade.rcss`

- 一个全屏 `<div>`，CSS `background-color: black` + `opacity` 属性
- 通过 C++ 侧定时器每帧更新 `opacity`（RmlUi CSS transition 不支持精确的回调时机，需要 C++ 侧驱动以保证状态机可靠性）

#### Step 2.2: RmlScreenFade 类

**新建** `src/engine/ui/rmlui/rml_screen_fade.h/cpp`

- `RmlScreenFade : public IScreenFade`
- 内部持有 `Rml::ElementDocument*` + `Rml::Element*`（全屏遮罩元素）
- 维护与旧 `UIScreenFade` 相同的 4 阶段状态机（Idle → FadingOut → Holding → FadingIn → Idle）
- `fadeOut(seconds)` / `fadeIn(seconds)` 启动过渡，每帧由外部调用 `update(delta_time)` 推进 alpha 插值
- `phase()` 返回当前状态——`MapTransitionSystem` 的轮询契约不变

#### Step 2.3: 替换

- `GameScene::initUI()` 中创建 `RmlScreenFade` 替代旧 `UIScreenFade`
- `MapTransitionSystem::setFadeOverlay()` 已在 Phase 0 改为接受 `IScreenFade*`，此处传入 `RmlScreenFade*`
- 旧 `UIScreenFade` 不再被引用（最终在 Phase 8 随框架一起删除）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/overlay/screen_fade.rml/rcss` |
| 新建 | `src/engine/ui/rmlui/rml_screen_fade.h/cpp` |
| 修改 | `src/game/scene/game_scene.h/cpp`（替换 ScreenFade 创建） |

**验证**：
- 地图切换时淡出→黑屏→加载→淡入流程完整
- `MapTransitionSystem` 轮询 `phase()` 时序正确（FadingOut → Holding → FadingIn → Idle）
- 非地图切换场景的全屏 fade 效果正常

---

### Phase 3: 菜单场景

**目标**：将 TitleScene、PauseMenuScene、RestDialogScene、SaveSlotSelectScene 的 UI 迁移到 RmlUi。这些是独立的全屏/模态 UI，不涉及拖拽，适合批量迁移。

#### Step 3.1: TitleScene

**新建** `assets/ui/rmlui/scenes/title.rml` + `title.rcss`

- 背景图 + Logo（CSS animation 实现上下浮动）
- 按钮列表（Start / Load / Menu / Exit）
- 按钮点击通过 `RmlEventBridge` → scene method 调用

**修改** `src/game/scene/title_scene.cpp`：移除 UIManager 创建，改为加载 RML 文档

#### Step 3.2: PauseMenuScene

**新建** `assets/ui/rmlui/scenes/pause_menu.rml` + `pause_menu.rcss`

- 半透明遮罩（CSS `background-color: rgba(0,0,0,0.5)`）
- 中央面板 + 按钮组（Resume / Save / Load / BackToTitle）
- 音量/时间刻度控制：data binding 绑定数值，±按钮触发 C++ 回调

**修改** `src/game/scene/pause_menu_scene.cpp`

#### Step 3.3: RestDialogScene

**新建** `assets/ui/rmlui/scenes/rest_dialog.rml` + `rest_dialog.rcss`

- 模态面板 + 小时±调节 + 确认/取消
- data binding 绑定 hours 数值

**修改** `src/game/scene/rest_dialog_scene.cpp`

#### Step 3.4: SaveSlotSelectScene

**新建** `assets/ui/rmlui/scenes/save_slot_select.rml` + `save_slot_select.rcss`

- 存档槽列表（`data-for` 循环渲染）
- 覆写确认模态框（CSS visibility 控制显隐）

**修改** `src/game/scene/save_slot_select_scene.cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/scenes/title.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/pause_menu.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/rest_dialog.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/save_slot_select.rml/rcss` |
| 修改 | `src/game/scene/title_scene.h/cpp` |
| 修改 | `src/game/scene/pause_menu_scene.h/cpp` |
| 修改 | `src/game/scene/rest_dialog_scene.h/cpp` |
| 修改 | `src/game/scene/save_slot_select_scene.h/cpp` |

**验证**：所有菜单场景正常交互、按钮响应、数值调节正确、场景切换流畅。

---

### Phase 4: 对话气泡 + Tooltip

**目标**：迁移世界锚定 UI 和鼠标跟随 UI。

#### Step 4.1: DialogueBubbleView

**新建** `assets/ui/rmlui/hud/dialogue_bubble.rml` + `dialogue_bubble.rcss`

- 绝对定位面板，通过 C++ 每帧设置 `left` / `top`（世界→屏幕坐标转换）
- 支持 3 通道：加载 3 份文档实例（RmlUi 支持同文档多实例）
- 文本通过 data binding 更新
- 气泡背景使用 sprite sheet decorator 引用现有 `dialogue box.png`

**替换** `DialogueBubbleController` 中的 View 引用，改为操作 RML 文档元素

#### Step 4.2: ItemTooltipUI

**新建** `assets/ui/rmlui/hud/item_tooltip.rml` + `item_tooltip.rcss`

- 绝对定位，每帧跟随鼠标
- data binding：`{{item_name}}` / `{{item_category}}` / `{{item_description}}`
- CSS 控制文字换行（`word-break`）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/hud/dialogue_bubble.rml/rcss` |
| 新建 | `assets/ui/rmlui/hud/item_tooltip.rml/rcss` |
| 修改 | `src/game/ui/dialogue_bubble_controller.h/cpp` |
| 删除 | `src/game/ui/dialogue_bubble_view.h/cpp` |
| 删除 | `src/game/ui/item_tooltip_ui.h/cpp` |
| 修改 | `src/game/scene/game_scene.cpp` |

**验证**：对话气泡跟随 NPC 移动、文本正确换行、tooltip 跟随鼠标、屏幕边缘不溢出。

---

### Phase 5: 战斗 UI

**目标**：迁移 BattleScene 的 UI。

**新建** `assets/ui/rmlui/scenes/battle.rml` + `battle.rcss`

- 战斗面板：回合信息、单位状态、行动结果
- 操作按钮（Attack / Skill / Item / Guard / Escape / EndTurn）
- 按钮启禁状态通过 data binding 的 `data-if` / `data-attr-class` 控制
- 状态机不变，仅 UI 层替换

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/scenes/battle.rml/rcss` |
| 修改 | `src/game/scene/battle_scene.h/cpp` |

**验证**：战斗流程全通、按钮状态正确切换、结果显示正常。

---

### Phase 6: 快捷栏 — HotbarUI

**目标**：迁移带拖拽交互的快捷栏。

**新建** `assets/ui/rmlui/hud/hotbar.rml` + `hotbar.rcss`

- 水平 10 槽布局（CSS flexbox）
- 每槽：图标 + 数量标签 + 选中高亮
- data binding：`data-for="slot : hotbar_slots"`，每个 slot 绑定 `item_icon` / `item_count` / `is_active`
- 拖拽：RmlUi `drag="drag-drop"` + `dragdrop` 事件 → `HotbarBindCommand` / `HotbarUnbindCommand`
- 右键使用：`contextmenu` 或自定义事件 → `UseItemCommand`

**替换** `src/game/ui/hotbar_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/hud/hotbar.rml/rcss` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 删除 | `src/game/ui/hotbar_ui.h/cpp` |

**验证**：10 槽显示正确、拖拽物品到/从快捷栏、活跃槽高亮、右键使用物品。

---

### Phase 7: 物品栏 — InventoryUI

**目标**：迁移最复杂的 UI 组件。

**新建** `assets/ui/rmlui/hud/inventory.rml` + `inventory.rcss`

- 可拖动面板（RmlUi `drag="drag"` 在标题栏 + `move_target` 属性）
- 5×4 网格：RmlUi 6.2 不支持 CSS Grid，采用 **flexbox wrap** 布局——外层容器 `display: flex; flex-wrap: wrap; width: 5*slot_width`，每个 slot 固定宽度，`data-for` 循环渲染 20 个 slot
- 分页：上/下页按钮 + 页码标签，通过 data binding 切换当前页数据
- 关闭按钮
- 拖拽排序：slot 间拖拽 → `InventoryMoveCommand`
- 跨 UI 拖拽：inventory slot ↔ hotbar slot
  - RmlUi drag-drop 支持跨文档拖放（同一 `Rml::Context` 内）
  - 拖拽源标记 `data-source="inventory"` `data-slot-index="N"`
  - 目标通过 `dragdrop` 事件解析来源并分发对应命令
- 右键使用：→ `UseItemCommand`

**替换** `src/game/ui/inventory_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/hud/inventory.rml/rcss` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 删除 | `src/game/ui/inventory_ui.h/cpp` |
| 删除 | `src/game/ui/ui_drag_drop_helpers.h` |

**验证**：物品栏开关正常、分页切换、拖拽排序、跨 UI 拖拽（inventory ↔ hotbar）、右键使用、物品数量显示正确。

---

### Phase 8: 清理 — 移除旧 UI 框架

**目标**：移除所有不再使用的 `engine::ui` 代码和相关资源。

> **前置约束**：Phase 1-7 的每个阶段已将对应的旧测试替换为新测试（见下方 Step 8.1）。此处仅做最终清扫。

#### Step 8.1: 测试迁移（贯穿 Phase 1-7，此处汇总要求）

采用 **旧删 ↔ 新补 成对推进** 策略：每个 Phase 删除旧 UI 代码时，同步删除对应旧测试并补建 RmlUi 等价测试。

| 旧测试 | 删除时机 | 对应新测试 |
|--------|----------|-----------|
| `engine/ui/ui_button_factory_api_test.cpp` | Phase 8 | N/A（框架移除，无需替代） |
| `engine/ui/ui_preset_manager_*_test.cpp`（4 个） | Phase 8 | N/A（PresetManager 移除） |
| `engine/ui/ui_interaction_state_source_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_interaction_runtime_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_layout_source_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_layout_invalidation_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_world_anchor_test.cpp` | Phase 4 | 对话气泡世界锚定测试（RML 版） |
| `engine/ui/ui_stack_layout_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_grid_layout_test.cpp` | Phase 8 | N/A |
| `game/ui_layout_integration_test.cpp` | Phase 7 | inventory/hotbar RML 集成测试 |
| `game/pause_menu_scene_async_save_ui_test.cpp` | Phase 3 | pause menu RML 版异步保存测试 |
| `engine/render/rmlui_pipeline_stage_test.cpp` | **保留** | 更新以匹配新渲染顺序 |
| `engine/input/input_manager_rmlui_routing_test.cpp` | **保留** | 不变 |

**原则**：
- 框架类测试（布局/交互/preset）在 Phase 8 统一删除——因为被测代码已不存在，无需替代
- 行为类测试（世界锚定/集成/场景交互）在对应迁移阶段 **先补新测再删旧测**
- RmlUi 管线和输入路由测试保留并更新

#### Step 8.2: 清理工具链依赖

**删除** `tools/ui_tester/` 整个目录

**修改** `tools/visual_tester/visual_test_cases.cpp`：
- 移除 `RenderPassCoverageVisualTest` 中的 `drawUIFilledRect()` / `drawUIImage()` 调用
- 该测试用例的 UI pass 覆盖部分改为注释标记或直接移除（visual_tester 的核心价值在场景/光照/特效通道验证，UI pass 验证已由 RmlUi 管线测试覆盖）

#### Step 8.3: 移除 TextRenderer UI 通道

**修改** `src/engine/render/text_renderer.h/cpp`：
- 移除 `use_ui_pass` 参数及相关分支
- 移除 `drawScreenText()` 方法（所有屏幕文字已由 RmlUi 承担）
- 仅保留世界文字渲染路径（scene pass）

#### Step 8.4: 移除 Game UI 残留

- 确认 `src/game/ui/` 目录下所有文件已被删除或替换
- 移除 `DialogueBubbleController` 对旧 View 的依赖（如已在 Phase 4 完成则跳过）

#### Step 8.5: 移除 Engine UI 框架

**删除整个目录**（RmlUi 子目录和 `screen_fade_interface.h` 除外）：

- `src/engine/ui/ui_element.h/cpp`
- `src/engine/ui/ui_interactive.h/cpp`
- `src/engine/ui/ui_panel.h/cpp`
- `src/engine/ui/ui_button.h/cpp`
- `src/engine/ui/ui_label.h/cpp`
- `src/engine/ui/ui_image.h/cpp`
- `src/engine/ui/ui_item_slot.h/cpp`
- `src/engine/ui/ui_progress_bar.h/cpp`
- `src/engine/ui/ui_screen_fade.h/cpp`（旧实现，IScreenFade 接口保留）
- `src/engine/ui/ui_input_blocker.h/cpp`
- `src/engine/ui/ui_draggable_panel.h/cpp`
- `src/engine/ui/ui_drag_preview.h/cpp`
- `src/engine/ui/ui_defaults.h`
- `src/engine/ui/ui_manager.h/cpp`
- `src/engine/ui/ui_preset_manager.h/cpp`
- `src/engine/ui/layout/` 整个目录
- `src/engine/ui/behavior/` 整个目录

#### Step 8.6: 移除 UIPass + drawUI* API

- 删除 `src/engine/render/opengl/ui_pass.h/cpp`
- 从 `GLRenderer` 中移除 `ui_pass_` 成员及所有 `drawUIRect` / `drawUITexture` / `drawUIRectGradient` / `drawUITextureGradient` 方法
- 从 `Renderer` 中移除 `drawUIImage` / `drawUIFilledRect` / `drawUINineSliceInternal` 方法
- 调整 `present()` 渲染顺序：`... → RmlUI → ImGui → SwapWindow`（移除 UIPass flush 步骤）

> **执行前检查**：用 grep 确认无任何文件仍引用 `drawUI*` 方法。Step 8.2/8.3 应已消除 visual_tester 和 TextRenderer 的引用。

#### Step 8.7: 移除 UIPresetManager

- 从 `engine::core::Context` 的 `ResourceServices` 中移除 `UIPresetManager`
- 删除 UI preset 调试面板（`ui_preset_debug_panel.h/cpp`）

#### Step 8.8: 移除 Scene 基类 UIManager 引用

- 从 `engine::scene::Scene` 中移除 `ui_manager_` 成员
- 各场景不再持有 `UIManager`

#### Step 8.9: 清理 CMakeLists + 测试注册

- 从 `src/CMakeLists.txt` 中移除已删除的源文件
- 从 `tests/CMakeLists.txt` 中移除已删除的旧 UI 测试注册（Step 8.1 表中标记删除的项）
- 从 `tools/CMakeLists.txt` 中移除 `ui_tester` 目标

#### Step 8.10: 清理资产引用

- 审查 `assets/data/icon_config.json` 中 `indicator` 分类（cursor/hand 图标），如仅被旧 UIManager 使用则可移除
- 删除 `assets/data/ui_button_presets.json` 和 `assets/data/ui_image_presets.json`（已由 RCSS spritesheet 替代）

**验证矩阵**：
1. 构建通过（无编译错误、无未解析符号）
2. 全部游戏 UI 功能正常（逐一回归测试 Phase 1-7 验证项）
3. ImGui 调试面板正常工作
4. `tools/visual_tester` 构建运行正常（UI 部分已移除）
5. 全部测试通过（新 RmlUi 测试 + 保留的管线/输入路由测试）
6. 无内存泄漏（RML 文档正确卸载）

---

## 涉及文件总览

### 新建

| 文件 | 阶段 |
|------|------|
| `src/engine/ui/rmlui/rml_data_bridge.h/cpp` | Phase 0 |
| `src/engine/ui/rmlui/rml_event_bridge.h/cpp` | Phase 0 |
| `src/engine/ui/screen_fade_interface.h` | Phase 0 |
| `assets/ui/rmlui/theme/base.rcss` | Phase 0 |
| `assets/ui/rmlui/theme/spritesheet.rcss` | Phase 0 |
| `assets/ui/rmlui/theme/animation.rcss` | Phase 0 |
| `assets/ui/rmlui/hud/time_clock.rml/rcss` | Phase 1 |
| `assets/ui/rmlui/overlay/screen_fade.rml/rcss` | Phase 2 |
| `src/engine/ui/rmlui/rml_screen_fade.h/cpp` | Phase 2 |
| `assets/ui/rmlui/scenes/title.rml/rcss` | Phase 3 |
| `assets/ui/rmlui/scenes/pause_menu.rml/rcss` | Phase 3 |
| `assets/ui/rmlui/scenes/rest_dialog.rml/rcss` | Phase 3 |
| `assets/ui/rmlui/scenes/save_slot_select.rml/rcss` | Phase 3 |
| `assets/ui/rmlui/hud/dialogue_bubble.rml/rcss` | Phase 4 |
| `assets/ui/rmlui/hud/item_tooltip.rml/rcss` | Phase 4 |
| `assets/ui/rmlui/scenes/battle.rml/rcss` | Phase 5 |
| `assets/ui/rmlui/hud/hotbar.rml/rcss` | Phase 6 |
| `assets/ui/rmlui/hud/inventory.rml/rcss` | Phase 7 |

### 修改

| 文件 | 阶段 |
|------|------|
| `src/engine/ui/rmlui/rml_ui_layer.h/cpp` | Phase 0 |
| `src/engine/core/context.h/cpp` | Phase 0 |
| `src/engine/scene/scene.h/cpp` | Phase 0 |
| `src/engine/scene/scene_manager.cpp` | Phase 0 |
| `src/game/system/map_transition_system.h/cpp` | Phase 0（IScreenFade 接口切换） |
| `src/game/scene/game_scene.h/cpp` | Phase 1-7 |
| `src/game/scene/title_scene.h/cpp` | Phase 3 |
| `src/game/scene/pause_menu_scene.h/cpp` | Phase 3 |
| `src/game/scene/rest_dialog_scene.h/cpp` | Phase 3 |
| `src/game/scene/save_slot_select_scene.h/cpp` | Phase 3 |
| `src/game/ui/dialogue_bubble_controller.h/cpp` | Phase 4 |
| `src/game/scene/battle_scene.h/cpp` | Phase 5 |
| `tools/visual_tester/visual_test_cases.cpp` | Phase 8（移除 UI 调用） |
| `src/engine/render/text_renderer.h/cpp` | Phase 8（移除 UI 通道） |
| `src/engine/render/opengl/gl_renderer.h/cpp` | Phase 8 |
| `src/engine/render/renderer.h/cpp` | Phase 8 |
| `src/CMakeLists.txt` | Phase 8 |
| `tests/CMakeLists.txt` | Phase 3-8（逐步更新测试注册） |

### 删除

| 文件 | 阶段 |
|------|------|
| `src/game/ui/time_clock_ui.h/cpp` | Phase 1 |
| `src/game/ui/dialogue_bubble_view.h/cpp` | Phase 4 |
| `src/game/ui/item_tooltip_ui.h/cpp` | Phase 4 |
| `src/game/ui/hotbar_ui.h/cpp` | Phase 6 |
| `src/game/ui/inventory_ui.h/cpp` | Phase 7 |
| `src/game/ui/ui_drag_drop_helpers.h` | Phase 7 |
| `tools/ui_tester/` 整个目录 | Phase 8 |
| `src/engine/ui/`（除 rmlui/ 和 screen_fade_interface.h）全部文件 | Phase 8 |
| `src/engine/render/opengl/ui_pass.h/cpp` | Phase 8 |
| `src/engine/debug/panels/ui_preset_debug_panel.h/cpp` | Phase 8 |
| `assets/data/ui_button_presets.json` | Phase 8 |
| `assets/data/ui_image_presets.json` | Phase 8 |
| `assets/ui/rmlui/demo.rml/rcss` | Phase 8 |
| 旧 UI 测试文件（12 个，见 Step 8.1 表） | Phase 3-8 |
