# Phase 2: 物品菜单场景实现 — 详细计划

## 可行性分析

Phase 2 方案完全可行，与 PauseMenuScene 模式高度一致。需做如下修正：

1. **InventoryMenuUI 不需要单独的 C++ 控制器类** — 数据绑定和事件处理直接在 InventoryMenuScene 内完成即可（与 PauseMenuScene 一致）。仅当 UI 逻辑复杂到需要拆分时再抽取。
2. **Tooltip 直接复用 ItemTooltipUI** — 它已经是独立组件，接受 owner_scene_id，可以在新场景中直接创建。
3. **操作子菜单推迟到 Phase 3** — 先做核心功能（场景、网格、导航、Tooltip），子菜单作为独立步骤后补。这样可以尽早验证整体流程。
4. **装备槽和角色信息区推迟** — Phase 2 聚焦背包网格 + 标签页占位，角色区作为静态占位，Phase 3 实装。

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/game/scene/inventory_menu_scene.h` | 场景头文件 |
| `src/game/scene/inventory_menu_scene.cpp` | 场景实现 |
| `ui/rmlui/scenes/inventory_menu.rml` | 菜单 RML 标记 |
| `ui/rmlui/scenes/inventory_menu.rcss` | 菜单样式 |

## 实现思路

遵循 PauseMenuScene 的成熟模式：
- Scene 自身持有数据绑定变量（slot ViewModel 数组）和事件回调
- RmlDataBridge 绑定到 RML `data-model`，通过 `data-for` 循环渲染 40 slot
- RmlEventBridge 处理 slot 点击、标签页切换等交互
- GameScene::onInventoryToggle() 改为 pushScene，不再 toggle overlay
- 打开菜单时从 player 的 InventoryComponent 读取数据填充 ViewModel
- 监听 InventoryChanged 事件以支持实时同步（如 Lua 脚本修改背包）

## 实现步骤

### Step 1: 新建 InventoryMenuScene 骨架

创建 `inventory_menu_scene.h/cpp`，仿照 PauseMenuScene：
- 构造参数: `(name, context, player_entity, item_catalog)`
- `init()`: 保存 previous_state → setState(Paused) → pushContext(Menu) → initUI()
- `clean()`: removeEventListeners → popContext → setState(previous) → unloadAllRmlDocuments → destroyBridge
- `update()`: dirtyCheck → data_bridge_.DirtyAllVariables() 刷新 UI

### Step 2: 新建 RML/RCSS 菜单文件

`inventory_menu.rml`: 引用 theme 公共样式，`data-model="inventory_menu"`
- 全屏 overlay 遮罩
- 居中面板 440×310dp（ninepatch `menu-panel-bg`）
- 标签页行: 5 个 16×16 图标按钮（仅 Inventory 可点击，其余 disabled）
- 背包网格: `data-for` 循环 40 slot，每 slot 20×20dp
- 角色信息区: 静态占位（头像 + 文字 + 装备槽锁定 + 金币）
- 垃圾桶按钮

`inventory_menu.rcss`: 新增 spritesheet 定义（引用 sprites.md 中的坐标），布局样式

### Step 3: 数据绑定 — Slot ViewModel

在 Scene 中定义：
```
struct SlotVM { int index; String icon_decorator; String count_text; bool has_item; bool has_count; };
Vector<SlotVM> backpack_slots_;  // 40 个
```
- `initUI()` 中创建 data model，Bind backpack_slots_
- 提供 `syncFromInventory()` 方法：读取 player InventoryComponent → 填充 ViewModel
- `init()` 末尾调用一次全量同步

### Step 4: GameScene 集成

- `onInventoryToggle()` 改为创建 InventoryMenuScene 并 pushScene
- 需要传入 player entity 和 item_catalog 指针
- 移除旧的 inventory UI toggle 逻辑（旧 InventoryUI 暂不删除，Phase 3 清理）

### Step 5: 键盘/手柄网格导航

RML 中每个 slot 元素设置 `tab-index: auto` + `nav-up/down/left/right: auto`
- RmlUi 的 auto nav 会自动按布局计算最近邻
- 10×4 flex-wrap 网格下，auto 导航天然支持上下左右
- 菜单打开后 queueFocusElementById 聚焦第一个 slot
- 监听 `menu_cancel` action → requestPopScene()

### Step 6: Tooltip 集成

- 在 Scene 中创建 ItemTooltipUI（传入 instanceId()）
- Slot hover/focus 事件 → 查 ItemCatalog → showItem(name, category, desc)
- Slot 失焦 → hideTooltip()
- `update()` 中调用 tooltip_.update(dt) 刷新位置

### Step 7: 标签页 UI

- 5 个标签按钮，初期只有 Inventory (index 0) 为 active
- 其余标签 disabled（opacity 0.5，不可点击）
- active 标签底部高亮或 box-shadow
- Q/E 快捷键切换标签（预留，目前只有一个可用）

## 待办

- [ ] Step 1: InventoryMenuScene 骨架 (h/cpp)
- [ ] Step 2: inventory_menu.rml/rcss (布局 + spritesheet)
- [ ] Step 3: Slot ViewModel 数据绑定 + syncFromInventory
- [ ] Step 4: GameScene 集成 (pushScene 替代 toggle)
- [ ] Step 5: 键盘/手柄网格导航
- [ ] Step 6: Tooltip 集成
- [ ] Step 7: 标签页 UI
- [ ] 更新 CMakeLists.txt
- [ ] 构建验证 + 运行测试
