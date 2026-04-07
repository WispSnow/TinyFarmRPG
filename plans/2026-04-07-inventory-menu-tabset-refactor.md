# InventoryMenuScene Tabset 与 InventoryTabContent 精简计划

## 元信息

- 任务ID：`UI-INVMENU-002`
- 任务标题：`InventoryMenuScene 迁移到 RmlUi tabset，并精简 InventoryTabContent`
- 优先级：`P1`
- 状态：`Proposed`
- 计划时间：`2026-04-07` 起
- 依赖任务：`UI-INVMENU-001` 已完成的 Scene 薄壳化与 `InventoryTabContent` 拆分
- 设计原则：优先使用 RmlUi 原生控件和数据绑定；保留 `IMenuTabContent` 生命周期边界；先搭好多 Tab 框架，再做局部复杂度收敛；不做向后兼容包袱

## 背景

当前代码已经完成第一轮拆分：

- `InventoryMenuScene` 已经从巨型类收敛为 Scene 薄壳，约 247 行。
- `InventoryTabContent` 承接背包页内容逻辑，约 1092 行。
- RML 已使用 `data-for`、`data-if`、`data-class-*`、`data-style-decorator`、RmlUi drag/drop、ninepatch 与 spritesheet。

当前 tab 切换仍是自建状态机：

- C++ 维护 `active_tab_id_` / `active_tab_id_bind_`。
- RML 用 `data-class-tab-active="active_tab_id == N"` 控制高亮。
- RML 用 `data-if="active_tab_id == 0"` 控制 Inventory 内容区创建/销毁。
- 点击 tab 通过 `data-event-click="switch_tab(N)"` 进入 C++。

RmlUi 6.2 已内置 `ElementTabSet`：

- `<tabset>` / `<tab>` / `<panel>` 负责 tab 与 panel 对应关系。
- 点击 `<tab>` 后自动设置 `<tab>` 与 `<panel>` 的 `:selected` 伪类。
- 自动将旧 `<panel>` 设为 `display: none`，新 `<panel>` 恢复显示。
- 切换时派发 `tabchange` 事件，参数包含 `tab_index`。
- RmlUi data event 可通过 `ev.tab_index` 读取事件参数，因此可写 `data-event-tabchange="switch_tab(ev.tab_index)"`。

## 综合判断

Claude 的判断中，“`<tabset>` 当前收益有限，等第二个标签页再迁移”在只做短期修补时成立。但本项目下一步明确要扩展 RPG 菜单页，且不需要顾虑向后兼容，因此这里调整优先级：**先迁移 `<tabset>`，把多 Tab 框架一次搭好**。

这样做的理由：

1. 越早迁移，越不会让 Equipment / Quests / Map / Options 继续依赖 `active_tab_id` + `data-if` 的自建协议。
2. `ElementTabSet` 已经处理了 panel 显隐、`:selected` 与点击切换，自建 `data-class-tab-active` 可以删除。
3. `IMenuTabContent::onActivated/onDeactivated()` 仍然保留，作为 gameplay/UI 资源生命周期边界；只把视觉 tab 切换交给 RmlUi。
4. 当前未实现的 tabs 不应再用“disabled 但 tabset 无原生 disabled”的半套方案；应先做可切换的 placeholder tab content，未来逐个替换。

`InventoryTabContent` 的大类问题也存在，但不应和 tabset 迁移混在同一个大改里一次完成。建议在 tabset 骨架稳定后，按低风险重复逻辑先收敛，再评估是否抽出 action menu。

## 目标

| 目标 | 衡量标准 |
|------|----------|
| 使用 RmlUi 原生 tabset | `inventory_menu.rml` 使用 `<tabset>/<tab>/<panel>`；不再使用 `data-class-tab-active` 控制 tab 高亮 |
| 保留 C++ Tab 生命周期 | tabset `tabchange` 驱动 `InventoryMenuScene::switchTab(...)`；活跃 tab 仍调用 `onActivated/onDeactivated/update/onCancel` |
| 多 Tab 框架到位 | Equipment / Quests / Map / Options 都有 panel 与 placeholder tab content，不再是不可点击 disabled icon |
| Inventory 行为不回退 | 背包/快捷栏 slot、tooltip、detail、action menu、sort/trash、drag/drop 行为保持一致 |
| InventoryTabContent 复杂度下降 | 合并重复物品解析和 hotbar 同步逻辑；后续可单独抽出 action menu |
| 测试覆盖同步 | 现有 grep 型测试改为断言 tabset 结构与生命周期绑定，不再断言旧 `active_tab_id` 协议 |

