# Milestone D / Stage 5: UI 收尾、集成回归与文档封版计划

## 实现思路

- Stage 5 不再引入新的商店玩法规则，也不继续扩展 `ShopCatalog` / `ShopTransactionService` 的经济语义；本阶段目标是把 Milestone D 从“功能已接通”收尾到“可稳定验收”。
- 当前 `ShopMenuScene`、`ShopInteractionSystem`、`ShopTransactionService` 与 `InventoryChanged -> HotbarSystem` 主链路已经存在，Stage 5 的重点应收敛在：
  - 纯键盘 / 手柄路径下的完整商店可操作性
  - 更接近真实使用链路的集成测试与存档回归
  - Milestone D 文档、索引与验收口径的最终同步
- Stage 5 推荐继续坚持“一个商店 Scene + 显式构造注入 + Scene 持有输入状态”的现有架构，不切回 RmlUi 自行托管全部导航，也不新建第二套 shop scene。
- UI 收尾的重点是“交互完整性与语义清晰”，不是视觉重做：
  - 继续复用当前 `shop_menu.rml / rcss`
  - 保持 neutral naming，不再残留 `buy_*` 命名承载 sell 语义
  - 空状态、成功 / 失败提示、列表标题、主操作按钮文案都要与当前 mode 一致
- Stage 4 留下的一个范围收敛是“Buy / Sell mode toggle 可先通过显式 UI 事件切换”；Stage 5 建议把它提升为正式验收项：
  - 玩家不依赖鼠标
  - 只用现有 `menu_up/down/left/right/confirm/cancel`
  - 就能完成 `切换 Buy / Sell -> 选择条目 -> 调整数量 -> 提交 -> 离开`
- 推荐不要把纯导航能力完全交给 RmlUi 的自动 focus 规则，而是在 `ShopMenuScene` 内引入一个最小 focus/substate：
  - 例如 `ModeToggle / EntryList / Quantity / PrimaryAction`
  - 由 scene 继续统一消费 `menu_*`
  - RML 只负责展示当前 focus / selected / disabled 态
- Stage 5 的测试重点应从“source wiring 已存在”进一步提升到：
  - mode 切换与输入分派的 runtime 行为
  - 商店交易后的 save capture/apply 或 roundtrip 不变性
  - merchant 打开 shop 后完成交易再退出的关键路径稳定性
- 由于 Milestone D 明确“不新增商店存档 schema”，Stage 5 应显式补一条回归证明：
  - buy / sell 产生的 wallet / inventory 变化
  - 经过现有 save capture/apply
  - 不需要任何新字段也能正确保留
- 文档收尾建议与代码同步完成：
  - 更新 Milestone D 索引状态
  - 更新总里程碑文档中 Milestone D 的进度
  - 若 `docs/overview.md` 有玩法清单，也同步补上“任务 + 商店”当前已落地范围
- Stage 5 不包含：
  - 动态库存、折扣、会员价、声望价
  - 装备对比、试穿、武器 / 防具专门 UI
  - 多商店统一价格面板或经济重平衡
  - 将商店进一步抽成通用 menu framework

## 需要新增的文件

- `plans/jrpg-milestone-d-stage5-ui-docs-and-tests.md`
- `tests/game/shop_menu_navigation_test.cpp`
- `tests/game/shop_save_roundtrip_test.cpp`

说明：

- `src/game/scene/shop_menu_scene.h/.cpp`
- `src/game/ui/shop_menu_support.h/.cpp`
- `ui/rmlui/scenes/shop_menu.rml`
- `ui/rmlui/scenes/shop_menu.rcss`
- `tests/game/shop_menu_scene_smoke_test.cpp`
- `tests/game/shop_menu_buy_flow_test.cpp`
- `tests/game/shop_menu_sell_flow_test.cpp`
- `tests/game/shop_interaction_system_test.cpp`
- `tests/game/game_scene_runtime_assembly_test.cpp`
- `plans/jrpg-milestone-d-shop-index.md`
- `plans/jrpg-milestones.md`
- `docs/overview.md`

以上文件以补完现有商店闭环为主，不新增新的 gameplay domain service。

## 实现步骤

### Step 1. 锁定商店最终验收口径

- 先把 `ShopMenuScene` 当前已经落地的 Buy / Sell 行为整理成稳定验收口径：
  - 可以打开合法 merchant shop
  - Buy / Sell 都能完成 preview + commit
  - hotbar 同步不变量已成立
  - close / cancel 语义稳定
