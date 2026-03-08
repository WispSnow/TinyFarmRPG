### Phase 7: 物品栏 — InventoryUI

**目标**：将 `InventoryUI` 迁移到 RmlUi，同时保留现有显示内容、分页语义、右键使用、物品排序，以及与 `HotbarUI` 的跨 UI 拖拽行为。

## 范围修订

**本 Phase 包含**：

- 物品栏面板、20 个可见槽位（5×4）、分页区、关闭按钮的 RmlUi 渲染
- 物品图标 / 数量显示 / Tooltip
- 左键拖拽排序（inventory 内部 move / swap / merge）
- 右键使用物品
- `inventory ↔ hotbar` 跨 UI 拖拽
- 面板拖动
- `show()` / `hide()` / `toggle()` / 页码同步 / 打开时同步等既有行为保持不变
- `GameScene` 对 `InventoryUI` 的接入改为与 `HotbarUI` 一致的 RmlUi wrapper 模式

**本 Phase 不包含**：

- `ItemTooltipUI` / `HotbarUI` / `DialogueBubbleView` 的新功能扩展
- 旧 UI 框架的整体移除
- `UIManager` / `UIItemSlot` / `UIDraggablePanel` 等旧类的最终删除（统一留到 Phase 8）

## 当前草案需要修正的点

当前简版计划可行，但存在以下关键偏差，需要修正后再进入实现：

1. **不应再把 `InventoryUI` 规划为旧 `UIElement` 子树**
   - Phase 6 中 `HotbarUI` 已改为独立 RmlUi wrapper。
   - Phase 7 应与之保持一致：`InventoryUI` 改为独立类，由 `GameScene` 直接持有 `std::unique_ptr<InventoryUI>`。
   - 因此不是“删除 `inventory_ui.h/cpp`”，而是**原地重写其内部实现**。

2. **面板拖动不应写成“标题栏 + drag=drag”**
   - 旧实现 `UIDraggablePanel` 是整个外壳可拖，不存在显式标题栏。
   - RmlUi 中更合适的方案是 `<handle move_target="#inventory-panel">`，但要避免覆盖槽位与按钮交互区。
   - 建议使用**显式拖动区域**（panel chrome / 空白区），而不是用整面板透明覆盖层吞掉交互。

3. **跨 UI 拖拽不能只改 `InventoryUI`，还必须补 `HotbarUI`**
   - `inventory -> hotbar` 的 drop 事件落在 hotbar 文档上。
   - `hotbar -> inventory` 的 drop 事件落在 inventory 文档上。
   - 因此 Phase 7 必须包含对 `HotbarUI` 的**最小增强**，使其能识别来自 inventory 的拖拽源。

4. **不能继续沿用 `UIItemSlot*` 命中测试桥接**
   - 旧 `findSlotIndex(const UIItemSlot*)` / `resolveInventoryIndex(...)` 基于旧 UI 树。
   - 迁移后跨文档拖放应改为**RmlUi DOM 元素属性 + drag metadata**，不再依赖旧 `UIItemSlot*`。

5. **面板尺寸应按 RmlUi content-box 重算**
   - 旧代码的 `panel_size` 是总视觉尺寸。
   - RmlUi `width/height` 是 content-box，需要提前扣掉 padding，避免出现与 Phase 5 类似的 box model 偏差。

## 迁移策略

**推荐方案**：与 Phase 6 的 `HotbarUI` 保持一致，将 `InventoryUI` 改为独立 RmlUi wrapper。

### 方案对比

**方案 A（推荐）**：`InventoryUI` 改为独立类，`GameScene` 直接持有
- 优点：
  - 与 `HotbarUI` 架构统一
  - 不再依赖旧 `UIManager` 拖拽预览 / 命中测试
  - 跨文档拖放实现更自然
  - Phase 8 清理旧 UI 框架成本最低
- 缺点：
  - 需要同步改 `GameScene` 接线与部分测试

