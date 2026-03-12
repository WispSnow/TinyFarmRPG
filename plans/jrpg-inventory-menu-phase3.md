# Phase 3: 菜单交互补完与旧 InventoryUI 清理

## 可行性分析

按**当前代码**看，Phase 3 完全可行，但范围需要重新定义。

当前 Phase 2 实际已经完成了这些基础：

- `InventoryMenuScene` 已作为独立场景接入
- 菜单内已存在 `backpack + menu hotbar + detail panel + tooltip`
- 菜单内已支持背包与 hotbar 间拖拽、换位、重绑
- `GameScene` 已改为 `inventory` 键 push 新场景
- 探索态 `HotbarUI` / `HotbarComponent` / `HotbarSystem` 仍在正常工作

因此，Phase 3 的正确目标不是“移除 Hotbar”，而是：

1. 保留探索态 `HotbarUI`
2. 保留菜单内 hotbar 区域
3. 在此基础上补齐操作子菜单、角色信息区数据绑定、旧 `InventoryUI` 清理、分页移除、排序命令

## 对审阅意见的结论

### 1. 关于 `UseItemCommand` 的执行时机

这个担心**有价值，但结论要修正**。

当前场景栈行为是：

- `SceneManager::fixedUpdate()` 只更新栈顶场景
- 菜单打开后，底层 `GameScene` 的 scheduler 确实会被冻结

但当前命令系统不是靠 scheduler 拉取，而是靠全局 `dispatcher` 的订阅回调：

- `ItemUseSystem` 在构造时已向全局 `dispatcher` 订阅 `UseItemCommand`
- 只要菜单中使用 `dispatcher.trigger(...)`，`ItemUseSystem::onUseItem()` 就会**立即执行**

所以本阶段不需要“先关菜单再发命令”，而是需要在计划里明确：

- 菜单内 `Use` 必须使用 `trigger`，不使用 `enqueue`
- `UseItemCommand` 的 `show_prompt` 应为 `false`

原因：

- 物品使用本身会立刻生效
- `show_prompt=true` 会走底层提示气泡计时逻辑，而该计时依赖底层系统更新；菜单打开时并不适合作为主反馈
- 菜单内的主反馈应是槽位变化、detail panel、tooltip 刷新

### 2. 关于子菜单缺少 RML 结构描述

这个意见**完全合理**。  
原计划只写了 Scene 状态机，没有把 UI 结构写清楚。Phase 3 需要同时定义：

- 子菜单浮层的 RML 结构
- `data-if` / `data-for` 绑定方式
- 子菜单定位策略
- 子菜单内部的键盘焦点导航规则

### 3. 关于 `trash` 按钮

这个意见**完全合理**。  
当前 RML 已经有 `trash-btn`，但没有处理器。Phase 3 必须明确它的去向，不能继续悬空。

本计划采用的策略是：

- `trash-btn` 不做独立功能
- 它复用与 `Discard` 完全相同的确认逻辑
- 仅对“当前选中的非空 backpack slot”生效

### 4. 关于 `UseItemCommand` 字段名与 `show_prompt`

这个意见**正确**。

应使用：

- `inventory_slot_index`
- `show_prompt = false`

### 5. 关于 `RemoveItemCommand` 参数

这个意见**正确**。

丢弃整组物品时，不是简单“触发 RemoveItemCommand”，而是必须先从槽位读取：

- `item_id`
- `count`
- `slot_index`

再构造：

- `RemoveItemCommand{ target, item_id, count, slot_index }`

### 6. 关于执行顺序

这个意见**合理，我同意调整**。

新的顺序改为：

1. 先清理旧 `InventoryUI` 路径
2. 再移除分页模型
3. 再补菜单交互与角色区绑定
4. 最后做排序与回归

这样能减少死代码干扰，并避免新功能落在即将删除的旧耦合上。

### 7. 关于 `Equip` / `Bind` 文案

这个意见**合理**。

