# JRPG 物品栏菜单重构 Phase 2 细化计划

## 结论

Phase 2 **可行**，而且现有架构已经具备主要前提：

- `SceneManager + Scene::requestPushScene()` 已支持独立菜单场景。
- `RmlUILayer` 已支持按 `owner_scene_id` 隔离文档交互。
- `InputContext::Menu` 已接入统一的 RmlUi 键盘/手柄导航。
- `InventoryChanged` / `InventorySyncCommand` 已能提供完整 40 槽数据。

但原计划里有几处需要修正，否则实现时会出现“UI 做出来了，但交互边界不稳定”的问题：

1. **不要在 Phase 2 里承诺移除 Hotbar 运行时链路。**
   当前 `PlayerControlSystem` 仍依赖 `HotbarComponent` 选择工具/种子。旧 Hotbar HUD 可以后续清理，但 `HotbarComponent / HotbarSystem / HotbarActivateCommand` 现在还不能删。
2. **不要把 `Q/E` 或 `LB/RB` 切标签页写成既定能力。**
   当前 `InputContext::Menu` 只放行 `menu_*` 动作，没有现成的 tab 切换 action。Phase 2 先做静态标签页占位，后续需要时再补输入映射。
3. **Tooltip 不能直接复用“跟随鼠标”的现状。**
   现有 `ItemTooltipUI` 只会跟随鼠标，无法满足键盘/手柄焦点驱动的菜单。Phase 2 需要补“锚定到选中槽位/元素”的能力。
4. **底部角色信息区先按“展示占位”处理。**
   当前运行时没有稳定的玩家姓名 / 等级 / 金币 / 农场名聚合数据源。Phase 2 不应被这块数据阻塞。
5. **旧 `InventoryUI` 不应继续作为正式入口。**
   新场景接管后，`GameScene` 应停止通过旧 HUD 逻辑打开背包；旧文件可暂时保留，删除放到后续清理阶段。

---

## 方案选择

### 推荐方案：独立场景 + 专用 UI 控制器 + 焦点驱动交互

- 新建 `InventoryMenuScene`，沿用 `PauseMenuScene` / `RestDialogScene` 的生命周期。
- 新建 `InventoryMenuUI`，只负责 40 槽 ViewModel、焦点状态、Tooltip、操作子菜单。
- 所有可交互节点统一使用 `button`，以 `focus` 作为“当前选中项”的唯一真源。

这是最稳妥的做法。它能直接复用现有场景栈、RmlUi 焦点导航和输入上下文，不需要再在 `GameScene` HUD 上硬拼一个大菜单状态机。

### 不推荐方案 1：在旧 `InventoryUI` 上继续改

- 旧实现是 HUD 内嵌、分页、拖拽导向。
- 新需求是独立场景、暂停游戏、焦点导航、操作子菜单。

继续在旧类上堆逻辑会把“鼠标拖拽背包”和“JRPG 菜单焦点导航”混在一起，后续维护成本更高。

### 不推荐方案 2：Phase 2 同时做 UI 重构 + 背包数据模型清理 + Hotbar 运行时替换

这会把视觉改造、输入重构、玩法链路替换混成一次大手术，回归范围过大。Phase 2 应先把新菜单场景独立落地，再决定何时真正替换 Hotbar 玩法链路。

---

## 实现思路

### 1. 场景层

- 新建 `InventoryMenuScene`，职责与 `PauseMenuScene` 对齐：
  - `init()`:
    - 记录 `previous_state_`
    - `GameState -> Paused`
    - `InputManager.pushContext(Menu)`
    - 初始化 `InventoryMenuUI`
    - 连接 `menu_cancel`，并实现“先关 submenu，再关 scene”的优先级
  - `clean()`:
    - 移除事件监听、销毁 UI、恢复 `previous_state_`
    - `popContext()`
- `GameScene::onInventoryToggle()` 改为 `requestPushScene(std::make_unique<InventoryMenuScene>(...))`
- 这里**不需要**新增全局 `OpenInventoryMenu` command。现有做法里暂停菜单也是由 `GameScene` 直接 push，Phase 2 保持一致即可。

### 2. UI 控制器层

- 新建 `InventoryMenuUI`，但不要依赖 `active_page_` 语义：
  - 直接缓存 40 个 `slot_items_`
  - 全量接收 `InventoryChanged` / `InventorySyncCommand`
  - `active_page` 先忽略，仅保证不影响旧存档/旧系统