- Stage 4 已经完成了主要命名清理：
  - `buy_confirm -> confirm_trade`
  - `shop-buy-button -> shop-primary-action-button`
  - `shop-buy-entry-* -> shop-entry-*`
- 因此 Step 1 不再默认安排新的命名重构工作，而是先用 `rg` 确认是否还有残余歧义；若 grep 已干净，本步骤只负责确认验收口径。
- `buy_entry_select / sell_entry_select` 保持现状即可：
  - 它们分别服务于各自 mode 的列表点击
  - 没有“一名多义”的问题
  - Stage 5 不建议为了“绝对中性命名”继续改它们
- 本步骤不改变经济规则，只锁定“Milestone D 到什么程度算完成”。

### Step 2. 给 ShopMenuScene 增加纯键盘 / 手柄可达的 focus/substate

- 推荐的正式方案仍然是显式 focus/substate，而不是继续把所有 `menu_*` 都硬编码成“列表 / 数量 / 提交”三合一逻辑。
- 在 `ShopMenuScene` 中引入一个最小 focus 状态，例如：
  - `enum class ShopMenuFocusArea { ModeToggle, EntryList, Quantity, PrimaryAction }`
- 初始 focus 明确锁定为 `EntryList`：
  - 打开商店后直接可浏览当前 mode 的商品
  - 与当前鼠标使用习惯一致
  - 避免玩家每次进店都要先离开 mode toggle
- 推荐先把 focus 转移表写死，再开始实现。最小建议如下：
  - `ModeToggle`
  - `menu_left/right`：切换 `Buy / Sell`
  - `menu_down` 或 `menu_confirm`：进入 `EntryList`
  - `menu_up`：不处理
  - `EntryList`
  - `menu_up/down`：切换当前条目
  - `menu_left`：进入 `ModeToggle`
  - `menu_right`：进入 `Quantity`
  - `menu_confirm`：进入 `PrimaryAction`
  - `Quantity`
  - `menu_left/right`：调整数量
  - `menu_up`：回到 `EntryList`
  - `menu_down` 或 `menu_confirm`：进入 `PrimaryAction`
  - `PrimaryAction`
  - `menu_confirm`：提交当前 trade
  - `menu_up`：回到 `Quantity`
  - `menu_left`：回到 `EntryList`
  - `menu_right/down`：不处理
- 当前六个输入处理函数都要按 focus state 重写分派，而不是只改局部逻辑：
  - `onMenuUpPressed()`
  - `onMenuDownPressed()`
  - `onMenuLeftPressed()`
  - `onMenuRightPressed()`
  - `onMenuConfirmPressed()`
  - `onMenuCancelPressed()`
- 组织方式建议对齐 `BattleScene::MenuState` 的模式：先判当前 focus/substate，再分派到 `select* / adjustQuantity / switchMode / confirm*`。
- Scene 继续作为输入 owner：
  - 不把关键逻辑下放给 RmlUi 自动焦点
  - RML 只通过 data model 展示 focus / selected / disabled
- 一个更轻量的备选方案可以记录为 fallback：
  - 不引入四区 focus
  - 只增加最小 mode-switch 键盘入口
  - 但它仍必须保证“不依赖鼠标切换 Buy / Sell，也不依赖鼠标触发主操作按钮”
- 该 fallback 可以作为时间压力下的降级路径，但不作为 Stage 5 的首选方案。

### Step 3. 补齐 ShopMenuScene 的 runtime 行为测试

- 在现有 source/smoke test 之外，新增更偏 runtime 的测试文件，例如：
  - `shop_menu_navigation_test.cpp`
- 这一组测试不建议停留在 source-reading 层；Stage 5 的目标是验证输入分派逻辑，而不是只验证字符串 wiring。
- 推荐做法是：
  - 将 focus 转移与 `menu_* -> scene action` 的决策逻辑抽成一个小的纯 helper，优先放在 `shop_menu_support.h/.cpp` 或等价的 scene-local helper 中
  - `shop_menu_navigation_test.cpp` 直接测试这个 helper 的状态转移与动作输出
  - `ShopMenuScene` runtime test 只再补一层最小集成，验证 helper 已被 scene 接入
- 不推荐默认引入 `friend class` 或大面积开放 private 状态；优先通过“纯输入决策 helper + scene 薄集成”获得可测性。
- 推荐覆盖：
  - 初始 focus / mode 状态
  - 纯 `menu_left/right` 可切换 Buy / Sell
  - 切换 mode 后详情区和主按钮文本同步变化
  - 切换条目后数量重置为 `1`
  - disabled 条目 / 空列表下 confirm 不会错误提交
  - `menu_cancel` 始终可关闭 scene，不被 focus 卡住
