# InventoryMenuScene 拆分重构计划

## 元信息
- 任务ID：`UI-INVMENU-001`
- 任务标题：`InventoryMenuScene 拆分为 Tab 管理器 + Tab Content 组件`
- 优先级：`P1`
- 状态：`Pending`
- 计划时间：`2026-04-04` 起
- 依赖任务：`无`
- 设计原则：`Scene 只做文档/生命周期/标签切换壳；每个标签页内容由独立组件承担，各 Tab 之间零耦合；充分利用 RmlUi 声明式绑定管理 tab 状态；资源释放遵循 RAII`

## 背景

`InventoryMenuScene` 当前 .cpp ~1190 行、.h ~170 行、17 个成员变量、~35 个私有方法——仅覆盖了第一个标签页（Inventory）。后续还要补充 Equipment / Quests / Map / Options 四个标签页（见 `inventory_menu.rml:19-21`），如果继续在同一个类里增长，预计将膨胀到 3000+ 行。

核心问题：
1. **Scene 壳与 Tab 内容混在一起**——文档加载、input context 管理、game state 切换等 Scene 级职责与 slot 同步、拖拽、action menu、detail panel 等 Inventory Tab 专属逻辑全部耦合在一个类中。
2. **Backpack / Hotbar 事件回调高度重复**——16 个 handler（bp 8 + hb 8）模式几乎一致，差异仅在操作的数据源不同。
3. **选中状态分散**——`detail_bp_slot_` / `detail_hb_slot_` + 各 `SlotGridViewModel::is_selected` 多处手动同步，容易遗漏。
4. **RCSS 样式重复**——`.bp-slot-icon` / `.hb-slot-icon`、`.bp-slot-count` / `.hb-slot-count` 规则完全相同，仅类名不同。
5. **Tab 状态未声明化**——tab bar 是静态 HTML，高亮和内容可见性完全由 C++ 控制；RmlUi 的 `data-class-*` / `data-if` 本可以承担这部分工作。
6. **缺少 `tf-nav-root`**——`inventory_menu.rml` 的 `<body>` 缺少 `tf-nav-root` class，导致 RmlUi 原生导航根未启用。对照 `pause_menu.rml`、`save_slot_select.rml`、`title.rml` 均有此 class。
7. **角色面板归属不明**——`syncCharacterPanel()` 及 `char_*` 绑定变量当前在 Scene 中，但逻辑上被当作 Inventory Tab 私有内容。Equipment Tab 同样需要更新角色/装备信息，应明确为 Scene 级共享面板。

## 重构目标

| 目标 | 衡量标准 |
|------|----------|
| Scene 薄壳化 | `InventoryMenuScene` 不包含任何 Tab 内容逻辑，仅管理文档生命周期、tab 切换与共享面板 |
| 多 Tab 架子一次到位 | `TabId` 枚举 + `tabs_` 容器 + `active_tab_id_` 一步搭好，即使当前只有 Inventory 一个实现 |
| Tab 内容组件化 | 每个标签页实现统一接口，可独立开发/测试 |
| Tab 状态声明化 | tab 高亮、内容区可见性通过 `active_tab_id` 绑定变量 + `data-class-*` / `data-if` 管理 |
| 角色面板为 Scene 共享 | `char_*` / equip grid 绑定与同步由 Scene 壳负责，各 Tab 可通知 Scene 刷新 |
| 消除 bp/hb 回调重复 | 16 个 handler 缩减为 8 个参数化方法 |
| 统一选中模型 | 单一 `SelectedSlot` 值对象替代分散状态 |
| RCSS 去重 | bp/hb slot icon & count 共用一套几何规则 |
| RAII 生命周期 | Tab 资源释放由析构函数承担，接口不含 `cleanup()` |

## 架构设计

### TabId 枚举

```cpp
// game/ui/menu_tab_id.h
namespace game::ui {

enum class MenuTabId {
    Inventory,
    Equipment,
    Quests,
    Map,
    Options,
};

} // namespace game::ui
```