## 非目标

- 不在本阶段实现 Equipment / Quests / Map / Options 的完整业务。
- 不在 tabset 迁移同一阶段抽出大型 action menu 子系统。
- 不为 tabset 引入“disabled tab”自定义控件或拦截回滚逻辑。
- 不把所有 detail/action/menu 状态强行塞进单个大 struct，只在确实降低复杂度时再做。

## 关键设计

### 1. RML 结构迁移到 tabset

当前：

```xml
<div id="tab-bar">
    <button id="tab-inventory"
            data-class-tab-active="active_tab_id == 0"
            data-event-click="switch_tab(0)"></button>
    ...
    <button id="sort-btn" data-if="active_tab_id == 0">Sort</button>
    <button id="trash-btn" data-if="active_tab_id == 0"></button>
</div>

<div id="inventory-col" data-if="active_tab_id == 0">
    ...
</div>
```

目标：

```xml
<tabset id="menu-tabset" data-event-tabchange="switch_tab(ev.tab_index)">
    <tab id="tab-inventory" class="tab-icon tf-nav-auto tf-focus-ring-gold"></tab>
    <panel id="panel-inventory">
        <div id="inventory-actions">
            <button id="sort-btn" class="tf-nav-auto tf-focus-ring-blue" data-event-click="sort">Sort</button>
            <button id="trash-btn" class="tf-nav-auto tf-focus-ring-danger" data-event-click="trash"></button>
        </div>
        <!-- existing inventory content -->
    </panel>

    <tab id="tab-equipment" class="tab-icon tf-nav-auto tf-focus-ring-gold"></tab>
    <panel id="panel-equipment" class="placeholder-panel">
        <div class="placeholder-title">Equipment</div>
    </panel>

    <tab id="tab-quests" class="tab-icon tf-nav-auto tf-focus-ring-gold"></tab>
    <panel id="panel-quests" class="placeholder-panel">
        <div class="placeholder-title">Quests</div>
    </panel>

    <tab id="tab-map" class="tab-icon tf-nav-auto tf-focus-ring-gold"></tab>
    <panel id="panel-map" class="placeholder-panel">
        <div class="placeholder-title">Map</div>
    </panel>

    <tab id="tab-options" class="tab-icon tf-nav-auto tf-focus-ring-gold"></tab>
    <panel id="panel-options" class="placeholder-panel">
        <div class="placeholder-title">Options</div>
    </panel>
</tabset>
```

注意：

- `Sort` / `Trash` 不放进 `<tabs>`；它们属于 Inventory panel 的内容。需要通过 RCSS 把它们视觉上放到 tab 行右侧，或放在 inventory panel 顶部。
- `<tab>` 是普通元素，不是 button；必须保留 `tf-nav-auto` 或显式 `tab-index: auto`，否则键盘/手柄导航会退化。
- 原 `.tab-active` 改为 `tab:selected` 或 `#menu-tabset tab:selected`。
- 原 `.tab-disabled` 删除；未实现 tabs 先展示 placeholder，避免和 tabset 的默认切换机制冲突。

### 2. C++ Tab 生命周期仍由 Scene 管

`InventoryMenuScene` 保留：

```cpp
std::unordered_map<MenuTabId, std::unique_ptr<IMenuTabContent>, MenuTabIdHash> tabs_{};
MenuTabId active_tab_id_{MenuTabId::Inventory};
```

但删除 RML 绑定状态：

- 删除 `active_tab_id_bind_`。
- 删除 `constructor.Bind("active_tab_id", &active_tab_id_bind_)`。
- 删除 `switchTabByIndex` 旧命名，替换为 `switchTabFromTabsetIndex(int tab_index)` 或保留但明确语义。
- 删除 RML 中所有 `data-class-tab-active` 与 `data-if="active_tab_id == ..."`。

新增/调整：