- 这类测试不要求上 Playwright，也不要求完整复刻 `ShopInteractionSystemTest` 级别的 UI fixture。

### Step 4. 补齐商店与存档的集成回归

- 新增 `shop_save_roundtrip_test.cpp` 或等价测试。
- 目标是证明：
  - buy 后的 gold / inventory 变化可通过现有 save capture/apply 保留
  - sell 后的 gold / inventory 变化同样成立
  - 不需要新增 merchant save schema
- 推荐最小覆盖：
  - 玩家购买后保存，再恢复到新 registry，wallet / inventory 正确
  - 玩家卖出后保存，再恢复到新 registry，wallet / inventory 正确
- 当前项目已经有 `quest_save_roundtrip_test.cpp` 这类完整 fixture 模式；Stage 5 推荐直接复用它的写法，即使存在一定重复也可接受。
- 也就是说，本阶段默认选择“有意识地复制 save roundtrip fixture 模式”，而不是额外抽一个共享 `SaveRoundtripTestFixture` 基类。
- 若后续 save roundtrip 测试继续增长，再单独做 fixture 抽取；不把这件事抬进 Milestone D 收尾范围。

### Step 5. 补强 merchant -> shop -> trade 的关键路径集成测试

- 现有 `ShopInteractionSystemTest::ValidMerchantPushesShopMenuScene` 已经覆盖了 merchant -> push scene 这条主链，因此 Stage 5 不需要再重复造一份同层级测试。
- 默认推荐的补强方式是“在现有测试基础上加深装配与 scene invariant”，而不是强行在集成测试里驱动完整 buy/sell UI：
  - merchant 交互成功 push `ShopMenuScene`
  - runtime services 装配齐全
  - scene 构造参数与 injected services 不为空
  - scene 仍保持可关闭 / 可 pop
- 若直接驱动 scene 内交易流程需要引入完整 RmlUi runtime、私有方法访问或更重的 fixture，则不作为 Step 5 的默认目标。
- 换句话说，Stage 5 的集成测试默认锁“入口与装配稳定”，不默认锁“在集成测试里完成整笔交易”。

### Step 6. 更新 Milestone D 文档与项目概览

- `plans/jrpg-milestone-d-shop-index.md`：
  - 将 Stage 5 细化计划挂入索引
  - 完成后同步各阶段状态
- `plans/jrpg-milestones.md`：
  - 更新 Milestone D 当前进度
  - 明确 Milestone D 已覆盖的最小玩法边界
- `docs/overview.md`：
  - 若有玩法总览，补上当前已落地的 merchant / buy / sell / task 闭环
- 文档更新应与真实代码行为一致，不提前写入未实现能力。

### Step 7. 做 Milestone D 的最终回归清单

- Stage 5 结束时建议整理一组最小回归命令，作为 Milestone D 的封版清单：
  - `ShopTransactionService` 相关
  - `ShopMenuScene` source/runtime 相关
  - `ShopInteractionSystem` 相关
  - `InventoryHotbarConsistency` 相关
  - save roundtrip 相关
- 若后续 Milestone E / 战斗系统改动触碰 inventory、scene stack、input context，这组清单可以直接作为商店回归入口继续复用。

## ToDo

- [ ] 锁定 Stage 5 的 Milestone D 验收口径，并确认命名残余是否已清空
- [ ] 给 `ShopMenuScene` 增加纯键盘 / 手柄可达的 focus/substate，并写死最小转移表
- [ ] 将输入分派决策抽成可测试 helper，并新增 `shop_menu_navigation_test.cpp`
- [ ] 新增 `shop_save_roundtrip_test.cpp`，用现有 roundtrip fixture 模式验证商店交易后的存档闭环
- [ ] 在现有 `ShopInteractionSystemTest` / runtime assembly 测试上补强 scene 入口与装配 invariant
- [ ] 更新 `jrpg-milestone-d-shop-index.md`、`jrpg-milestones.md` 与 `docs/overview.md`
- [ ] 整理 Milestone D 的最终回归清单

## 疑问

- 暂无必须先澄清的问题；当前推荐假设是“Stage 5 把纯键盘 / 手柄可完成 Buy / Sell 全流程提升为正式验收项”。如果你更希望 Stage 5 只做测试与文档，不再扩输入可达性，我也可以按那个更保守的范围收缩。