### Tab Content 接口

```
game/ui/menu_tab_content.h
```

```cpp
namespace game::ui {

/// 菜单标签页内容的统一接口。
/// Scene 负责文档和数据模型的生命周期，Tab 内容组件只关心自身的数据绑定和交互逻辑。
/// 资源释放由析构函数负责（RAII），接口不设 cleanup()。
class IMenuTabContent {
public:
    virtual ~IMenuTabContent() = default;

    /// 在 Scene 创建 DataModelConstructor 后调用，注册自身的数据绑定与事件回调。
    /// @return false 表示绑定失败，Scene 应中止初始化。
    [[nodiscard]] virtual bool bindModel(Rml::DataModelConstructor& constructor) = 0;

    /// tab 切到前台时调用，连接 dispatcher、执行初始数据同步、标记 dirty。
    virtual void onActivated() = 0;

    /// tab 切走时调用，断开 dispatcher、清理临时 UI 状态（tooltip、action menu 等）。
    /// 注意：这是行为状态切换，不是资源释放。
    virtual void onDeactivated() = 0;

    /// 每帧更新（仅活跃 tab 被调用）。
    virtual void update(float delta_time) = 0;

    /// 处理 cancel/ESC 操作。返回 true 表示 tab 内部消费了（如关闭 action menu），
    /// false 表示 tab 无事可取消，Scene 应执行 pop。
    [[nodiscard]] virtual bool onCancel() = 0;
};

} // namespace game::ui
```

**与初版计划的差异**：移除了 `cleanup()` 方法。Tab Content 的资源释放（`tooltip_ui_` 等）由析构函数自动完成。Scene 切走 tab 时调用 `onDeactivated()` 清理行为状态，销毁时直接 reset `unique_ptr` 触发析构。这避免了"切换状态"和"资源释放"变成两套协议的问题。

### SelectedSlot 值对象

```
game/ui/slot_grid_support.h（扩展）
```

```cpp
/// 标识菜单中可交互面板的种类（选中、焦点语境）。
/// 与 SlotGridDragSourceKind 分开定义——选中和拖拽是不同概念，
/// 未来 Equipment 等面板可选中但不可拖拽。
enum class MenuPanelKind {
    None = 0,
    Backpack,
    Hotbar,
    // 未来：Equipment, QuestList, ...
};

/// 统一记录当前选中的槽位：面板种类 + 索引。
struct SelectedSlot {
    MenuPanelKind panel{MenuPanelKind::None};
    int index{-1};

    [[nodiscard]] bool valid() const {
        return panel != MenuPanelKind::None && index >= 0;
    }
    [[nodiscard]] bool isBackpack() const { return panel == MenuPanelKind::Backpack; }
    [[nodiscard]] bool isHotbar() const { return panel == MenuPanelKind::Hotbar; }
    void clear() { panel = MenuPanelKind::None; index = -1; }
};
```

**与初版计划的差异**：不再复用 `SlotGridDragSourceKind`，改为独立的 `MenuPanelKind` 枚举，避免选中与拖拽的语义泄漏。

### 重构后的类职责划分

```
InventoryMenuScene (Scene 薄壳，~200 行)
├── 文档加载/卸载、input context push/pop、game state 切换
├── 共享数据类型注册（SlotGridViewModel 等跨 Tab 类型）
├── active_tab_id_ 绑定变量 + tab bar 切换事件 → RmlUi data-class / data-if 驱动
├── 角色共享面板（char_name, char_title, gold_label, equip grid）绑定与同步
├── cancel 事件分发：先问 active tab，再决定 pop
└── std::unordered_map<MenuTabId, std::unique_ptr<IMenuTabContent>> tabs_

InventoryTabContent (IMenuTabContent 实现，~750 行)
├── backpack_slots_, hotbar_slots_, action_menu_entries_ 绑定
├── tooltip, detail panel, selection (SelectedSlot)
├── 拖拽处理
├── action menu
├── 事件回调（参数化为 MenuPanelKind，8 个方法）
└── 响应 InventoryChanged / HotbarChanged 事件

（未来：EquipmentTabContent, QuestsTabContent, ...）
```