```cpp
constexpr std::array<MenuTabId, 5> kTabOrder = {
    MenuTabId::Inventory,
    MenuTabId::Equipment,
    MenuTabId::Quests,
    MenuTabId::Map,
    MenuTabId::Options,
};

void InventoryMenuScene::switchTabFromTabsetIndex(int tab_index) {
    if (tab_index < 0 || tab_index >= std::ssize(kTabOrder)) {
        return;
    }
    switchTab(kTabOrder[tab_index]);
}
```

`data-event-tabchange="switch_tab(ev.tab_index)"` 继续绑定到 `switch_tab` event callback。

`initUI()` 加载文档后仍手动激活默认 Inventory tab：

```cpp
syncCharacterPanel();
if (auto* tab = activeTab()) {
    tab->onActivated();
}
document_controller_.markAllDirty();
```

原因：RmlUi 默认 active tab 为 0，不会因为初始状态派发 `tabchange`。初始生命周期仍由 Scene 显式执行。

### 3. PlaceholderTabContent

为避免 tabset 已切换 UI 但 C++ `tabs_` 找不到 tab 的状态不一致，应给每个 tab 都注册一个 content：

```cpp
class PlaceholderTabContent final : public IMenuTabContent {
public:
    bool bindModel(Rml::DataModelConstructor&) override { return true; }
    void onActivated() override {}
    void onDeactivated() override {}
    void update(float) override {}
    bool onCancel() override { return false; }
};
```

放置建议：

- 如果只在 inventory menu 使用，可放在 `inventory_menu_scene.cpp` 匿名 namespace。
- 如果后续多个菜单会复用，再提取到 `game/ui/menu_tab_content.h/.cpp`。

### 4. display:none 与 data-if 的取舍

`tabset` 使用 `display:none` 隐藏非活跃 panel，不会像 `data-if` 一样销毁 DOM 子树。这是可以接受的：

- 当前 inventory panel 只有一个真实复杂页，其他 panel 是 lightweight placeholder。
- 资源生命周期仍由 `onActivated/onDeactivated()` 管，切走 Inventory 时会断开 dispatcher、关闭 action menu、清 tooltip、清 drag state。
- 未来某个 tab 变得很重时，在该 panel 内部再使用 `data-if` 或按 `onActivated()` 延迟同步数据，不要回退到全局 `active_tab_id`。

### 5. InventoryTabContent 第一轮精简

先做两个低风险收敛：

#### 5.1 合并物品解析

新增一个统一解析方法，减少 tooltip/detail 两条并行链路：

```cpp
const game::data::ItemData* resolveItemForPanel(MenuPanelKind kind, int slot_index) const;
int resolveInventorySlotForPanel(MenuPanelKind kind, int slot_index) const;
```

目标替换：

- `showTooltipForInventorySlot`
- `showTooltipForHotbarSlot`
- `showTooltipForPanel`
- `updateDetailForInventorySlot`
- `updateDetailForHotbarSlot`
- `updateDetailForPanel`

收敛后保留更少入口：

```cpp
void showTooltipForPanel(MenuPanelKind kind, int slot_index);
void updateDetailForPanel(MenuPanelKind kind, int slot_index);
void setDetailFromItem(const game::data::ItemData& item);
```

#### 5.2 简化 `syncHotbarFromInventory`

把三段 `populateSlotGridViewModel(...)` 分支合并成一个路径：

```cpp
std::optional<engine::ui::SlotItem> slot_item;
bool active = false;
if (hotbar && inventory) {
    active = hotbar->active_slot_index_ == i;
    const int inv_idx = hotbar->slot(i).inventory_slot_index_;
    if (!hotbar->slot(i).empty() && inv_idx >= 0 && inv_idx < inventory->slotCount()) {
        slot_item = toSlotItem(inventory->slot(inv_idx));
    }
}

populateSlotGridViewModel(vm, slot_item, item_catalog_, {
    .can_drag = slot_item.has_value(),
    .is_selected = selected_slot_.isHotbar() && selected_slot_.index == i,
    .is_active = active,
    .label = vm.label,
});
```

### 6. Slot event helper 后续收敛

当前 `InventoryTabContent::bindModel()` 内部手写 `bind_grid_events(prefix, kind)`，而 `HotbarUI` 已经使用 `bindSlotGridEvents(...)`。

建议新增一个带 context 的 helper，避免重复拼接事件名：

```cpp
template<typename Owner, typename Context>
using IndexedContextEventHandler = void (Owner::*)(Context, int, Rml::Event&);
```

