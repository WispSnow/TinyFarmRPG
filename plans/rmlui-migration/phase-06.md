### Phase 6: 快捷栏 — HotbarUI

**目标**：将快捷栏主体迁移到 RmlUi，同时保持现有显示内容、操作语义和外部调用接口稳定。

## 范围修订

**本 Phase 包含**：

- 快捷栏面板与 10 个槽位的 RmlUi 渲染
- 物品图标 / 数量显示 / 激活槽高亮
- 左键激活槽位
- 右键使用物品
- Hover Tooltip
- 快捷栏内部拖拽换位
- 从快捷栏拖出解绑
- 拖拽预览视觉尽量保持与旧版一致（图标 + 数量，而不是整槽高亮框）
- `show()` / `hide()` / `toggle()` / 同步刷新等既有行为保持不变

**本 Phase 不包含**：

- `inventory ↔ hotbar` 跨 UI 拖拽

该能力依赖旧 `InventoryUI` 与新 `HotbarUI` 的跨框架命中测试与拖放桥接。若在本 Phase 强行保留，会引入一次性兼容层，复杂度高且会在 Phase 7（InventoryUI 迁移）再次删除。因此将其统一延后到 **Phase 7**，届时在双端都迁移到 RmlUi 后一次完成。

## 迁移策略

**保留** `game::ui::HotbarUI` 类名与主要公共接口，**替换其内部实现**：

- `HotbarUI` 改为 **独立类**，不再继承 `UIElement`
- `HotbarUI` 不再加入旧 `UIManager`
- `GameScene` 改为 `std::unique_ptr<HotbarUI>` 直接持有
- 内部改为 `RmlDataBridge + RmlUi document` 驱动
- 不再使用旧 `UIPanel` / `UIStackLayout` / `UIItemSlot` / `DragBehavior`
- 样式、尺寸、图片切片统一放在 `RML/RCSS`，避免在 C++ 硬编码 UI 表现参数

建议形态与 `TimeClockHud` / `DialogueBubbleView` / `ItemTooltipUI` 保持一致：

- C++ 负责：数据同步、事件桥接、拖拽状态机、命令派发、tooltip 协调
- RML/RCSS 负责：布局、尺寸、九宫格背景、图标层级、激活态视觉

## UI 文件

**新建**：

- `ui/rmlui/hud/hotbar.rml`
- `ui/rmlui/hud/hotbar.rcss`

**修改**：

- `ui/rmlui/theme/spritesheet.rcss`

### 布局要求

- 根文档 `body` 必须占满视口：`width: 100%; height: 100%;`
- 快捷栏底部居中布局
- 保持旧版视觉位置：距底部 `5dp`
- 10 个水平槽位
- 单槽尺寸 `32dp × 32dp`
- 槽位间距 `4dp`
- 面板 padding `8dp`

### 视觉资源要求

必须复用旧快捷栏资源，保持迁移前后显示内容一致。

#### 1. 快捷栏外框与槽位精灵

在 `spritesheet.rcss` 中新增 `ui-hotbar` 精灵表，并显式定义 outer + inner 精灵：

```rcss
@spritesheet ui-hotbar {
    src: ../../../assets/farm-rpg/UI/Inventory/Slots.png;

    hotbar-panel-bg:         6px 105px 164px 28px;
    hotbar-panel-bg-inner:   13px 112px 150px 15px;  /* L7 T7 R7 B6 */

    hotbar-slot-bg:          151px 38px 18px 18px;
    hotbar-slot-bg-inner:    153px 40px 14px 13px;   /* L2 T2 R2 B3 */

    hotbar-selected-bg:      119px 6px 18px 18px;
    hotbar-selected-bg-inner:123px 10px 10px 10px;   /* L4 T4 R4 B4 */
}
```

RML/RCSS 中应通过 `ninepatch(...)` 使用这些精灵，不在 C++ 中保留切片坐标。

#### 2. 物品图标精灵

`HotbarSlotViewModel::icon_decorator` 不应直接拼原始贴图路径 + 矩形；应基于 **预注册精灵名** 输出，例如：

- `image(item-tools-hoe)`
- `image(item-crops-strawberry)`

因此 Phase 6 需要在 `spritesheet.rcss` 中新增 `ui-items` 精灵表，将当前 demo 使用到的物品图标从 `assets/data/icon_config.json` 预注册进去。

当前项目图标量较小，**手工同步或脚本生成均可**；但计划必须明确：

- 至少覆盖 `item_config.json` 当前引用到的所有 item icon
- 命名采用稳定规则，例如 `item-<category>-<name>`
- 后续新增 item 时，需同步更新 `icon_config.json` 与 `spritesheet.rcss`

实现层还需要一份 **`icon_id(hash) -> sprite/decorator`** 的稳定映射。现有 `ItemCatalog` 仅保留 hash，不保留原始 icon key，因此 Phase 6 需新增以下二选一能力：

- 在 `ItemCatalog` 中保留原始 `icon_id` 字符串或提供查询接口
- 或新增专用 UI helper / registry，从配置生成 `icon_id(hash) -> decorator` 映射