### Tab 状态声明化（RML 绑定）

Scene 壳维护一个 `active_tab_id` 绑定变量（int），RML 用数据绑定控制 tab chrome：

```xml
<!-- tab bar：高亮由 data-class 驱动 -->
<button class="tab-icon tf-nav-auto tf-focus-ring-gold" id="tab-inventory"
        data-class-tab-active="active_tab_id == 0"
        data-event-click="switch_tab(0)"></button>
<button class="tab-icon tf-nav-auto tf-focus-ring-gold" id="tab-equipment"
        data-class-tab-active="active_tab_id == 1"
        data-event-click="switch_tab(1)"></button>
<!-- ... -->

<!-- 内容区：可见性由 data-if 驱动 -->
<div id="inventory-col" data-if="active_tab_id == 0">
    <!-- 背包/快捷栏 slot grid -->
</div>
<div id="equipment-col" data-if="active_tab_id == 1">
    <!-- 装备内容（未来） -->
</div>
```

C++ 侧 Scene 壳只需：
```cpp
constructor.Bind("active_tab_id", &active_tab_id_);
constructor.BindEventCallback("switch_tab", [this](...) { switchTab(arg); });
```

这样 tab 高亮和内容可见性完全由 RmlUi 声明式驱动，不需要 C++ 手动操作 DOM class。

### 角色共享面板

角色面板（portrait、name、title、gold、equip grid）由 Scene 壳拥有和绑定：

```cpp
// InventoryMenuScene 成员
Rml::String char_name_{"Player"};
Rml::String char_title_{"Lv.1 Farmer"};
Rml::String gold_label_{"Gold: --"};
Rml::String farm_label_{"TinyFarm"};
```

各 Tab 可通过回调/事件通知 Scene 刷新角色面板（例如 Equipment Tab 装备变化后通知更新）。RML 中角色栏位于 `#char-col`，不受 `data-if="active_tab_id == N"` 控制，始终可见。

### Slot 事件回调参数化

将 16 个 `onBpSlot*` / `onHbSlot*` 合并为 8 个参数化方法：

```cpp
void onSlotFocus(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotMouseDown(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotMouseUp(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotHoverEnter(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotHoverExit(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotDragStart(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotDragDrop(MenuPanelKind kind, int index, Rml::Event& event);
void onSlotDragEnd(MenuPanelKind kind, int index, Rml::Event& event);
```

绑定时用 lambda 捕获 `MenuPanelKind`，采用方案 A（不修改 `SlotGridEventHandlers` 模板）：

```cpp
// 在 InventoryTabContent::bindModel 中，直接向 constructor 注册 lambda
auto bind_grid_events = [&](std::string_view prefix, MenuPanelKind kind) {
    const auto make_name = [&](std::string_view suffix) {
        std::string name{prefix};
        name += suffix;
        return name;
    };
    const auto bind = [&](std::string_view suffix, auto method) {
        return constructor.BindEventCallback(
            Rml::String{make_name(suffix)},
            [this, kind, method](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList& args) {
                (this->*method)(kind, getSingleIntArgument(args), e);
            });
    };
    return bind("_focus",       &InventoryTabContent::onSlotFocus)
        && bind("_mouse_down",  &InventoryTabContent::onSlotMouseDown)
        && bind("_mouse_up",    &InventoryTabContent::onSlotMouseUp)
        && bind("_hover_enter", &InventoryTabContent::onSlotHoverEnter)
        && bind("_hover_exit",  &InventoryTabContent::onSlotHoverExit)
        && bind("_drag_start",  &InventoryTabContent::onSlotDragStart)
        && bind("_drag_drop",   &InventoryTabContent::onSlotDragDrop)
        && bind("_drag_end",    &InventoryTabContent::onSlotDragEnd);
};

bind_grid_events("bp_slot", MenuPanelKind::Backpack);
bind_grid_events("hb_slot", MenuPanelKind::Hotbar);
```