**方案 B**：保留 `UIElement` 外壳，内部再挂 RmlUi 文档
- 优点：
  - 表面改动看似更小
- 缺点：
  - 同时维护两套生命周期和交互语义
  - 会继续拖住 `UIManager` / `UIItemSlot` 兼容层
  - Phase 8 还要再做一次拆除

本项目当前已经完成 `HotbarUI` 迁移，因此 Phase 7 应采用 **方案 A**。

## 目标形态

- **保留** `game::ui::InventoryUI` 类名与主要公共接口
- **替换**其内部实现为：
  - `RmlUILayer&`
  - `RmlDataBridge`
  - `Rml::DataTypeRegister`
  - `Rml::ElementDocument*`
  - 适量运行时状态（当前页、拖拽状态、缓存物品数据）
- **不再使用**：
  - `UIDraggablePanel`
  - `UIGridLayout`
  - `UIItemSlot`
  - `DragBehavior`
  - `HoverBehavior`
  - `ui_drag_drop_helpers.h`
- `GameScene` 改为：
  - `std::unique_ptr<InventoryUI> inventory_ui_`
  - 不再将 inventory 加入旧 `UIManager`
- `ItemTooltipUI` 暂保持现状，继续作为 tooltip 提供方

## UI 文件

**新建**：

- `ui/rmlui/hud/inventory.rml`
- `ui/rmlui/hud/inventory.rcss`

**修改**：

- `ui/rmlui/theme/spritesheet.rcss`
- `src/game/ui/inventory_ui.h`
- `src/game/ui/inventory_ui.cpp`
- `src/game/ui/hotbar_ui.h`
- `src/game/ui/hotbar_ui.cpp`
- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- `tests/game/ui_layout_integration_test.cpp`

**可选共享提取**（推荐本 Phase 一并完成）：

- 新增共享 helper，例如 `src/game/ui/rml_item_icon_helpers.h`
  - 提取 `icon_id -> sprite decorator` 生成逻辑
  - 避免在 `InventoryUI` 中复制 `HotbarUI::buildIconDecorator()` / `spriteNameFromIconKey()`

## 旧视觉参数（需保持一致）

以下为旧 `InventoryUI` 的精确布局参数，Phase 7 应以此为基准：

| 属性 | 值 |
|------|----|
| Grid columns | 5 |
| Grid rows | 4 |
| Visible slots | 20 |
| Slot size | `32dp × 32dp` |
| Slot spacing | `6dp × 6dp` |
| Grid visual size | `184dp × 146dp` |
| Panel padding | `12dp` 四边 |
| Bottom button area | `16dp` |
| Panel total visual size | `208dp × 186dp` |
| Initial right margin | `20dp` |
| Initial top-left | `x = 412dp`, `y = 87dp`（逻辑分辨率 `640×360`） |
| Close button size | `16dp × 16dp` |
| Page button size | `20dp × 20dp` |
| Page label font | `16dp` |

### RmlUi content-box 换算

旧面板总视觉尺寸是 `208×186`，而 RmlUi `width/height` 是 content-box，因此：

- `width = 184dp`
- `height = 162dp`（`146 + 16`）
- `padding = 12dp`

这样总视觉尺寸才会回到 `208×186`。

## 视觉资源要求

### 1. Inventory panel / slot 精灵

需要在 `spritesheet.rcss` 中新增 inventory 精灵，复用旧资源：

```rcss
@spritesheet ui-inventory {
    src: ../../../assets/farm-rpg/UI/Inventory/inventory.png;

    inventory-panel-bg:        0px 64px 48px 48px;
    inventory-panel-bg-inner: 10px 74px 28px 28px;   /* L10 T10 R10 B10 */

    inventory-slot-bg:        39px 9px 18px 18px;
    inventory-slot-bg-inner:  41px 11px 14px 14px;   /* L2 T2 R2 B2 */
}
```

### 2. Page / close 按钮精灵

按钮资源来自 `HUD.png`，建议在现有 `ui-hud` 中补充：