- 控制器内部维护：
  - `selected_slot_index_`
  - `action_menu_open_`
  - `action_menu_target_slot_`
  - `action_menu_entries_`
- 新菜单采用“焦点即选中”：
  - 鼠标 hover 通过 `HoverFocusSyncListener` 把焦点同步到按钮
  - 键盘/手柄通过 RmlUi 导航切焦点
  - Tooltip、子菜单、选中高亮都基于当前焦点槽位

### 3. RML / RCSS 结构

- 文件建议放到 `ui/rmlui/scenes/`，而不是新开 `ui/rmlui/menu/`
  - 该菜单本质是独立场景，不是 HUD 片段
- 结构分为四块：
  - 全屏 overlay
  - 主面板
  - 背包区（10x4 槽位按钮）
  - 底部角色信息区
- Phase 2 中的标签页与装备槽：
  - 标签页做**静态展示占位**
  - 装备槽做**禁用按钮/非交互占位**
  - 先不要把未实现页签纳入正式 focus graph
- 右上垃圾桶图标建议先作为视觉元素，不单独做焦点节点。
  丢弃动作统一走物品操作子菜单，避免出现两套入口。

### 4. Tooltip 方案

- 推荐继续复用 `ItemTooltipUI`，但补一个“锚点模式”：
  - 能按逻辑坐标或目标元素矩形定位
  - 优先显示在槽位右侧，越界时翻到左侧/下侧
- `InventoryMenuUI` 在焦点变更时调用 tooltip 更新。
- 鼠标模式只是焦点切换的一种来源，不再让 tooltip 自己读取鼠标位置决定一切。

### 5. 操作子菜单

- Phase 2 可以做，但建议限定为“菜单壳 + 最小可用行为”：
  - `Use`
  - `Discard`
  - `Cancel`
- `Equip` 先只做禁用项或按物品类型隐藏，不要伪接入未来装备系统。
- 交互规则：
  - `Confirm` on slot -> 打开子菜单
  - `Cancel` while submenu open -> 只关闭 submenu，不关闭整个 scene
  - `Cancel` on root -> `requestPopScene()`
- 命令接线：
  - `Use` -> 复用 `UseItemCommand`
  - `Discard` -> 复用 `RemoveItemCommand{item_id=当前物品, slot_index=当前槽位, count=1}`
- 若担心误删，Phase 2 里应补一个简短确认层，至少做到“第一次选中 Discard，第二次 Confirm 才真正执行”。

### 6. Phase 2 的边界

Phase 2 **包含**：

- 独立场景打开/关闭
- 全屏菜单 RML/RCSS
- 40 槽显示与同步
- 键盘/手柄焦点导航
- 焦点驱动 Tooltip
- 最小可用操作子菜单

Phase 2 **不包含**：

- 删除 `HotbarComponent / HotbarSystem`
- 替换玩家当前工具/种子的运行时选择机制
- 背包存档结构清理（`active_page` 字段下线）
- 真正可用的 Equipment / Quest / Map / Options 标签页
- `Q/E` / `LB/RB` 页签切换快捷键

---

## 需要新增的文件

- `src/game/scene/inventory_menu_scene.h`
- `src/game/scene/inventory_menu_scene.cpp`
- `src/game/ui/inventory_menu_ui.h`
- `src/game/ui/inventory_menu_ui.cpp`
- `ui/rmlui/scenes/inventory_menu.rml`
- `ui/rmlui/scenes/inventory_menu.rcss`

---

## 需要修改的文件

- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- `src/game/ui/item_tooltip_ui.h`
- `src/game/ui/item_tooltip_ui.cpp`
- `src/CMakeLists.txt`

Phase 2 原则上**不修改**：

- `src/game/component/inventory_component.h`
- `src/game/system/hotbar_system.*`
- `src/game/system/player_control_system.*`
- `src/game/save/*`

---

## 实现步骤

### Step 1. 接入独立场景入口

- 新建 `InventoryMenuScene`
- 按 `PauseMenuScene` 模式接入 `Paused + Menu Context`
- `GameScene::onInventoryToggle()` 改为 push 新场景
- 旧 `inventory` HUD toggle 入口停用

说明：
这是 Phase 2 的第一落点，先保证菜单生命周期成立，再做具体 UI。

### Step 2. 搭建 RML 骨架与 focus graph

