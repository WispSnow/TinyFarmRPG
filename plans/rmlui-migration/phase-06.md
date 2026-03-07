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
- `show()` / `hide()` / `toggle()` / 同步刷新等既有行为保持不变

**本 Phase 不包含**：

- `inventory ↔ hotbar` 跨 UI 拖拽

该能力依赖旧 `InventoryUI` 与新 `HotbarUI` 的跨框架命中测试与拖放桥接。若在本 Phase 强行保留，会引入一次性兼容层，复杂度高且会在 Phase 7（InventoryUI 迁移）再次删除。因此将其统一延后到 **Phase 7**，届时在双端都迁移到 RmlUi 后一次完成。

## 迁移策略

**保留** `game::ui::HotbarUI` 类名与外部公共接口，**替换其内部实现**：

- 继续保留现有对外方法，避免 `GameScene` / 其他调用点发生不必要扩散修改
- 内部改为 `RmlDataBridge + RmlUi document` 驱动
- 不再使用旧 `UIPanel` / `UIStackLayout` / `UIItemSlot` / `DragBehavior`
- 样式、尺寸、图片切片统一放在 `RML/RCSS`，避免在 C++ 硬编码 UI 表现参数

建议形态与 `TimeClockHud` / `DialogueBubbleView` / `ItemTooltipUI` 保持一致：

- C++ 负责：数据同步、事件桥接、拖拽状态机、命令派发
- RML/RCSS 负责：布局、尺寸、九宫格背景、图标层级、激活态视觉

## UI 文件

**新建**：

- `ui/rmlui/hud/hotbar.rml`
- `ui/rmlui/hud/hotbar.rcss`

### 布局要求

- 底部居中布局
- 保持旧版视觉位置：距底部 `5dp`
- 10 个水平槽位
- 单槽尺寸 `32dp × 32dp`
- 槽位间距 `4dp`
- 面板 padding `8dp`

### 视觉资源要求

必须复用旧快捷栏资源，保持迁移前后显示内容一致：

- panel background：`assets/farm-rpg/UI/Inventory/Slots.png` 区域 `[6, 105, 164, 28]`
  - nine-slice：`left=7 top=7 right=7 bottom=6`
- slot background：`assets/farm-rpg/UI/Inventory/Slots.png` 区域 `[151, 38, 18, 18]`
  - nine-slice：`left=2 top=2 right=2 bottom=3`
- selected background：`assets/farm-rpg/UI/Inventory/Slots.png` 区域 `[119, 6, 18, 18]`
  - nine-slice：`left=4 top=4 right=4 bottom=4`

如可行，优先在 `spritesheet.rcss` / `RCSS decorator` 层表达这些切片，不把图像区域信息散落到 C++。

## Data Model 设计

使用 `data-for` 渲染 10 个槽位，建议注册 `HotbarSlotViewModel`：

| 字段 | 类型 | 用途 |
|------|------|------|
| `slot_index` | `int` | 槽位索引 |
| `icon_decorator` | `Rml::String` | 图标装饰器，空槽则为空 |
| `count_text` | `Rml::String` | 数量文本，数量 <= 1 可为空 |
| `has_item` | `bool` | 控制图标/数量显隐 |
| `is_active` | `bool` | 控制高亮态 |
| `is_bound` | `bool` | 是否绑定到 inventory slot |

建议根模型字段：

| 字段 | 类型 | 用途 |
|------|------|------|
| `hotbar_slots` | `std::vector<HotbarSlotViewModel>` | 10 个槽位列表 |
| `visible` | `bool` | 控制文档整体显示/隐藏（如需要） |

说明：

- 10 个槽位数量固定，但仍建议使用 `data-for`，以统一后续 InventoryUI 的列表渲染模式
- `icon_decorator` 建议由 C++ 输出 `image(...)` 或等价 decorator 字符串，RCSS 只负责图层表现
- 高亮 / 空槽 / 非绑定状态都通过 class/data binding 控制，不在 C++ 直接改视觉属性

## 事件与交互方案

### 1. 槽位点击 / 右键

**不要**使用 `contextmenu` 作为主方案。