这样无需修改 `SlotGridEventHandlers` 模板，对其他使用方零影响。

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/game/ui/menu_tab_content.h` | `IMenuTabContent` 接口 + `MenuTabId` 枚举 |
| `src/game/ui/inventory_tab_content.h` | `InventoryTabContent` 类头文件 |
| `src/game/ui/inventory_tab_content.cpp` | `InventoryTabContent` 类实现 |

## 修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/game/scene/inventory_menu_scene.h` | 移除所有 Tab 内容成员和方法；新增 `TabId` / `tabs_` / `active_tab_id_` / tab 切换；保留角色面板绑定变量 |
| `src/game/scene/inventory_menu_scene.cpp` | 移除所有 Tab 内容实现（~1000 行）；保留 Scene 壳（~200 行）；`initUI()` 创建 tab + 绑定 `active_tab_id` + `switch_tab` 事件 |
| `src/game/ui/slot_grid_support.h` | 新增 `MenuPanelKind` 枚举和 `SelectedSlot` 结构体 |
| `ui/rmlui/scenes/inventory_menu.rml` | `<body>` 添加 `tf-nav-root`；tab bar 改用 `data-class-tab-active` + `data-event-click="switch_tab(N)"`；inventory 内容区包裹 `data-if="active_tab_id == 0"`；RCSS 共享类名替换 |
| `ui/rmlui/scenes/inventory_menu.rcss` | 新增 `.inv-slot-icon` / `.inv-slot-count` 共享几何类；删除重复的 `.bp-slot-icon` / `.hb-slot-icon` / `.bp-slot-count` / `.hb-slot-count`；移除静态 `.tab-active` 规则（改由 data-class 驱动） |
| `src/CMakeLists.txt` | 新增 `game/ui/inventory_tab_content.cpp` 到构建 |
| `tests/game/inventory_menu_scene_slot_grid_registration_test.cpp` | 适配新结构 |

## 执行步骤

### Stage 0：前置准备（不改变运行时行为）

1. 在 `slot_grid_support.h` 中新增 `MenuPanelKind` 枚举和 `SelectedSlot` 结构体
2. 创建 `menu_tab_content.h`，定义 `MenuTabId` 枚举和 `IMenuTabContent` 接口（无 `cleanup()`）
3. `inventory_menu.rml` 的 `<body>` 从 `class="tf-screen-root"` 改为 `class="tf-screen-root tf-nav-root"`
4. 更新 `CMakeLists.txt`（暂无新 .cpp）
5. 编译验证

### Stage 1：Tab 状态声明化 + 角色面板归属调整

此阶段仍在 `InventoryMenuScene` 内部改造，不创建新类，确保每步可独立验证。

1. `inventory_menu_scene.h` 新增 `active_tab_id_`（int，默认 0）绑定变量
2. `initUI()` 中绑定 `active_tab_id` 和 `switch_tab` 事件回调
3. `inventory_menu.rml` tab bar 改为声明式：
   - 各 tab button 添加 `data-class-tab-active="active_tab_id == N"`
   - 移除静态 `tab-active` class
   - 已启用的 tab 添加 `data-event-click="switch_tab(N)"`
   - disabled tab 暂保留 `disabled` 属性
4. `inventory_menu.rcss` 中保留 `.tab-active` 样式规则（类名不变，只是改为由 data-class 动态添加）
5. 确认角色面板（`char_*`、`gold_*`、`farm_*`）绑定变量和 `syncCharacterPanel()` 留在 Scene 中，不迁移到 Tab Content
6. 编译验证，确认 tab 高亮与当前行为一致

### Stage 2：提取 InventoryTabContent + 多 Tab 架子