或更直接新增 inventory 专用小 helper：

```cpp
bool bindMenuPanelSlotGridEvents(Rml::DataModelConstructor& constructor,
                                 std::string_view prefix,
                                 MenuPanelKind kind,
                                 InventoryTabContent& owner);
```

这一步不作为 tabset 迁移的阻塞项，可在 5.1 / 5.2 之后做。

### 7. Action menu 是否提取

Claude 建议把 action menu 提取成独立类是合理的，但建议排到第二阶段之后再做。原因：

- 这块包含 DOM 几何定位、RmlUi update 时序、entry 数组生命周期、selected slot 命令执行语义，风险比 `resolveItemForPanel` 高。
- 当前 `closeActionMenu()` 里有避免 `data-for` stale index 的时序注释，抽取时必须保留该行为。

若提取，目标类建议是 UI 通用组件，而不是 inventory 业务类：

```cpp
class SlotActionMenu {
public:
    using Entry = ...;
    using ClickCallback = std::function<void(int)>;

    bool bindModel(Rml::DataModelConstructor& constructor);
    void show(Rml::String title, std::vector<Entry> entries, std::string_view grid_id, int slot_index);
    void close();
    bool visible() const;
};
```

`InventoryTabContent` 仍负责：

- 根据 backpack/hotbar 构建 entries。
- 根据 action id 派发 `UseItemCommand` / `HotbarBindCommand` / `RemoveItemCommand` 等 gameplay 命令。

## 执行阶段

### Stage 0：基线确认

- [ ] 记录当前相关文件行数：
  - `src/game/scene/inventory_menu_scene.*`
  - `src/game/ui/inventory_tab_content.*`
  - `ui/rmlui/scenes/inventory_menu.rml`
  - `ui/rmlui/scenes/inventory_menu.rcss`
- [ ] 跑现有相关测试，确认改前状态：
  - `ninja -C build/debug game_tests`
  - `cd build/debug && ctest --output-on-failure -R InventoryMenu`

### Stage 1：迁移到 `<tabset>`

- [ ] 修改 `inventory_menu.rml`：
  - [ ] 用 `<tabset id="menu-tabset" data-event-tabchange="switch_tab(ev.tab_index)">` 替换手写 tab bar。
  - [ ] 把 Inventory 内容放进 `<panel id="panel-inventory">`。
  - [ ] 增加 Equipment / Quests / Map / Options placeholder panels。
  - [ ] 移除 `data-class-tab-active`。
  - [ ] 移除 `data-if="active_tab_id == 0"`。
  - [ ] 将 Sort / Trash 移入 Inventory panel。
- [ ] 修改 `inventory_menu.rcss`：
  - [ ] 为 `tabset`、`tabs`、`tab`、`panels`、`panel` 设置显式 `display`。
  - [ ] 用 `tab:selected` 替代 `.tab-active`。
  - [ ] 删除 `.tab-disabled`。
  - [ ] 保证 `<tab>` 有 `tab-index: auto` / `nav-*`。
  - [ ] 调整 Sort / Trash 的定位，使视觉不明显退化。
- [ ] 修改 `InventoryMenuScene`：
  - [ ] 删除 `active_tab_id_bind_` 和 `active_tab_id` data binding。
  - [ ] 使用 tabset index -> `MenuTabId` 的固定映射。
  - [ ] 绑定 `switch_tab(ev.tab_index)`。
  - [ ] 给四个未实现 tabs 注册 placeholder content。
  - [ ] 保持初始化时显式激活 Inventory content。
- [ ] 更新测试：
  - [ ] `InventoryMenuSceneSlotGridRegistrationTest` 不再断言 `data-class-tab-active`。
  - [ ] 新断言 RML 包含 `<tabset id="menu-tabset">`、`data-event-tabchange="switch_tab(ev.tab_index)"`、五个 `<panel id="panel-*">`。
  - [ ] 新断言 Scene 不再绑定 `active_tab_id`，但仍绑定 `switch_tab`。

### Stage 2：低风险精简 `InventoryTabContent`