原因：当前项目内置的 RmlUi 6.2 事件表中有 `dragstart / dragdrop / dragend`，但没有可直接依赖的 `contextmenu` 迁移范式。

建议采用：

- `data-event-mousedown` 或 `data-event-mouseup`
- 通过 `BindEventCallback(...)` 接收槽位索引
- 在 C++ 回调中读取 `event.GetParameter("button", -1)`：
  - `0` → 左键激活 `HotbarActivateCommand`
  - `2` → 右键使用 `UseItemCommand`

这样能与旧逻辑保持一致，也能明确吞掉发生在快捷栏区域内的右键输入。

### 2. 拖拽

本 Phase 仅处理 **快捷栏内部拖拽** 与 **拖出解绑**。

建议使用：

- 槽位元素设置 `drag: drag-drop`
- 通过 `BindEventCallback(...)` 处理：
  - `dragstart(slot_index)`
  - `dragdrop(slot_index)`
  - `dragend(slot_index)`

C++ 侧维护临时拖拽状态：

- `dragging_`
- `dragging_slot_`
- `dragging_inventory_slot_`
- `dragging_item_`
- 是否已完成 drop

行为映射：

- 拖到另一个 hotbar 槽位：
  - 交换 / 重绑，保持当前 `HotbarBindCommand` / `HotbarUnbindCommand` 语义不变
- 拖回原槽位：
  - 还原显示
- 拖到快捷栏外：
  - 触发 `HotbarUnbindCommand`

### 3. Hover Tooltip

保持当前语义：

- hover 有物品的槽位时显示 tooltip
- hover 离开或开始拖拽时隐藏 tooltip
- tooltip 内容仍来自 `ItemCatalog`

## C++ 实现要求

### `src/game/ui/hotbar_ui.h/cpp`

- 保留 `HotbarUI` 类及其对外 API
- 内部改为：
  - `RmlDataBridge`
  - `Rml::ElementDocument*`
  - 必要的 event callback / listener 管理
- 清理顺序遵循现有 RmlUi 规范：
  - 解绑事件
  - 卸载文档
  - 销毁 data bridge
- 若析构路径与 `clean()` 并存，需保证双路径清理幂等

### `src/game/scene/game_scene.cpp/h`

- `HotbarUI` 不再加入旧 `UIManager`
- 改为由 `GameScene` 直接持有 / 管理其生命周期
- 初始化时通过 `RmlUILayer` 加载 hotbar 文档，并传入当前 `instance_id_` 作为 owner
- 继续保留现有：
  - `setTarget(...)`
  - `setTooltipUI(...)`
  - `toggle()`
  - `HotbarSyncCommand` 响应
  - `HotbarSlotChanged` 响应

### 兼容边界

由于本 Phase 不做跨 UI 拖拽，以下旧依赖允许暂时保留到 Phase 7：

- `InventoryUI::setHotbarUI(...)`
- `HotbarUI::setInventoryUI(...)`

但在 Phase 6 实现中，这些接口可仅保留兼容壳，不再承担实际跨拖拽逻辑。真正的 `inventory ↔ hotbar` 拖放重建放到 Phase 7 一次完成。

## 涉及文件

| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/hotbar.rml` |
| 新建 | `ui/rmlui/hud/hotbar.rcss` |
| 修改 | `src/game/ui/hotbar_ui.h` |
| 修改 | `src/game/ui/hotbar_ui.cpp` |
| 修改 | `src/game/scene/game_scene.h` |
| 修改 | `src/game/scene/game_scene.cpp` |

**本 Phase 不删除**：

- `src/game/ui/hotbar_ui.h/cpp`

它们将被保留为 RmlUi wrapper 容器，而不是直接删除。

## 验证标准

### 必须通过

- 10 槽显示正确
- 底部居中位置与旧版一致
- 图标 / 数量 / 高亮显示正确
- 左键点击可激活槽位
- 右键使用物品正常
- hover tooltip 正常显示 / 隐藏
- 快捷栏内部拖拽换位正常
- 将物品从快捷栏拖出后可解绑
- 快捷栏开关与同步刷新正常

### 本 Phase 不验收

- `inventory ↔ hotbar` 跨 UI 拖拽

该项移至 Phase 7 验收。

---