- `page-left-icon` / `page-left-icon-pressed`（已存在，可复用）
- `page-right-icon` / `page-right-icon-pressed`（已存在，可复用）
- `close-icon`
- `close-icon-pressed`

### 3. 物品图标 decorator

继续复用 Phase 6 已建立的规则：

- `image(item-tools-hoe)`
- `image(item-seeds-strawberry-seed)`
- `image(item-materials-stone)`

Inventory 不应重新拼贴图路径和 source rect，而应与 Hotbar 使用同一套 decorator 生成逻辑。

## DOM 结构建议

```rml
<body data-model="inventory_ui">
    <div id="inventory-panel">
        <div id="inventory-drag-handle">
            <handle move_target="#inventory-panel"></handle>
        </div>

        <button id="inventory-close" data-command="close"></button>

        <div id="inventory-grid">
            <div class="inventory-slot"
                 data-for="slot : inventory_slots"
                 data-class-has-item="slot.has_item"
                 data-class-has-count="slot.has_count"
                 data-event-mouseup="slot_mouse_up(slot.inventory_index)"
                 data-event-dragdrop="slot_drag_drop(slot.inventory_index)">
                <div class="inventory-slot-drag-proxy"
                     data-class-draggable="slot.can_drag"
                     data-event-mouseover="slot_hover_enter(slot.inventory_index)"
                     data-event-mouseout="slot_hover_exit(slot.inventory_index)"
                     data-event-dragstart="slot_drag_start(slot.inventory_index)"
                     data-event-dragend="slot_drag_end(slot.inventory_index)">
                    <div class="inventory-slot-icon" data-style-decorator="slot.icon_decorator"></div>
                    <div class="inventory-slot-count">{{ slot.count_text }}</div>
                </div>
            </div>
        </div>

        <div id="inventory-pagination">
            <button id="inventory-page-left" data-command="page_left"></button>
            <div id="inventory-page-label">{{ page_text }}</div>
            <button id="inventory-page-right" data-command="page_right"></button>
        </div>
    </div>
</body>
```

### 拖动区域说明

不要把 `<handle>` 做成覆盖整个 panel 的透明层，否则会吞掉槽位与按钮交互。

建议：

- 将拖动区域限制在 **panel chrome / 空白区域**
- 至少覆盖：
  - 顶部 padding 区
  - 关闭按钮以外的顶部空白区
  - 若需要，也可覆盖底部分页条背景空白区
- 不覆盖：
  - inventory grid 槽位区
  - close button
  - page buttons

## Data Model 设计

### `InventorySlotViewModel`

建议字段：

| 字段 | 类型 | 用途 |
|------|------|------|
| `local_slot_index` | `int` | 当前页本地索引 `0..19` |
| `inventory_index` | `int` | 全局物品栏索引 `0..39` |
| `icon_decorator` | `Rml::String` | 图标 decorator |
| `count_text` | `Rml::String` | 数量文本 |
| `has_item` | `bool` | 控制图标显隐 |
| `has_count` | `bool` | 控制数量显隐（仅 `count > 1`） |
| `can_drag` | `bool` | 空槽禁止拖拽 |

### 根模型字段

| 字段 | 类型 | 用途 |
|------|------|------|
| `inventory_slots` | `std::vector<InventorySlotViewModel>` | 当前页 20 个可见槽位 |
| `page_text` | `Rml::String` | 例如 `1/2` |

说明：

- 不需要把全部 40 槽直接 `data-for` 到 DOM，保持与旧版一样只渲染当前页 20 槽即可。
- 页切换时，更新 `inventory_slots` + `page_text` 并统一 `markDirty()`。
- 拖拽期间**不要修改 `inventory_slots`**；与 Hotbar 一样，视觉预览由 `drag: clone` 提供，真实刷新等待命令执行后的 `InventoryChanged`。

## 事件与交互方案

### 1. 静态按钮

