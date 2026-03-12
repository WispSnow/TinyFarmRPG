# Phase 3: 菜单交互补完与旧 InventoryUI 清理

## 可行性分析

按**当前代码**看，Phase 3 完全可行，但范围需要重新定义。

当前 Phase 2 实际已经完成了这些基础：

- `InventoryMenuScene` 已作为独立场景接入
- 菜单内已存在 `backpack + menu hotbar + detail panel + tooltip`
- 菜单内已支持背包与 hotbar 间拖拽/换位/绑定
- `GameScene` 已改为 `inventory` 键 push 新场景
- 探索态 `HotbarUI` / `HotbarComponent` / `HotbarSystem` 仍在正常工作

因此，Phase 3 的正确目标不是“移除 Hotbar”，而是：

1. **保留探索态 HotbarUI**
2. **保留菜单内 hotbar 区域**
3. 在此基础上补齐**操作子菜单、角色信息区数据绑定、旧 InventoryUI 清理、分页移除、排序命令**

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

### 3. `Equip` 语义按当前代码解释

当前项目还没有真正的 JRPG 装备系统，也没有 `EquipmentComponent`。  
因此本阶段若保留 `Equip` 文案，其实际语义应为：

- **将物品绑定到 hotbar，并激活该槽位**

这更符合现有代码与当前玩法闭环。  
如果希望表达更准确，也可以在 UI 文案里直接改成 `Bind`。

### 4. 真正该清理的是旧 InventoryUI 路径

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
3. 键盘/手柄 `Confirm` 增加操作子菜单，补足非鼠标路径
4. 探索态保留 `HotbarUI`
5. 删除旧 `InventoryUI`
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

### Step 1: 在现有菜单 hotbar / backpack 结构上增加操作子菜单

在 `InventoryMenuScene` 内加入最小状态机：

- 当前菜单焦点来源：`BackpackSlot` / `HotbarSlot`
- 当前选中槽位索引
- 子菜单是否打开
- 子菜单动作列表
- 当前高亮动作
- discard 确认态

要求：

- 键盘/手柄 `Confirm` 打开子菜单
- 鼠标拖拽开始时不打开子菜单
- 子菜单打开时暂停焦点切换和 tooltip 漫游

### Step 2: 明确 backpack slot 的动作语义

对背包槽位按物品情况生成动作：

- `Use`
  - 条件：物品存在 `on_use_`
  - 行为：`UseItemCommand{ target, slot_index, 1, true }`

- `Equip` 或 `Bind`
  - 条件：槽位非空
  - 行为：绑定到当前 active hotbar slot，并触发激活
  - 说明：这是当前代码语义，不是真正 RPG 装备

- `Discard`
  - 条件：槽位非空
  - 行为：先二次确认，再触发 `RemoveItemCommand`
  - 首版直接丢弃整组 stack，不做数量输入

- `Cancel`
  - 关闭子菜单

### Step 3: 补齐 hotbar slot 的键盘/手柄交互

当前 menu hotbar 已有鼠标与拖拽路径，但键盘确认路径还不完整。  
补齐后建议提供：

- `Activate`
  - 将该 hotbar 槽设为当前 active slot

- `Use`
  - 若绑定物品可用，则触发 `UseItemCommand`

- `Unbind`
  - 触发 `HotbarUnbindCommand`

- `Cancel`
  - 关闭子菜单

这样菜单内 hotbar 在非鼠标路径下也能闭环。

### Step 4: 角色信息区改为真实数据绑定

当前角色区基本还是静态文本，占位性质较强。  
本阶段改成“真实数据 + 占位表现”：

- 玩家名称：从 `NameComponent` 读取
- 头像：先保留静态占位或固定图
- 等级/职业：无稳定数据则继续占位文本
- 金币：若当前运行时无统一来源，则继续静态占位，不额外造系统
- 8 个装备槽：继续只读占位，不做实际装备逻辑

目标是先把右侧区域从硬编码文案改成可绑定结构，而不是提前进入装备系统。

### Step 5: 删除旧 InventoryUI，并清理 GameScene 的旧库存接线

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

### Step 6: 移除分页模型 `active_page_`

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
- hotbar 子菜单 `Activate / Use / Unbind`
- 删除旧 `InventoryUI` 后探索态 `HotbarUI` 仍正常工作
- 排序后 hotbar 映射仍正确
- 存档 / 读档后 inventory + hotbar 状态仍正确

## 待办

- [ ] Step 1: 为 `InventoryMenuScene` 增加操作子菜单状态机
- [ ] Step 2: 接通 backpack slot 的 `Use / Equip(Bind) / Discard / Cancel`
- [ ] Step 3: 接通 hotbar slot 的 `Activate / Use / Unbind / Cancel`
- [ ] Step 4: 将角色信息区改成真实数据绑定 + 占位显示
- [ ] Step 5: 删除旧 `InventoryUI` 与旧 inventory RML/RCSS
- [ ] Step 6: 清理 `GameScene` 中旧 inventory UI 集成，保留探索态 `HotbarUI`
- [ ] Step 7: 精简 `HotbarUI` 对旧 `InventoryUI` 的耦合
- [ ] Step 8: 删除 `active_page_` 与相关分页命令/事件字段
- [ ] Step 9: 新增 `InventorySortCommand`，并在排序后修复 hotbar 映射
- [ ] Step 10: 更新 `CMakeLists.txt` 并完成整体测试

## 非目标

以下内容不属于本阶段：

- 移除探索态 `HotbarUI`
- 移除 `HotbarComponent` / `HotbarSystem`
- 真正的 RPG 装备系统
- 装备槽可穿戴/可卸下逻辑
- 独立的角色成长面板（等级、职业、属性）
- 金币系统的完整运行时接入

## 备注

本计划已经按你的最新确认更新：

- **探索态 HotbarUI 保留**
- **菜单内 hotbar 区域保留**
- **一切以当前代码为准**