目标是让 `HotbarUI` 能以数据驱动方式生成 `icon_decorator`，而不是写死 if/switch。

## Data Model 设计

使用 `data-for` 渲染 10 个槽位，建议注册 `HotbarSlotViewModel`：

| 字段 | 类型 | 用途 |
|------|------|------|
| `slot_index` | `int` | 槽位索引 |
| `icon_decorator` | `Rml::String` | 图标 decorator，空槽则为空 |
| `count_text` | `Rml::String` | 数量文本，数量 <= 1 可为空 |
| `has_item` | `bool` | 控制图标/数量显隐 |
| `is_active` | `bool` | 控制高亮态 |
| `is_bound` | `bool` | 是否绑定到 inventory slot |

建议根模型字段：

| 字段 | 类型 | 用途 |
|------|------|------|
| `hotbar_slots` | `std::vector<HotbarSlotViewModel>` | 10 个槽位列表 |

说明：

- 10 个槽位数量固定，但仍建议使用 `data-for`，以统一后续 InventoryUI 的列表渲染模式
- 图标层建议使用 `data-style-decorator="slot.icon_decorator"`
- 高亮 / 空槽 / 非绑定状态都通过 class / bool binding 控制，不在 C++ 直接改视觉属性
- **拖拽期间不要修改 `hotbar_slots` 数据模型**；拖拽结果应等系统派发 `HotbarChanged` / `HotbarSyncCommand` 后一次性刷新，避免 `data-for` 在拖拽中重建 DOM

## 事件与交互方案

### 1. 槽位点击 / 右键

**不要**使用 `contextmenu` 作为主方案。

建议采用：

- 槽位根元素绑定 `data-event-mousedown` 或 `data-event-mouseup`
- 通过 `BindEventCallback(...)` 接收槽位索引
- 在 C++ 回调中读取 `event.GetParameter("button", -1)`：
  - `0` → 左键激活 `HotbarActivateCommand`
  - `2` → 右键使用 `UseItemCommand`

要求：

- 迁移后删除旧的 `InputManager::onAction("secondary_action")` 连接/断开逻辑
- 右键点击发生在快捷栏区域内时，应在 RmlUi 侧完成消费，避免继续落到世界交互逻辑

### 2. Hover Tooltip

保持当前语义：

- hover 有物品的槽位时显示 tooltip
- hover 离开或开始拖拽时隐藏 tooltip
- tooltip 内容仍来自 `ItemCatalog`

### 3. 拖拽（两阶段事件流）

本 Phase 仅处理 **快捷栏内部拖拽** 与 **拖出解绑**。

#### DOM 结构建议

- 槽位根元素：作为 `dragdrop` 目标
- 槽位内部单独放一个 `slot-drag-proxy` 元素，承载 **图标 + 数量**
- 对 `slot-drag-proxy` 设置 `drag: clone`

原因：

- `drag: clone` 可直接复用 RmlUi 原生拖拽克隆能力
- 将可拖拽元素限制在“图标 + 数量”层，可让拖拽预览更接近旧版 `UIDragPreview`（而不是整槽克隆）
- 原槽位 DOM 在拖拽期间保持不变，不需要像旧 UI 那样清空再恢复

#### 事件流要求

- `dragstart`：触发在 **源元素**（`slot-drag-proxy`）
- `dragdrop`：触发在 **目标槽位**
- `dragend`：触发在 **源元素**

C++ 侧维护拖拽状态：

- `dragging_`
- `dragging_slot_`
- `dragging_inventory_slot_`
- `dragging_item_`
- `drop_handled_`

具体约定：

1. `dragstart(slot_index)`
   - 校验源槽位是否有绑定 item
   - 记录 `dragging_slot_` / `dragging_inventory_slot_`
   - `drop_handled_ = false`
   - 隐藏 tooltip
   - **不要修改 `hotbar_slots`**

2. `dragdrop(target_slot_index)`
   - 该事件发生在目标槽位上
   - 源槽位索引优先使用 C++ 侧已记录的 `dragging_slot_`
   - 如有需要，可用 `event.GetParameter<void*>("drag_element")` 读取源元素做一致性校验
   - 若目标合法：
     - 同槽位 drop → 仅标记 `drop_handled_ = true`
     - 不同槽位 drop → 派发 `HotbarBindCommand` / `HotbarUnbindCommand` 完成换位或重绑，并标记 `drop_handled_ = true`

3. `dragend(slot_index)`
   - 该事件始终回到源元素
   - 若 `drop_handled_ == false`，说明鼠标释放在快捷栏外，应执行 `HotbarUnbindCommand`
   - 无论成功与否，最后统一清理拖拽状态

### 4. 数据刷新约束

旧实现会在 drag start 时临时清空源槽位显示；RmlUi 方案中**不要沿用**。

原因：

- `data-for` 绑定列表在数据变化时可能重建节点
- 若拖拽过程中改写 `icon_decorator` / `has_item`，可能使正在拖拽的 DOM 元素失效

因此应采用：