- 新建 `inventory_menu.rml/rcss`
- 所有交互节点统一使用 `button`
- 背包 40 槽、关闭按钮、子菜单项建立稳定 id/class
- 所有可导航按钮显式设置 `tab-index: auto`
- 网格内部使用 `nav-*` 或默认邻近导航形成稳定路径

说明：
先把“可导航的空菜单”跑起来，能快速验证 `InputContext::Menu` 是否与布局匹配。

### Step 3. 实现 `InventoryMenuUI` 的 40 槽数据模型

- 绑定 40 槽 ViewModel
- 订阅 `InventoryChanged`
- scene 打开时主动 `InventorySyncCommand`
- 先忽略 `active_page`，把现有 40 槽直接平铺显示

说明：
这里先只做展示同步，不碰背包底层结构，降低改动面。

### Step 4. 把“选中项”统一收敛到焦点

- 用 `HoverFocusSyncListener` 统一鼠标 hover -> focus
- `InventoryMenuUI` 维护当前 focused slot
- 槽位高亮样式、tooltip 更新、confirm 行为都从 focused slot 派生

说明：
这一步是整个菜单稳定性的核心。只要“焦点 = 选中项”建立清楚，后面的 tooltip 和子菜单都会简单很多。

### Step 5. 扩展 Tooltip 为锚点定位

- 在 `ItemTooltipUI` 上增加锚点模式
- 支持按元素/逻辑坐标定位
- 焦点切换时刷新 tooltip 位置与内容
- 空槽、禁用项、submenu 打开时隐藏 tooltip

说明：
不解决这个点，键盘/手柄模式下的 tooltip 体验会明显不对。

### Step 6. 实现最小可用操作子菜单

- 选中物品后打开 submenu
- `Use` 接现有 `UseItemCommand`
- `Discard` 接现有 `RemoveItemCommand`
- `Cancel` 返回槽位焦点
- submenu 打开时冻结背包网格 focus，避免焦点乱跳

说明：
先做最小闭环，不引入装备系统、任务页或多层弹窗。

### Step 7. 收口 `GameScene` 对旧 InventoryUI 的依赖

- 新场景稳定后，`GameScene` 不再初始化旧 `InventoryUI`
- `HotbarUI` 继续保留
- `ItemTooltipUI` 继续作为共享组件使用

说明：
文件删除可以后做，但运行时入口要先唯一化，避免新旧两套背包同时订阅事件。

### Step 8. 做一轮验证并补回归清单

- 打开/关闭菜单
- 场景切换时恢复 `Playing`
- 鼠标、键盘、手柄三种输入路径
- tooltip 越界修正
- submenu cancel 优先级
- 使用 / 丢弃后的物品同步

说明：
这一步通过后，Phase 2 才算真正可进入编码执行。

---

## 待办清单

- [ ] 新建 `InventoryMenuScene` 并接入场景栈
- [ ] `GameScene` 的 inventory 输入改为 push 新场景
- [ ] 新建 `inventory_menu.rml/rcss`
- [ ] 背包 40 槽改为 button + focus 导航
- [ ] 接入 `HoverFocusSyncListener`
- [ ] 新建 `InventoryMenuUI` 并绑定 40 槽数据
- [ ] 菜单打开时发起 `InventorySyncCommand`
- [ ] `ItemTooltipUI` 增加锚点定位模式
- [ ] 焦点变化时刷新 tooltip
- [ ] 标签页做静态占位，不进入正式交互
- [ ] 装备槽做禁用占位，不进入正式交互
- [ ] 实现最小可用操作子菜单
- [ ] `Use` 接 `UseItemCommand`
- [ ] `Discard` 接 `RemoveItemCommand`
- [ ] `menu_cancel` 优先关闭 submenu，其次关闭 scene
- [ ] `GameScene` 停用旧 InventoryUI 入口
- [ ] 更新 `src/CMakeLists.txt`
- [ ] 完成打开/关闭、导航、tooltip、同步回归验证

---

## 待确认问题

若没有额外要求，我建议按下面两个默认假设推进：

1. **底部角色信息区先用占位文案。**
   头像可用已确认素材，姓名 / 等级 / 金币 / 农场名先放静态值或 `--`，等后续有人物状态聚合源再接真数据。
2. **`Discard` 在 Phase 2 做真实行为，但带一次确认。**
   这样菜单闭环完整，同时避免误操作。

如果你希望：

- 底部角色信息区在 Phase 2 就必须接真实数据，或
- `Discard` 想延后到 Phase 3，只先做菜单壳

那下一版实现计划需要再收窄一次范围。