建议使用 `RmlEventBridge + data-command`：

- `close`
- `page_left`
- `page_right`

理由：

- 三个按钮是静态控件，无需 `BindEventCallback` 传索引
- 与 Phase 3/5 的静态按钮桥接模式一致

### 2. 槽位事件

建议继续使用 `BindEventCallback(...)`，原因是每个槽位都需要携带 `inventory_index`。

需要的回调：

- `slot_mouse_up(inventory_index)`
- `slot_hover_enter(inventory_index)`
- `slot_hover_exit(inventory_index)`
- `slot_drag_start(inventory_index)`
- `slot_drag_drop(inventory_index)`
- `slot_drag_end(inventory_index)`

### 3. 右键使用

迁移后删除旧的 `InputManager::onAction("mouse_right")` 监听。

改为在槽位 `mouseup` 回调中读取：

- `button == 0`：普通点击（本 Phase 不引入额外行为，可无操作）
- `button == 2`：右键使用 → `UseItemCommand{target, inventory_index, 1, true}`

要求：

- 槽位内右键必须在 RmlUi 侧消费，避免传播到世界交互层。
- 空槽右键也应吞掉事件，以保持旧行为“右键点在物品栏上不会落到场景”。

### 4. Hover Tooltip

保持现有语义：

- hover 有物品的槽位 → `ItemTooltipUI::showItem(...)`
- hover 离开 / 开始拖拽 / 关闭物品栏 → `hideTooltip()`

### 5. 分页

保留旧分页语义：

- 页面总数固定来自 `InventoryComponent::PAGE_COUNT`
- 页码显示如 `1/2`
- 左右按钮即使点击到边界页，也只是 clamp，不引入额外弹窗或提示
- 页切换时派发 `InventorySetActivePageCommand`

## 拖拽语义

### A. Inventory 内部拖拽

- 源：inventory slot drag proxy
- 目标：inventory slot root
- drop 在不同 inventory slot：
  - 派发 `InventoryMoveCommand{target, from_slot, to_slot, true}`
- drop 在同槽位：no-op

### B. Inventory -> Hotbar

- drop 事件落在 **HotbarUI** 的目标槽位上
- 因此 Phase 7 必须扩展 `HotbarUI::onSlotDragDrop(...)`：
  - 识别 `drag_element` 来自 `inventory`
  - 读取源 `inventory_index`
  - 派发 `HotbarBindCommand{target, hotbar_index, inventory_index}`

### C. Hotbar -> Inventory

- drop 事件落在 **InventoryUI** 的目标槽位上
- `InventoryUI::onSlotDragDrop(...)` 需要识别 `drag_element` 来自 `hotbar`
- 读取源 `inventory_index`（不是 hotbar index）
- 派发 `InventoryMoveCommand{target, src_inventory_slot, dst_inventory_slot, true}`

### D. 数据来源标记

建议在 drag proxy 元素上显式设置 metadata：

- `data-drag-source="inventory"` / `"hotbar"`
- `data-inventory-index="..."`
- hotbar 可额外保留 `data-hotbar-index="..."`

在 `dragdrop` 回调中：

- 通过 `event.GetParameter<void*>("drag_element", nullptr)` 拿到源元素
- 再从源元素属性中读取 source / inventory index
- 不再依赖旧 `UIItemSlot*` 命中测试

## C++ 实现要求

### `src/game/ui/inventory_ui.h/cpp`

- `InventoryUI` 改为独立类，不再继承 `UIElement`
- 保留 gameplay-facing 主接口：
  - `setSlotItem()`
  - `clearSlot()`
  - `clearAllSlots()`
  - `setTarget()`
  - `setTooltipUI()`
  - `show()` / `hide()` / `toggle()`
- `setHotbarUI()` 可暂保留为兼容壳，但 Phase 7 内部逻辑不再依赖它做命中测试
- `setUIManager()` 删除
- `findSlotIndex(const UIItemSlot*)` / `resolveInventoryIndex(...)` 不再作为主路径；如需编译兼容，可保留为 no-op / 兼容壳，最终在 Phase 8 删除