- 拖拽开始后，原始 `hotbar_slots` 保持不变
- 视觉预览由 `drag: clone` 提供
- 真实数据只在命令执行完、`HotbarChanged` 到达后刷新

## C++ 实现要求

### `src/game/ui/hotbar_ui.h/cpp`

- `HotbarUI` 改为独立类，不再继承 `UIElement`
- 保留 gameplay-facing 公共接口：
  - `setSlotItem()`
  - `clearSlot()`
  - `clearAllSlots()`
  - `setSlotInventoryIndex()`
  - `resetInventoryMappings()`
  - `setActiveSlot()`
  - `setTarget()`
  - `setTooltipUI()`
  - `show()` / `hide()` / `toggle()`
- 不再保留旧 `setUIManager()` 路径
- 移除旧 `secondary_action` 全局输入监听
- 内部持有：
  - `engine::ui::rmlui::RmlUILayer&` 或等价引用
  - `engine::ui::rmlui::RmlDataBridge`
  - `Rml::ElementDocument*`
  - `owner_scene_id`
  - 必要的 event callback / listener 管理

### 文档加载与清理

`HotbarUI` 不是 `Scene`，因此不能调用 `Scene::loadRmlDocument()`；需由 `GameScene` 在构造或 `initUI()` 时传入：

- `RmlUILayer&`
- 当前 `instance_id_`

文档加载方式：

- `layer.loadDocument("ui/rmlui/hud/hotbar.rml", instance_id_)`

清理要求：

- 事件监听器 / event callback 必须先解绑
- 再卸载文档或清空文档引用
- 再销毁 `data_bridge_`

**推荐顺序**：

- `GameScene::clean()` 中先 `hotbar_ui_.reset()`
- `HotbarUI` 析构/clean 内先移除事件
- 如 `document_` 仍有效，可直接 `layer.unloadDocument(document_)`，然后置空
- 最后销毁 `data_bridge_`
- 随后 `Scene::clean()` 再执行 owner 级 `unloadAllRmlDocuments()`；若文档已被直接卸载，此步应自然 no-op

这样既满足“监听器先于文档销毁移除”的要求，也兼容初始化失败 / 提前析构路径。

### `src/game/scene/game_scene.h/cpp`

- `hotbar_ui_` 改为 `std::unique_ptr<game::ui::HotbarUI>` 直接持有
- `HotbarUI` 不再加入旧 `UIManager`
- 初始化时通过 `RmlUILayer` 创建 hotbar 文档，并传入当前 `instance_id_` 作为 owner
- 继续保留现有：
  - `setTarget(...)`
  - `setTooltipUI(...)`
  - `toggle()`
  - `HotbarSyncCommand` 响应
  - `HotbarSlotChanged` 响应
- `GameScene::clean()` 中确保 `hotbar_ui_.reset()` 发生在 `Scene::clean()` 之前

### 兼容边界

由于本 Phase 不做跨 UI 拖拽，以下旧依赖允许暂时保留到 Phase 7：

- `InventoryUI::setHotbarUI(...)`
- `HotbarUI::setInventoryUI(...)`

但在 Phase 6 实现中，这些接口最多只保留 **兼容壳 / no-op**，不再承担实际跨拖拽逻辑。

如为保持编译稳定需要，允许对 `InventoryUI` 做**最小兼容修改**，例如：

- 禁用旧的 hotbar 目标识别分支
- 让 `inventory → hotbar` 旧互拖路径在 Phase 6 直接返回未处理

真正的 `inventory ↔ hotbar` 拖放重建放到 Phase 7 一次完成。

## 涉及文件

| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/hotbar.rml` |
| 新建 | `ui/rmlui/hud/hotbar.rcss` |
| 修改 | `ui/rmlui/theme/spritesheet.rcss` |
| 修改 | `src/game/ui/hotbar_ui.h` |
| 修改 | `src/game/ui/hotbar_ui.cpp` |
| 修改 | `src/game/scene/game_scene.h` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 视实现方案修改 | `src/game/data/item_catalog.h` |
| 视实现方案修改 | `src/game/data/item_catalog.cpp` |
| 如需兼容胶水修改 | `src/game/ui/inventory_ui.h` |
| 如需兼容胶水修改 | `src/game/ui/inventory_ui.cpp` |

**本 Phase 不删除**：

- `src/game/ui/hotbar_ui.h/cpp`

它们将被保留为 RmlUi wrapper 容器，而不是直接删除。

## 验证标准

### 必须通过

- 10 槽显示正确
- 底部居中位置与旧版一致
- 面板 / 槽位 / 选中态图片显示与旧版一致
- 图标 / 数量 / 高亮显示正确
- 左键点击可激活槽位
- 右键使用物品正常，且不会误触发世界右键逻辑
- hover tooltip 正常显示 / 隐藏
- 快捷栏内部拖拽换位正常
- 将物品从快捷栏拖出后可解绑
- 拖拽预览视觉接近旧版（优先图标 + 数量，而非整槽背景）
- 快捷栏开关与同步刷新正常

### 本 Phase 不验收

- `inventory ↔ hotbar` 跨 UI 拖拽

该项移至 Phase 7 验收。

---