按当前代码语义，本计划统一使用 `Bind`。  
如果最终 UI 想保留更偏 JRPG 的文案，例如显示成 `Equip`，那也只应视为表现层文案，后端行为仍然是“绑定到 hotbar”。

## 需要修正的地方

### 1. Hotbar 不是废弃物，而是当前运行时模型的一部分

当前 `HotbarComponent` / `HotbarSystem` 不只是 UI：

- 探索态当前工具/种子选择依赖它
- `PlayerControlSystem` 依赖它切换当前可用工具/种子
- `EntityFactory` 依赖它初始化玩家起始工具
- `SaveService` 依赖它保存/恢复快捷栏
- `GameScene` 依赖它驱动探索态 `HotbarUI`

所以本阶段**不移除**：

- `HotbarUI`
- `HotbarComponent`
- `HotbarSystem`
- `HotbarBindCommand / HotbarUnbindCommand / HotbarActivateCommand / HotbarSyncCommand`

### 2. 菜单内 hotbar 区域继续保留

当前菜单已经有一套独立的 hotbar ViewModel 与交互逻辑。  
这部分不是“旧代码残留”，而是现阶段菜单设计的一部分，应继续保留并增强：

- 继续显示当前快捷栏绑定
- 继续支持背包 -> hotbar、hotbar -> 背包、hotbar -> hotbar 拖拽
- 增加键盘/手柄下的确认动作与操作子菜单

### 3. 真正该清理的是旧 InventoryUI 路径

当前真正已经过时的是：

- `GameScene` 中旧 `InventoryUI` 的创建和引用
- `ui/rmlui/hud/inventory.rml/rcss`
- `InventoryUI` 与探索态 `HotbarUI` 的旧式联动
- `InventoryComponent.active_page_` 及其分页命令/事件字段

这些才是 Phase 3 的清理重点。

## 实现思路

Phase 3 继续以 `InventoryMenuScene` 为菜单主入口，保留当前 menu hotbar 结构，补齐缺失交互并收掉旧库存路径。

整体原则：

1. 菜单内保留 `hotbar + backpack + detail + char panel`
2. 鼠标拖拽逻辑保留，不回退
3. 先清理旧 `InventoryUI` 路径，再补菜单子菜单逻辑
4. 键盘/手柄 `Confirm` 增加操作子菜单，补足非鼠标路径
5. 探索态保留 `HotbarUI`
6. 移除背包分页模型，使 `InventoryComponent` 与菜单展示一致为 40 槽平铺

## 需要新增或修改的文件

### 主要修改文件

- `src/game/scene/inventory_menu_scene.h/cpp`
- `ui/rmlui/scenes/inventory_menu.rml/rcss`
- `src/game/scene/game_scene.h/cpp`
- `src/game/ui/hotbar_ui.h/cpp`
- `src/game/ui/inventory_ui.h/cpp`
- `ui/rmlui/hud/inventory.rml/rcss`
- `src/game/component/inventory_component.h`
- `src/game/defs/commands.h`
- `src/game/defs/events.h`
- `src/game/system/inventory_system.h/cpp`
- `src/game/domain/inventory_domain_service.h/cpp`
- `src/game/save/save_data.h/cpp`
- `src/game/save/save_service.cpp`
- `src/CMakeLists.txt`

### 本阶段不新增独立 UI 控制器类

继续沿用当前方案：

- 菜单数据绑定和事件处理直接放在 `InventoryMenuScene`
- 只有当子菜单状态机明显膨胀时，再考虑拆内部辅助结构

## 实现步骤

### Step 1: 删除旧 InventoryUI，并清理 GameScene 的旧库存接线

本步骤是本阶段最明确的“旧路径清理”：

- `GameScene` 不再创建 `inventory_ui_`
- 删除旧 `InventoryUI` 相关引用
- 删除旧 `ui/rmlui/hud/inventory.rml/rcss`
- 删除 `src/game/ui/inventory_ui.h/cpp`
- 保留 `hotbar_ui_`
- 保留 `HotbarChanged / HotbarSlotChanged` 在 `GameScene` 的探索态处理

