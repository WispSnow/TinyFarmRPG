# Milestone D / Stage 4: Sell 流程与背包同步细化计划

## 实现思路

- Stage 4 不新建第二个商店 Scene，继续直接扩展当前已经落地的 `ShopMenuScene`。
- 当前代码里 `ShopTransactionService::previewSell() / commitSell()` 已经存在，Stage 4 的重点不再是重新设计交易核心，而是把 Sell UI、slot 语义和 `InventoryChanged -> HotbarSystem` 链路真正接通。
- Sell 模式的真相继续直接锚定玩家当前 `InventoryComponent`，不引入“商店镜像背包”或按 `item_id` 聚合后的第二套运行时库存。
- 推荐锁定为“一个非空 inventory slot 对应一个 sell entry”：
  - 列表展示顺序沿用背包槽位顺序
  - 每个条目携带真实 `slot_index`
  - 卖出预检与提交都直接使用这个 `slot_index`
- 不建议在 Stage 4 把多个同 item 的 stack 聚合成一个 sell 行：
  - 这会打断当前 `previewSell(slot_index)` 的 exact-slot 语义
  - 也会让 Hotbar 的 inventory-slot 引用关系变得更难推断
- Sell 资格继续只由 `ShopCatalog::sell_rules` 决定，不从 `ItemCategory`、`description`、`battle_use` 等隐式推断。
- 推荐保留“显示但禁用”的最小策略：
  - 背包中所有非空槽位都可出现在 Sell 列表
  - 未配置 sell rule 的条目显示为 disabled / `--`
  - 允许选中 disabled 条目查看详情，但 `Sell` 按钮保持禁用
- 数量模型建议直接复用 Stage 3 已落地的 `+/-` 交互，不再为 Sell 额外引入 `1 / all` 的第二套 UI：
  - `menu_up/down` 切换当前条目
  - `menu_left/right` 调整卖出数量
  - `menu_confirm` 提交当前交易
  - `menu_cancel` 关闭商店
- Sell 模式下的数量上限必须绑定到“当前选中槽位的 stack.count”，而不是同 item 的全背包总持有数。
- 若当前选中槽位在提交后被清空：
  - Scene 应重新构建 Sell 列表
  - 尝试选中“原位置附近的下一个有效条目”
  - 若列表为空，则清空详情区并禁用 Sell 按钮
- Stage 4 不为 Hotbar 写专门补丁：
  - 继续依赖 `InventoryDomainService::removeItem()` 发出的 `InventoryChanged`
  - 由现有 `HotbarSystem` 订阅后清理或更新快捷栏映射
- Stage 4 不新增 save schema，也不引入 merchant 库存持久化；卖出后的持久化仍然完全复用现有 wallet / inventory save 流程。
- Buy / Sell mode 的切换推荐先做成 Scene 内的显式 UI toggle，不在本阶段新增新的输入 action。
- 也就是说，Stage 4 的最低范围是“当前 mode 内键盘/手柄完整可用”，而 Buy / Sell mode toggle 若暂时只通过 UI 事件切换也可接受；若后续明确要求“纯手柄无鼠标切 mode”，再单独收敛一个局部 focus/substate 方案。
- Stage 4 还需要把当前偏 Buy 语境的状态文案与失败文案改成 mode-aware，不能继续让 Sell 路径复用 `"Purchase failed."` 这类只适用于购买的提示。

## 需要新增的文件

- `plans/jrpg-milestone-d-stage4-sell-flow-and-inventory-sync.md`
- `tests/game/shop_menu_sell_flow_test.cpp`

说明：

- `src/game/ui/shop_menu_support.h/.cpp`
- `src/game/scene/shop_menu_scene.h/.cpp`
- `ui/rmlui/scenes/shop_menu.rml`
- `ui/rmlui/scenes/shop_menu.rcss`
- `tests/game/shop_transaction_service_test.cpp`
- `tests/game/shop_menu_scene_smoke_test.cpp`
- `tests/game/inventory_hotbar_consistency_test.cpp`

以上文件以扩展现有实现为主，不另起新 Scene 或新的商店 domain service。

## 实现步骤

### Step 1. 扩展 Sell ViewModel 与 helper

- 在 `shop_menu_support.h/.cpp` 中新增 `ShopSellEntryViewModel`。
- 建议字段至少包含：
  - `index`
  - `slot_index`
  - `item_id_hash`
  - `icon_decorator`
  - `item_name`
  - `count_text`
  - `price_text`
  - `is_selected`
  - `is_disabled`
- 提供与 Buy 对齐的最小注册接口，例如：
  - `registerShopSellEntryViewModelType(Rml::DataModelConstructor&)`