内部持有：

- `engine::ui::rmlui::RmlUILayer&`
- `engine::ui::rmlui::RmlDataBridge`
- `Rml::DataTypeRegister`
- `Rml::ElementDocument*`
- `std::vector<InventorySlotViewModel> inventory_slots_`
- 全量缓存 `slot_items_`（40 槽）
- 当前页、拖拽状态、tooltip 状态等运行时数据

### `src/game/ui/hotbar_ui.h/cpp`

本 Phase 允许做**最小增强**，以支持跨文档拖放：

- `onSlotDragDrop(...)` 支持识别来自 inventory 的拖拽源
- 必要时在 Hotbar slot DOM 上增加 source metadata 读取辅助
- 不对 hotbar 布局与现有语义做额外改造

### `src/game/scene/game_scene.h/cpp`

- `inventory_ui_` 改为 `std::unique_ptr<InventoryUI>`
- `initUI()` 中不再把 inventory 加入 `UIManager`
- 与 `hotbar_ui_` 一样直接构造 RmlUi wrapper
- `GameScene::clean()` 中：
  - 先 `inventory_ui_.reset()`
  - 再 `hotbar_ui_.reset()` / 其他 wrapper reset
  - 最后 `Scene::clean()`

这样可以保证：

- wrapper 析构 → 先移除桥接 / 销毁 data model / 失效文档指针
- `Scene::clean()` → 再统一 `unloadAllRmlDocuments()`

### 旧辅助清理

完成迁移后，可在本 Phase 删除：

- `src/game/ui/ui_drag_drop_helpers.h`

但 `UIManager::beginDragPreview/endDragPreview()` 等旧基础设施是否继续删除，留到 Phase 8 统一处理。

## 验证要求

至少覆盖以下验证：

1. 打开 / 关闭物品栏正常
2. 关闭后再次打开不崩溃
3. 面板初始位置与旧版一致（右侧居中）
4. 面板拖动正常，不吞掉槽位 / 按钮交互
5. 20 个槽位的 5×4 布局、间距、分页区位置与旧版一致
6. 页码切换正常，`InventorySetActivePageCommand` 正常派发
7. 物品图标、数量显示正确（数量仅在 `count > 1` 显示）
8. Hover Tooltip 正常
9. 右键使用正常，且点击物品栏不会触发世界交互
10. inventory 内部拖拽 move / swap / merge 正常
11. inventory -> hotbar 拖拽绑定正常
12. hotbar -> inventory 拖拽移动正常
13. 打开 inventory 后同步刷新正常（`InventorySyncCommand`）

## 测试与文档

- 更新 `tests/game/ui_layout_integration_test.cpp`
  - 旧的 UIElement 树布局断言改为 RmlUi 文档级断言
  - 若当前 headless 测试环境没有 `RmlUILayer`，允许 `GTEST_SKIP()`
- 如 Inventory 相关旧测试构造函数或持有方式失配，统一同步到新接口
- 完成后回写 `plans/rmlui-migration/README.md`，将 Phase 7 标记为完成或部分完成

## 涉及文件（修订版）

| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/inventory.rml` |
| 新建 | `ui/rmlui/hud/inventory.rcss` |
| 修改 | `ui/rmlui/theme/spritesheet.rcss` |
| 修改 | `src/game/ui/inventory_ui.h` |
| 修改 | `src/game/ui/inventory_ui.cpp` |
| 修改 | `src/game/ui/hotbar_ui.h` |
| 修改 | `src/game/ui/hotbar_ui.cpp` |
| 修改 | `src/game/scene/game_scene.h` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 修改 | `tests/game/ui_layout_integration_test.cpp` |
| 可选新增 | `src/game/ui/rml_item_icon_helpers.h` |
| 删除 | `src/game/ui/ui_drag_drop_helpers.h` |