同时建议顺手清理 `HotbarUI` 中仅为旧 `InventoryUI` 服务的耦合：

- 去掉 `InventoryUI* inventory_ui_`
- 去掉 `setInventoryUI()`
- 删除仅用于旧 inventory overlay 跨 UI 拖拽的分支

菜单内 hotbar 交互由 `InventoryMenuScene` 自己负责，不再需要探索态 `HotbarUI` 与旧 `InventoryUI` 互相通信。

### Step 2: 移除分页模型 `active_page_`

当前菜单已经按 40 槽平铺显示，但库存后端和存档仍残留分页字段。  
本阶段统一收口：

- `InventoryComponent` 删除 `active_page_`
- 删除 `InventorySetActivePageCommand`
- `InventoryChanged` 删除 `active_page`
- `InventorySystem` / `InventoryDomainService` 移除分页相关逻辑
- 旧 `InventoryUI` 删除后，分页按钮与切页行为自然消失

存档策略：

- 新代码不再写 `active_page`
- 读档时即使旧存档里存在该字段，也直接忽略
- 这一项不必为了单个废弃字段升级 schema

### Step 3: 在现有菜单 hotbar / backpack 结构上增加操作子菜单

在 `InventoryMenuScene` 内加入最小状态机与对应数据绑定：

- 当前菜单焦点来源：`BackpackSlot` / `HotbarSlot`
- 当前选中槽位索引
- `action_menu_visible`
- `action_menu_entries`
- 当前高亮动作索引
- `action_menu_left/top`
- discard 确认态

同时修改 `inventory_menu.rml/rcss`：

- 新增动作面板容器
- 使用 `data-if` 控制显隐
- 使用 `data-for` 渲染动作项
- 为动作项补 `focus/click/confirm` 事件
- 在 RCSS 中定义浮层样式、选中态和边界内定位表现

定位策略建议：

- 子菜单优先显示在当前选中槽位右侧
- 若右侧越界，则翻到左侧
- 若垂直方向越界，则 clamp 到面板内容区内
- 坐标由 Scene 计算后绑定为 `action_menu_left/top`

要求：

- 键盘/手柄 `Confirm` 打开子菜单
- 鼠标左键点击已选中槽位也可打开子菜单
- 鼠标拖拽开始时不打开子菜单
- 子菜单打开时暂停主网格焦点漫游和 tooltip 漫游
- 子菜单内部使用垂直列表导航，`nav-up/down` 在动作列表内收敛

### Step 4: 明确 backpack slot 的动作语义

对背包槽位按物品情况生成动作：

- `Use`
  - 条件：物品存在 `on_use_`
  - 行为：`UseItemCommand{ target, inventory_slot_index, 1, false }`
  - 说明：必须使用 `dispatcher.trigger(...)`，不使用 `enqueue`

- `Bind`
  - 条件：槽位非空
  - 行为：绑定到当前 active hotbar slot，并触发激活
  - 说明：这是当前代码语义，不是真正 RPG 装备

- `Discard`
  - 条件：槽位非空
  - 行为：先进入统一的 discard 确认态
  - 执行时从槽位读取 `item_id + count + slot_index`，再触发
    `RemoveItemCommand{ target, item_id, count, slot_index }`
  - 首版直接丢弃整组 stack，不做数量输入

- `Cancel`
  - 关闭子菜单

同时规定：

- `trash-btn` 复用 `Discard` 同一确认流程
- 仅对当前选中的非空 backpack slot 生效
- 若当前选中的是 hotbar 槽位或空背包槽位，则按钮禁用或点击无效

### Step 5: 补齐 hotbar slot 的键盘/手柄交互

当前 menu hotbar 已有鼠标与拖拽路径，但键盘确认路径还不完整。  
补齐后建议提供：

- `Activate`
  - 将该 hotbar 槽设为当前 active slot

- `Use`
  - 若绑定物品可用，则触发 `UseItemCommand{ target, inventory_slot_index, 1, false }`