- 由于当前 `ShopMenuScene::initUI()` 使用 `data_types_registered_` 把 data type 注册限制在单次窗口内，Sell 的 type 注册与 `RegisterArray<decltype(sell_entries_)>()` 必须和 Buy 类型一起放进同一个注册分支里，再统一置位。
- 提供从真实背包槽位填充条目的 helper，例如：
  - 根据 `InventoryComponent::slot(i) + ItemCatalog + ShopCatalog::findSellRule()` 生成一条 Sell view model
- 建议同时补一层“槽位可卖数量”解析 helper，例如：
  - `resolveSellQuantityUiMax(const ItemStack&)`
- 这里的 helper 目标是把 Scene 中的“slot -> label / icon / price / disabled”逻辑尽量抽薄，而不是重新发明 inventory 真相。

### Step 2. 给 ShopMenuScene 增加 Buy / Sell mode 状态

- 在 `ShopMenuScene` 中新增最小 mode state，例如：
  - `enum class ShopMenuMode { Buy, Sell }`
  - `ShopMenuMode current_mode_{ShopMenuMode::Buy}`
  - `std::vector<ShopSellEntryViewModel> sell_entries_`
  - `int selected_sell_index_{0}`
  - `ShopSellPreview active_sell_preview_{}`
- 同时补齐最小 mode 绑定变量，供 RML 做列表与按钮显隐，例如：
  - `bool is_buy_mode_{true}`
  - `bool is_sell_mode_{false}`
  - `bool has_sell_entries_{false}`
  - `bool sell_enabled_{false}`
- 同时建议补一些 mode-aware 文案字段，避免 RML 文本被硬编码死：
  - `mode_title_` 或 `primary_action_text_`
  - `detail_owned_label_`
  - `list_empty_text_`
- 推荐补齐一组和 Sell 对应的刷新函数：
  - `rebuildSellEntries()`
  - `refreshSelectedSellEntry()`
  - `refreshSellPreview()`
  - `switchMode(ShopMenuMode next_mode)`
- 现有 `refreshStatusText()` 需要升级为 mode-aware：
  - Buy mode 读取 `active_buy_preview_`
  - Sell mode 读取 `active_sell_preview_`
  - empty state 文案也要按当前 mode 选择
- 现有 `onMenuUpPressed() / onMenuDownPressed() / onMenuLeftPressed() / onMenuRightPressed() / onMenuConfirmPressed()` 也需要按 `current_mode_` 分派到 Buy 或 Sell 路径，而不是继续硬编码只操作 Buy。
- `refreshAll()` 在 Stage 4 中至少要保证 gold、当前 mode 详情、当前 mode 列表与另一 mode 的可见状态一致；推荐不要在每次光标移动时无条件重建两份列表，而是把“当前 mode 立即刷新 + 非当前 mode 标记 stale，在 `switchMode()` 时再重建”作为优先实现方向。

### Step 3. 锁定 Sell 列表来源与 slot 语义

- Sell 列表只从玩家当前 `InventoryComponent` 派生，不读 `HotbarComponent`，也不持有 detached 副本。
- 推荐规则：
  - 空槽位不进入 Sell 列表
  - 非空槽位全部进入 Sell 列表
  - 若无 sell rule，则条目 `is_disabled = true` 且 `price_text = "--"`
- 每个 sell entry 必须稳定持有真实 `slot_index`。
- `selected_sell_index_` 是“列表索引”，`slot_index` 是“真实 inventory 槽位”；两者不要混用。
- Sell preview / commit 必须始终走：
  - `previewSell(player, item_id_hash, quantity, slot_index)`
  - `commitSell(player, item_id_hash, quantity, slot_index)`
- 若 inventory 在交易后变化导致当前 `slot_index` 已失效：
  - Scene 重建 Sell 列表后重新选择最近有效条目
  - 不要保留悬空索引

### Step 4. 将 Sell preview / commit 绑定到详情区

- Sell 模式下，详情区最少展示：
  - 物品名
  - 描述
  - 当前槽位数量
  - 单价
  - 总价
  - 交易后金币
- 与 Buy 一样，Sell 详情继续使用 Scene 本地状态，不直接把自由字符串塞回 domain service。
- Sell 模式下数量规则建议锁定为：
  - 最小值 `1`
  - 非堆叠物固定 `1`
  - 堆叠物最大值为当前槽位 `stack.count_`
  - 切换 Sell 条目后数量重置为 `1`
- `detail_owned_text` 可以继续复用同一个 data binding，但其语义必须显式区分：
  - Buy mode 展示全背包同 item 的聚合持有数
  - Sell mode 展示当前选中 `slot_index` 的 `stack.count_`
  - Sell mode 不应复用 `countOwnedItems()` 这种跨槽位聚合结果
- 若当前条目不可卖、槽位不匹配或数量超过当前 stack：
  - `active_sell_preview_` 失败
  - `Sell` 按钮禁用
  - Scene 本地状态文案给出明确反馈