1. 创建 `inventory_tab_content.h`，声明类：
   - 构造函数接收 `Context&`、`RmlDocumentController&`、`entt::registry&`、`entt::entity player`、`ItemCatalog*`
   - 持有从 `InventoryMenuScene` 迁移出的所有 Tab 内容成员变量（不含角色面板）
   - 声明 `IMenuTabContent` 的 4 个虚方法（`bindModel`、`onActivated`、`onDeactivated`、`update`、`onCancel`）+ 内部私有方法
   - 析构函数负责释放 `tooltip_ui_` 等资源

2. 创建 `inventory_tab_content.cpp`，从 `inventory_menu_scene.cpp` **搬移**以下实现：
   - `syncFromInventory` / `syncHotbarFromInventory` / `refreshSlot`
   - `markSlotsDirty` / `markActionMenuDirty`
   - `showTooltipFor*` / `clearTooltip`
   - `updateDetailFor*` / `clearDetail`
   - `selectBpSlot` / `selectHbSlot` / `clearSelection` / `clearSelectionAndDetail`
   - `clearDragState`
   - `closeActionMenu` / `openBackpackActionMenu` / `openHotbarActionMenu` / `openDiscardConfirmForBackpackSlot`
   - `showActionMenu` / `positionActionMenuForGridSlot` / `findIndexedChildElement` / `measureGridHorizontalGap`
   - `executeAction`
   - `onTrashClicked` / `onSortClicked`
   - `onInventoryChanged` / `onHotbarChanged`
   - 所有 16 个 `onBpSlot*` / `onHbSlot*` 回调（此阶段先原样搬移，Stage 3 再参数化）
   - `onActionEntryClick`
   - `ActionEntryViewModel` 结构体移入 `inventory_tab_content.h`

3. `InventoryTabContent::bindModel` 实现：
   - 注册 `ActionEntryViewModel` 类型（`SlotGridViewModel` 类型由 Scene 壳注册，跨 Tab 共享）
   - 绑定 `backpack_slots_`、`hotbar_slots_`、`action_menu_entries_`、detail 变量
   - 绑定所有事件回调（trash、sort、slot grid、action entry）

4. `InventoryTabContent::onActivated`：
   - 连接 `InventoryChanged` / `HotbarChanged` dispatcher
   - 调用 `syncFromInventory` / `syncHotbarFromInventory`
   - 标记全部 dirty

5. `InventoryTabContent::onDeactivated`：
   - 断开 dispatcher
   - `closeActionMenu(false)` / `clearTooltip` / `clearSelectionAndDetail`

6. `InventoryTabContent::onCancel`：
   - 如果 action menu 可见，关闭并返回 true
   - 否则返回 false

7. `InventoryTabContent::~InventoryTabContent()`：
   - 断开 dispatcher（防御性，正常路径 `onDeactivated` 已断开）
   - `tooltip_ui_` 由 unique_ptr 自动释放

8. 改造 `InventoryMenuScene`：
   - 从 `.h` 移除所有已迁移的成员变量和方法
   - 新增：
     ```cpp
     std::unordered_map<game::ui::MenuTabId, std::unique_ptr<game::ui::IMenuTabContent>> tabs_;
     game::ui::MenuTabId active_tab_id_{game::ui::MenuTabId::Inventory};
     int active_tab_id_bind_{0};  // 供 RmlUi 绑定的 int 镜像
     ```
   - `initUI()` 改为：创建 data model → 注册共享类型 → 绑定 `active_tab_id` / `switch_tab` / 角色面板变量 → 创建 `InventoryTabContent` 并插入 `tabs_` → 调用 `bindModel` → 加载文档 → `syncCharacterPanel()` → `tabs_[active].onActivated()`
   - `update()` 改为：`tabs_[active]->update(delta_time)`
   - `clean()` / 析构：`tabs_[active]->onDeactivated()` → `tabs_.clear()`（触发各 tab 析构）→ 恢复 input context / game state
   - `onMenuCancelPressed` 改为：
     ```cpp
     if (auto it = tabs_.find(active_tab_id_); it != tabs_.end() && it->second->onCancel())
         return true;
     requestPopScene();
     return true;
     ```
   - `switchTab(MenuTabId new_tab)` 实现：
     ```cpp
     if (new_tab == active_tab_id_) return;
     tabs_[active_tab_id_]->onDeactivated();
     active_tab_id_ = new_tab;
     active_tab_id_bind_ = static_cast<int>(new_tab);
     document_controller_.markDirty("active_tab_id");
     tabs_[active_tab_id_]->onActivated();
     ```