- `Unbind`
  - 触发 `HotbarUnbindCommand`

- `Cancel`
  - 关闭子菜单

这样菜单内 hotbar 在非鼠标路径下也能闭环。

### Step 6: 角色信息区改为真实数据绑定

当前角色区基本还是静态文本，占位性质较强。  
本阶段改成“真实数据 + 占位表现”：

- 玩家名称：从 `NameComponent` 读取
- 头像：先保留静态占位或固定图
- 等级/职业：无稳定数据则继续占位文本
- 金币：若当前运行时无统一来源，则继续静态占位，不额外造系统
- 8 个装备槽：继续只读占位，不做实际装备逻辑

目标是先把右侧区域从硬编码文案改成可绑定结构，而不是提前进入装备系统。

### Step 7: 新增自动排序 Command，并同步修复 hotbar 映射

新增 `InventorySortCommand`，排序规则按当前数据即可：

- 非空槽位排前，空槽位排后
- 分类顺序建议：
  - `Tool`
  - `Seed`
  - `Consumable`
  - `Crop`
  - `Material`
  - `Unknown`
- 同类按 `display_name`
- 再按原始槽位索引，保证稳定排序

关键点：

- 排序不能只重排 `InventoryComponent.slots_`
- 必须同时重写 `HotbarComponent` 中的 `inventory_slot_index_`
- 然后发送 `InventoryChanged(full_sync)` 和 `HotbarChanged(full_sync)`

否则探索态和菜单内 hotbar 都会指向错误槽位。

### Step 8: 更新构建与回归验证

需要覆盖这些路径：

- 打开/关闭菜单
- backpack 焦点移动
- menu hotbar 焦点移动
- 背包 -> hotbar 拖拽
- hotbar -> 背包 拖拽
- hotbar -> hotbar 换位/重绑
- backpack 子菜单 `Use / Bind / Discard`
- `trash-btn` -> discard 确认
- hotbar 子菜单 `Activate / Use / Unbind`
- 删除旧 `InventoryUI` 后探索态 `HotbarUI` 仍正常工作
- 排序后 hotbar 映射仍正确
- 存档 / 读档后 inventory + hotbar 状态仍正确

## 待办

- [ ] Step 1: 删除旧 `InventoryUI` 与旧 inventory RML/RCSS
- [ ] Step 1: 清理 `GameScene` 中旧 inventory UI 集成，保留探索态 `HotbarUI`
- [ ] Step 1: 精简 `HotbarUI` 对旧 `InventoryUI` 的耦合
- [ ] Step 2: 删除 `active_page_` 与相关分页命令/事件字段
- [ ] Step 3: 为 `InventoryMenuScene` 增加操作子菜单状态机与对应 RML/RCSS
- [ ] Step 4: 接通 backpack slot 的 `Use / Bind / Discard / Cancel`
- [ ] Step 4: 让 `trash-btn` 复用 discard 确认逻辑
- [ ] Step 5: 接通 hotbar slot 的 `Activate / Use / Unbind / Cancel`
- [ ] Step 6: 将角色信息区改成真实数据绑定 + 占位显示
- [ ] Step 7: 新增 `InventorySortCommand`，并在排序后修复 hotbar 映射
- [ ] Step 8: 更新 `CMakeLists.txt` 并完成整体测试

## 非目标

以下内容不属于本阶段：

- 移除探索态 `HotbarUI`
- 移除 `HotbarComponent` / `HotbarSystem`
- 真正的 RPG 装备系统
- 装备槽可穿戴/可卸下逻辑
- 独立的角色成长面板（等级、职业、属性）
- 金币系统的完整运行时接入

## 备注

本计划已经按最新结论更新：

- 探索态 `HotbarUI` 保留
- 菜单内 hotbar 区域保留
- `UseItemCommand` 明确使用 `trigger + show_prompt=false`
- `trash-btn` 明确复用 discard 流程
- 执行顺序调整为“先清理，后补功能”