- 当前偏 Buy 语境的 `formatFailureText()` 需要改造成 mode-aware，或直接拆成 Buy / Sell 两套 failure formatter，避免 `ItemNotSellable / SlotMismatch / InsufficientItemCount` 仍落成购买语境。
- Stage 4 推荐最小失败文案映射至少覆盖：
  - `ItemNotSellable` -> 当前物品不可出售
  - `SlotMismatch` -> 当前槽位已变化
  - `InsufficientItemCount` -> 当前数量不足
  - `InvalidQuantity` -> 数量无效
  - `InvalidPlayer / InvalidItem` -> 操作无效

### Step 5. 实现 mode toggle 与最小 Sell UI

- `shop_menu.rml / rcss` 在 Stage 4 中继续复用 Stage 3 布局，不另起第二张页面。
- 推荐新增顶部或左侧的 `Buy / Sell` toggle：
  - `switch_mode_buy`
  - `switch_mode_sell`
- RML 需要显式绑定 mode 变量，例如 `is_buy_mode / is_sell_mode`，再配合 `has_buy_entries / has_sell_entries` 做 `data-if` 或等价显隐控制，保证 Buy / Sell 列表互斥渲染。
- 当前 mode 只渲染自己的列表：
  - Buy mode 渲染 `buy_entries`
  - Sell mode 渲染 `sell_entries`
- 右侧 detail panel 尽量复用现有字段与布局，不为 Sell 再复制一套 detail 结构。
- 底部主按钮应切为 mode-aware：
  - Buy mode 显示 `Buy`
  - Sell mode 显示 `Sell`
- Stage 4 重点是“功能接通 + 状态清晰”，不追求完整视觉 polish。

### Step 6. 锁定 Hotbar / Inventory 同步不变量

- Stage 4 必须明确依赖现有链路：
  - `ShopTransactionService::commitSell()`
  - `InventoryDomainService::removeItem()`
  - `InventoryChanged`
  - `HotbarSystem::onInventoryChanged()`
- `ShopMenuScene` 不新增 `HotbarSyncCommand` 或商店专用 hotbar 修复逻辑。
- 需要重点覆盖两类不变量：
  - 若卖出后当前槽位数量减少但未清空，Hotbar 绑定继续存在且 count 正确
  - 若卖出后当前槽位被清空，Hotbar 对应映射被清理或按现有系统规则更新
- 若未来发现 `removeItem(slot_index)` 与 Hotbar 语义存在新的边界问题，应优先修正 domain / system，不要把补丁塞回商店 Scene。

### Step 7. 补齐 Stage 4 测试

- `shop_menu_sell_flow_test.cpp` 重点锁 Sell UI/Scene 的 source 或 helper 行为：
  - `ShopSellEntryViewModel` 填充逻辑
  - `ShopMenuScene` 使用 `previewSell / commitSell`
  - 存在 `slot_index` 绑定
  - mode toggle 与 Sell 列表绑定名存在
- 继续扩展现有 `shop_transaction_service_test.cpp`，建议补强：
  - disabled / unsellable item preview
  - 指定 `slot_index` 不匹配时整笔失败
  - 指定 `slot_index` 数量不足时整笔失败
  - Sell success 后 `final_gold_after` 正确
- 继续扩展现有 `inventory_hotbar_consistency_test.cpp`，建议新增一条 shop-sell 相关回归：
  - 某个 inventory slot 已绑定 hotbar
  - 通过 `ShopTransactionService::commitSell()` 卖空该槽位
  - 验证 `HotbarSystem` 通过 `InventoryChanged` 自动清理映射
- `shop_menu_scene_smoke_test.cpp` 继续保留，并补充：
  - `switch_mode_sell`
  - `sell_entries`
  - `previewSell / commitSell`
  - `is_buy_mode / is_sell_mode`
  - `slot_index` / exact-slot 相关 source 断言

## ToDo

- [ ] 新增 `ShopSellEntryViewModel` 与 Sell helper
- [ ] 给 `ShopMenuScene` 增加 `ShopMenuMode`、Sell 列表状态与 `ShopSellPreview`
- [ ] 锁定 Sell 列表的 exact-slot 语义，不做按 item 聚合
- [ ] 将 `previewSell()` 绑定到详情区与 Sell 按钮禁用态
- [ ] 将 `commitSell()` 接入 Sell 按钮与确认输入
- [ ] 扩展 `shop_menu.rml / rcss` 支持 Buy / Sell mode toggle
- [ ] 补齐 Sell UI、transaction service 与 hotbar sync 回归测试

## 疑问

- 暂无必须先澄清的问题；当前推荐假设是“Stage 4 先不新增 menu action，Buy / Sell mode switch 先走 Scene 内显式 UI toggle”。如果后续目标改成“纯键盘/手柄无鼠标切换 mode”，再单独把局部 focus/substate 提升为必做范围。