9. `inventory_menu.rml` 中 inventory 内容区包裹 `data-if="active_tab_id == 0"`（为未来 tab 切换预留）

10. 更新 `CMakeLists.txt` 添加 `inventory_tab_content.cpp`

11. 编译验证，全量测试

### Stage 3：Backpack / Hotbar 回调参数化 + SelectedSlot

1. 在 `InventoryTabContent` 中新增 8 个 `onSlot*(MenuPanelKind, int, Event&)` 参数化方法

2. 将原 16 个 `onBpSlot*` / `onHbSlot*` 的逻辑合并到对应的参数化方法中，通过 `switch(kind)` 或条件分支处理差异

3. `bindModel` 中改用 lambda 捕获 `MenuPanelKind` 的方式直接向 `constructor.BindEventCallback` 注册（方案 A），不再经过 `SlotGridEventHandlers` 模板

4. 删除原 16 个独立回调方法

5. 用 `SelectedSlot` 替代 `detail_bp_slot_` / `detail_hb_slot_`，统一选中/取消选中逻辑

6. 编译验证，全量测试

### Stage 4：RCSS 去重

注意：`slot_widgets.rcss` 中现有的 `.tf-slot-count` 仅负责显隐逻辑（`display: none/block`），`.tf-slot-icon-pop` 仅负责 hover 动画（`transform-origin + transition`）。真正的图标定位（`position: absolute; left: 2dp; top: 2dp; width: 16dp; height: 16dp`）和数量文字排版（`font-size, color, font-effect, text-align` 等）在页面级 `.bp-slot-icon` / `.hb-slot-icon` 中。因此**不能**直接把样式移入现有的 utility class，需要新建共享几何类。

1. 在 `inventory_menu.rcss` 中新增共享几何类：
   ```css
   /* 所有 slot 图标共享定位 */
   .inv-slot-icon {
       position: absolute;
       left: 2dp;
       top: 2dp;
       width: 16dp;
       height: 16dp;
   }

   /* 所有 slot 数量文字共享排版 */
   .inv-slot-count {
       position: absolute;
       left: 0;
       top: 0;
       width: 20dp;
       height: 20dp;
       text-align: right;
       vertical-align: bottom;
       font-size: 8dp;
       color: #ffffff;
       font-effect: shadow(1dp 1dp #000000cc);
       line-height: 20dp;
       padding: 0 1dp 0 0;
   }
   ```

2. 删除重复的 `.bp-slot-icon`、`.hb-slot-icon`、`.bp-slot-count`、`.hb-slot-count` 规则

3. 在 `inventory_menu.rml` 中更新 class：
   - `<div class="bp-slot-icon tf-slot-icon-pop" ...>` → `<div class="inv-slot-icon tf-slot-icon-pop" ...>`
   - `<div class="hb-slot-icon tf-slot-icon-pop" ...>` → `<div class="inv-slot-icon tf-slot-icon-pop" ...>`
   - `<div class="bp-slot-count tf-slot-count">` → `<div class="inv-slot-count tf-slot-count">`
   - `<div class="hb-slot-count tf-slot-count">` → `<div class="inv-slot-count tf-slot-count">`

4. 可视回归确认（手动运行，检查 slot 外观无变化）

## 验证清单

