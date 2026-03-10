### Phase 4: 对话气泡 + Tooltip

**目标**：迁移世界锚定 UI 和鼠标跟随 UI，同时尽量保持旧版显示内容、图片资源和跟随行为一致。

---

#### Step 4.0: Spritesheet 补充

**修改** `ui/rmlui/theme/spritesheet.rcss`

补充以下现有资源的 RmlUi sprite / ninepatch 定义：

- `assets/farm-rpg/UI/dialogue box.png`
- `assets/farm-rpg/UI/Inventory/Banner.png`

要求：
- 九宫格边距与旧 `ui_image_presets.json` 保持一致
- 背景拉伸后视觉尽量与旧 UI 一致

#### Step 4.1: DialogueBubbleView（保留类名，内部改为 RmlUi）

**新建** `ui/rmlui/hud/dialogue_bubble.rml` + `dialogue_bubble.rcss`

**修改** `src/game/ui/dialogue_bubble_view.h/cpp`

实现策略：
- **保留 `DialogueBubbleView` 类与现有 Controller API**，避免扩大 `GameScene` 与事件系统改动面
- 每个 channel 仍对应一个 `DialogueBubbleView` 实例，但内部不再创建旧 `UIPanel/UILabel`
- 每个实例加载一份 `dialogue_bubble.rml` 文档，并直接操作 DOM 元素（**不使用 data model**，避免多实例共享 data model 的上下文级冲突）
- 文本内容继续沿用 `DialogueBubbleController::formatDialogueText()` 的结果，保证换行结果尽量接近旧版
- 气泡位置继续复用旧 `UIManager` 的 **WorldAnchor 插值**：
  - `DialogueBubbleView` 继续继承 `UIElement`
  - `setWorldAnchor()` / `clearWorldAnchor()` 行为不变
  - 在 `renderSelf()` 中读取 UIManager 已解算的屏幕位置，同步到 RmlUi `left/top`
- 气泡外框尺寸继续按旧逻辑计算：
  - 最小外框 `160x48dp`
  - 内边距 `8dp`
  - 外框随文本尺寸增大

#### Step 4.2: ItemTooltipUI（保留类名，内部改为 RmlUi）

**新建** `ui/rmlui/hud/item_tooltip.rml` + `item_tooltip.rcss`

**修改** `src/game/ui/item_tooltip_ui.h/cpp`

实现策略：
- **保留 `ItemTooltipUI` 类与对外 API**（`showItem` / `hideTooltip` / `setPadding` / `setOffset`），避免改动 `InventoryUI` / `HotbarUI` 调用方
- 内部不再创建旧 `UIPanel/UILabel`，改为加载一份 RmlUi 文档并直接更新 DOM
- 保留旧 tooltip 的关键行为：
  - `max_text_width = 240dp`
  - 偏移 `(12, 16)`
  - 屏幕边缘钳制
  - 旧版 UTF-8 手动换行算法（优先保持显示内容一致，而不是完全依赖 RCSS 自动换行）
- 继续通过 `UIManager::update()` 驱动鼠标跟随；`renderSelf()` 不负责绘制，只同步 RmlUi 属性
- Tooltip 文档默认隐藏，`showItem()` 时显示，`hideTooltip()` 时隐藏

#### Step 4.3: GameScene 集成

**修改** `src/game/scene/game_scene.cpp`

- `TimeClockHud` 保持现状
- `InventoryUI` / `HotbarUI` 保持现状
- `ItemTooltipUI` 构造时传入 `instance_id_`，由其内部加载 RmlUi 文档
- `DialogueBubbleView` 构造时传入 `instance_id_`，由其内部加载 RmlUi 文档
- `DialogueBubbleController` API 保持不变，继续注册 3 个 channel
- `GameScene::clean()` 中提前清理顺序调整为：
  1. 清空对话事件队列
  2. 销毁 `dialogue_controller_`
  3. `ui_manager_.reset()`，让 tooltip / bubble 文档在 wrapper 析构时立即卸载
  4. 最后再进入 `Scene::clean()`

#### Step 4.4: 旧代码清理范围

本 Phase **不删除** 下列类文件，仅替换其内部实现：

- `src/game/ui/dialogue_bubble_view.h/cpp`
- `src/game/ui/item_tooltip_ui.h/cpp`

这样可以：
- 保持 `InventoryUI` / `HotbarUI` / `DialogueBubbleController` 的调用契约稳定
- 避免无意义的 `CMakeLists.txt` 大范围调整
- 将“删除旧 UI 框架类型”的工作推迟到 Phase 8 统一收尾

---

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 修改 | `ui/rmlui/theme/spritesheet.rcss` |
| 新建 | `ui/rmlui/hud/dialogue_bubble.rml/rcss` |
| 新建 | `ui/rmlui/hud/item_tooltip.rml/rcss` |
| 修改 | `src/game/ui/dialogue_bubble_view.h/cpp` |
| 修改 | `src/game/ui/item_tooltip_ui.h/cpp` |
| 修改 | `src/game/scene/game_scene.cpp` |

**验证**：
- 对话气泡跟随 NPC 移动时无明显抖动，关闭/切图/切场景不残留
- 气泡文本换行结果与旧版基本一致，气泡外框最小尺寸与旧版一致
- Tooltip 跟随鼠标，边缘位置自动翻转/钳制，不溢出屏幕
- Tooltip 的标题 / 分类 / 描述排版与旧版接近
- 相关图片资源（对话框 / Banner）拉伸效果与迁移前保持一致
- `GameScene::clean()` 后再次进入场景，不出现残留文档或重复卸载问题

---