- [ ] 新增 `resolveInventorySlotForPanel(...)`。
- [ ] 新增 `resolveItemForPanel(...)`。
- [ ] 合并 tooltip/detail 的重复 item lookup。
- [ ] 简化 `syncHotbarFromInventory()` 的重复 `populateSlotGridViewModel(...)` 分支。
- [ ] 保持 `markDirty` 粒度不大改，只在重复明显处增加小 helper。
- [ ] 跑 `game_tests` 和 `InventoryMenu` 相关测试。

### Stage 3：统一 slot event 绑定 helper

- [ ] 评估扩展 `slot_grid_support.h` 的通用 helper，支持额外 `MenuPanelKind` 上下文。
- [ ] 用 helper 替换 `InventoryTabContent::bindModel()` 内部手写 `bind_grid_events`。
- [ ] 不改变 RML 事件名，减少行为风险。

### Stage 4：Action menu 提取评估与实施

- [ ] 先确认 Stage 1-3 后 `InventoryTabContent` 的规模和复杂度。
- [ ] 如果 action menu 仍是最大复杂源，新增 `SlotActionMenu`。
- [ ] 抽取状态：
  - `ActionEntryViewModel`
  - `action_menu_entries_`
  - `action_menu_title_`
  - `action_menu_visible_`
  - `show/close/position/bind entry click`
- [ ] 保留 `executeAction()` 在 `InventoryTabContent` 中，避免 UI 组件依赖 gameplay 命令。
- [ ] 保留 `closeActionMenu()` 的 data-for stale index 时序保护。

## 验证清单

### 自动化验证

- [ ] `ninja -C build/debug game_tests`
- [ ] `cd build/debug && ctest --output-on-failure -R InventoryMenu`
- [ ] 如改动 slot/grid 公共 helper，再跑：
  - `cd build/debug && ctest --output-on-failure -R Hotbar`

### 手动验证

- [ ] 打开 InventoryMenu 默认显示 Inventory tab。
- [ ] 鼠标点击五个 tab，`:selected` 高亮和 panel 显隐正确。
- [ ] 键盘/手柄导航能聚焦并切换 tab。
- [ ] Inventory tab 下 hotbar/backpack slot 左键选择正常。
- [ ] 右键 action menu 正常显示、取消、使用、丢弃确认。
- [ ] Sort / Trash 按钮仍可点击，且只在 Inventory panel 可见。
- [ ] 物品 tooltip 和 detail panel 正常。
- [ ] inventory 内部拖拽、inventory -> hotbar、hotbar -> inventory 仍正常。
- [ ] 切走 Inventory tab 时 tooltip/action menu/drag 临时状态被清理。
- [ ] 切回 Inventory tab 后数据重新同步。

## 风险与应对

| 风险 | 应对 |
|------|------|
| `tabset` 先更新 UI 后派发 `tabchange`，C++ 若拒绝 tab 会不一致 | 不做 disabled/reject；给所有 tab 注册 placeholder content |
| `<tab>` 不是 button，导航可能失效 | RCSS 显式设置 `tab-index: auto` 与 `nav-*`，沿用 `tf-nav-auto` |
| Sort / Trash 移出 tab bar 后视觉退化 | 先放进 Inventory panel，再通过 RCSS 复原视觉位置；不要为此保留旧 `active_tab_id` |
| `display:none` 不销毁 panel DOM | 由 `onActivated/onDeactivated` 管理 dispatcher/tooltip/action menu/drag state；未来重 tab 内部再局部 data-if |
| action menu 定位依赖 DOM 几何 | Stage 1 不动 action menu；Stage 4 单独处理 |
| 现有测试是源码字符串断言 | 同步更新断言目标，避免测试强行锁死旧实现 |

## 预期结果

完成 Stage 1 后：

- `InventoryMenuScene` 不再维护 RML 用的 `active_tab_id` 绑定变量。
- `inventory_menu.rml` 的 tab 切换由 `<tabset>` 原生处理。
- 五个 tabs 的 DOM 和生命周期框架都已存在，未来实现新 tab 只需替换 placeholder content。

完成 Stage 2-3 后：

- `InventoryTabContent` 的重复 item lookup 和 hotbar 同步分支减少。
- Slot grid 事件绑定与 `HotbarUI` 更接近，后续维护成本下降。

完成 Stage 4 后：

- `InventoryTabContent` 从“背包页所有子系统集合”进一步收敛为背包页行为编排，action menu 作为可复用 UI 子组件存在。