- [ ] 编译通过（`ninja -C build`）
- [ ] 全量测试通过（`ctest --output-on-failure`）
- [ ] 背包界面打开/关闭正常
- [ ] 背包 slot 悬停 tooltip 正常
- [ ] 背包 slot 左键点击选中 + detail panel 显示正常
- [ ] 背包 slot 右键点击打开 action menu 正常
- [ ] action menu Use / Discard / Cancel 操作正常
- [ ] Discard 二次确认弹窗正常
- [ ] 背包 slot 拖拽交换位置正常
- [ ] 背包 slot 拖拽到快捷栏绑定正常
- [ ] 快捷栏 slot 右键 action menu（Activate / Use / Unbind）正常
- [ ] 快捷栏 slot 之间拖拽交换正常
- [ ] 快捷栏 slot 拖出解绑正常
- [ ] Sort 按钮功能正常
- [ ] Trash 按钮功能正常
- [ ] ESC 关闭 action menu / 返回游戏正常
- [ ] 外部 InventoryChanged / HotbarChanged 事件触发 UI 刷新正常
- [ ] 角色面板信息显示正常
- [ ] tab bar 高亮跟随 active_tab_id 切换（当前仅 Inventory 可点击，其余 disabled）
- [ ] Slot 图标和数量文字样式无视觉变化（Stage 4 后）
- [ ] `tf-nav-root` 已添加：RmlUi 原生键盘/手柄导航根已启用（`nav: auto` 生效），为将来恢复导航做好准备
- [ ] 非活跃 tab 的 `onDeactivated` 确实断开了 dispatcher（切换到未来新 tab 后，InventoryChanged 不再触发 inventory 逻辑）

## 风险与注意事项

1. **数据类型注册的生命周期**：RmlUi 的 `DataTypeRegister` 要求类型在 model 存活期间保持有效。当前 `type_register_` 和 `data_types_registered_` 标志属于 Scene 级别，Tab Content 不应重复注册。`SlotGridViewModel` 类型注册保留在 Scene 壳中（跨 Tab 共享），`ActionEntryViewModel` 可随 Tab Content 注册（Tab 专属类型）。

2. **`RmlDocumentController` 引用传递**：`InventoryTabContent` 需要调用 `markDirty`，因此持有 `RmlDocumentController&`。注意不要在 Tab Content 中调用 `load` / `unload`——这些仍由 Scene 管理。

3. **Dispatcher 连接时序**：`InventoryChanged` / `HotbarChanged` 的连接在 `onActivated` 中执行、在 `onDeactivated` 中断开，确保非活跃 tab 不会收到事件。析构函数中防御性再断一次。

4. **`active_tab_id` 的 RmlUi 绑定类型**：RmlUi data binding 不直接支持 C++ enum class，需用 `int` 镜像变量。`switchTab()` 负责同时更新 enum 成员和 int 镜像并 markDirty。

5. **`data-if` 对已渲染元素的影响**：`data-if="active_tab_id == 0"` 在条件为 false 时会从 DOM 中移除元素。如果 Tab Content 持有对这些元素的指针/引用，切 tab 后会悬空。因此 Tab Content 不应缓存 DOM 元素指针，而是每次通过 `document->GetElementById()` 查找。

6. **角色面板更新通知**：当前只有 `syncCharacterPanel()` 在 `initUI()` 调用一次。未来 Equipment Tab 装备变化后需要刷新角色信息，可通过 `entt::dispatcher` 事件（如 `CharacterInfoChanged`）通知 Scene 壳刷新。暂不需要在此次重构中实现，但设计上预留了这条路径。

7. **向后兼容**：按 CLAUDE.md 指示不考虑向后兼容，直接重构即可。

8. **测试适配**：`inventory_menu_scene_slot_grid_registration_test.cpp` 当前基于函数位置 grep 断言，拆文件后会断裂。建议顺手将其改为行为导向断言（验证数据模型是否成功创建、绑定变量是否存在），而非检查源码字符串。
