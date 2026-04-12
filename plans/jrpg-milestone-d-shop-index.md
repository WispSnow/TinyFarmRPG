# Milestone D: 商店 UI 与买卖规则索引计划

## Context

Milestone C 已完成“接任务 / 杀怪计数 / 回 NPC 交付”的最小任务闭环，当前项目已经具备几条对 Milestone D 很关键的可复用边界：

- `PlayerWalletComponent` 已是稳定的金币运行时真相，`SaveService::capture/apply()` 也已接通金币存档
- `InventoryDomainService` 已负责真实物品增减，并会发出 `InventoryChanged / InventoryFullEvent`
- `InventoryMenuScene` 已经验证了“覆盖式菜单 Scene + RmlDocumentController + data model + 事件绑定”的 UI 组织方式
- `QuestInteractionSystem` 与 `RestSystem` 已经证明：`InteractCommand` 很适合作为世界交互扩展点，订阅者可以负责通知或 `PushSceneEvent`
- `EntityBuilder` 已支持从地图 actor 实例属性附加特殊组件（当前已有 `quest_offer_id`）
- `InteractionSystem::chooseFacingTarget()` 已形成“特殊交互对象优先于普通对话”的可扩展策略

但 Milestone D 需要的关键能力仍然完全缺失：

- 还没有独立的商店静态目录，无法定义 merchant / 商品列表 / 价格 / 售卖规则
- 还没有 merchant 运行时 owner，世界里的 NPC 无法稳定打开 shop
- 还没有保证“金币 + 背包”原子交易的 domain service
- 还没有 Shop Scene、Buy / Sell UI、数量确认与失败反馈
- `ItemCatalog` 仍只负责物品身份、图标、描述与 use/battle_use，不包含买卖价格
- `RpgCatalog.manifest.features.shop` 目前只是被解析，还没有运行时消费者

因此，Milestone D 建议只解决这几件事：

- 让玩家可以从 merchant NPC 打开商店
- 让商店可以展示一份静态配置的 buy list
- 让玩家可以消耗金币购买商品并写回真实背包
- 让玩家可以按规则出售背包物品并获得金币
- 让整个商店过程在 UI、存档和测试层面形成稳定闭环

## 范围

### 本阶段包含

- 独立 Shop 目录：merchant / shop definition、buy entries、sell rules、最小文案
- Merchant 运行时组件与基于 `InteractCommand` 的商店入口
- 独立 `ShopMenuScene` 或等价覆盖式商店 Scene
- Buy / Sell 交易规则、数量规则、失败反馈
- 金币与背包写回，复用现有 `PlayerWalletComponent + InventoryDomainService`
- 对应的单元测试、交互测试与 UI smoke test

### 本阶段不包含

- 装备商店、武器店、护甲对比、试穿预览
- 动态折扣、税率、声望、讨价还价、会员价
- 多货币、代币商店、以物易物
- 限量库存、每日刷新、补货计时、世界级经济系统
- 同一 NPC 同时承载“普通对话 + 任务 + 商店”多态交互状态机
- 商店剧情演出、复杂对话分支、地图指引
- 经济系统的全面重平衡

## 实现思路

Milestone D 不适合直接在 `ItemCatalog` 上追加 `buy_price / sell_price`，也不适合把商店硬塞进 `InventoryMenuScene`。

原因：

- `ItemCatalog` 当前职责是“物品身份与效果目录”，而“这个商店卖什么、卖多少钱、能不能回收”属于经济/merchant 层规则
- `InventoryMenuScene` 的模型是“只看玩家自己的背包与快捷栏”，商店还需要 merchant 视角、买卖模式、数量确认和交易反馈
- `InventoryDomainService::addItem()` 允许在背包不足时部分接受；商店购买若直接调用它，极易出现“金币已扣、物品只进了一部分”的脏状态
- 商店入口已经天然适合复用 `InteractCommand -> 订阅者 -> PushSceneEvent` 这条路径，没有必要把 `GameScene` 变成 merchant 入口总控

因此更稳的推进方式是：

1. 先建立独立 `ShopCatalog` 与交易原子性边界
2. 再接 merchant 入口与 `ShopMenuScene` 路由
3. 再先做 Buy 闭环，而不是一上来追求“大而全商店页面”
4. 最后补 Sell 流程、测试和 UI 收尾

同时建议锁定这些边界：

- Shop 数据目录独立于 `ItemCatalog` 与 `RpgCatalog`，推荐新增 `ShopCatalog` 加载例如 `assets/data/shops.json`
- merchant runtime truth 放在地图实例实体上，例如 `MerchantComponent`，不要把 `shop_id` 写死到全局 actor blueprint
- `ShopInteractionSystem` 负责 merchant 交互入口，并通过 `PushSceneEvent` 打开 `ShopMenuScene`
- `DialogueSystem` 在 Milestone D 中应显式跳过带 `MerchantComponent` 的实体，避免商店和普通对话同时消费同一次 `InteractCommand`
- Milestone D 继续坚持“一个实体只有一个特殊交互 owner”的规则；`MerchantComponent` 不与 `QuestGiverComponent` 并行生效
- `InteractionSystem::chooseFacingTarget()` 在本阶段要显式加入 merchant 分支，推荐优先级为 `Merchant > QuestGiver > Dialogue NPC > Chest > Rest`
- 由于当前 `chooseFacingTarget()` 通过 `if (...) { ...; continue; }` 实现类型互斥，merchant 的扫描分支必须插在 `QuestGiverComponent` 分支之前，否则优先级会被错误反转
- `ShopMenuScene` 应遵循当前 RmlUi 集成方式：`RmlDocumentController + data model + event callback + dirty mark`，不回退到旧路径
- `ShopMenuScene` 的 gameplay 依赖应走显式构造注入，优先对齐 `InventoryMenuScene(player, item_catalog, ...)` 这类模式；`Context` 只继续承担共享基础设施（RmlUi、输入、dispatcher 等）
- 商店交易必须经由独立 `ShopTransactionService` 或等价 domain service；Scene 不能直接自己扣钱、加物品、删物品
- Buy / Sell 操作必须是原子的：要么整笔成功，要么整笔失败；不接受“部分成功再靠 UI 补救”
- 推荐默认 merchant stock 为“静态无限库存”，Milestone D 不引入商店库存存档；若未来要做限量/补货，单独立项
- Sell 资格不要靠 `ItemCategory` 猜测，建议使用显式 sell rule；未配置 sell rule 的 item 默认不可卖
- 卖出流程应绑定玩家当前真实背包槽位或等价稳定来源，避免“界面选的是这个堆叠，removeItem 却从别的槽扣”的歧义
- 当前 `InventoryDomainService::removeItem(target, item_id, count, slot_index)` 在指定槽位时若物品不匹配或数量不足，不会抛出显式错误，而是通过 `accepted/rejected` 表达结果；Sell 交易层必须在 commit 前完成精确预检，不能把它直接当成“精确扣除指定槽位”的原子原语
- 若购买或卖出导致 `InventoryChanged`，应复用现有事件链路让 Hotbar 等依赖自动同步，不另开特殊写回路径

## 阶段索引

### Stage 1: Shop 目录与交易核心

目标：

- 让商店从“概念需求”升级为“有静态定义、有价格规则、有原子交易边界”的真实系统

本阶段聚焦：

- 新增独立 `ShopCatalog` 或等价目录类，加载例如 `assets/data/shops.json`
- 定义最小 shop 数据模型：`shop_id / title / greeting / buy_entries`
- `buy_entry` 至少包含 `item_id / buy_price`，可选 `stock`
- 定义全局 sell rule 表，例如 `item_id -> sell_price`
- 目录加载时校验 `item_id` 必须能在 `ItemCatalog` 中找到
- 新增 `ShopTransactionService` 或等价 domain service
- 锁定 buy/sell 原子性、数量上限、失败原因返回结构
- 锁定 preview / validation 返回语义，至少覆盖 `can_afford / has_space / failure_reason / final_gold_after`，必要时补充 `requested_quantity / resolved_quantity / total_price`
- 锁定当前全局 sell rule 的语义：Milestone D 中同一 item 的卖出价在所有商店一致，后续若要做 per-shop sell rule 再单独扩展
- 明确加载期价格校验策略：若某 item 同时存在 buy_price 与 sell_price，且 `sell_price > 最低 buy_price`，至少给出 warn，避免配置错误制造刷金通道
- 明确 Sell 对指定槽位扣除的预检规则，确保 Stage 4 不会建立在模糊的 `removeItem(slot_index)` 语义上
- 锁定 Milestone D 默认不引入持久化 merchant stock，因此无需新增 save schema

推荐最小方案：

- `ShopCatalog` 同时承载“每个 shop 的 buy list”和“全局 sell rules”
- `buy list` 只定义商店卖给玩家的商品；sell 不要求 merchant 自己持有对称库存
- 未出现在 sell rule 表中的物品一律视为不可卖
- `ShopTransactionService` 需要先做 preview/validation，再执行 commit
- preview 结果优先做成稳定结构，至少让 UI 能直接读取“买得起/装得下/失败原因/交易后金币”
- 购买预检应基于背包快照或复制出的 `InventoryComponent` 模拟结果，保证“背包装不下”时不会先扣钱
- 卖出提交前也要先校验指定 inventory slot 的 `item_id + count` 是否完全匹配请求；若不匹配，应整笔失败而不是做部分扣除
- 工具类或 `stack_limit == 1` 的物品数量上限默认收敛为 1
- Milestone D 先接受“全局 sell rule = 全商店统一卖出价”的简化，后续若需要再扩展为 per-shop sell rule

原因：

- 价格和 merchant assortments 不是物品身份层属性，独立目录更利于后续扩展不同商店
- 若 sell 资格隐式从 category 推断，后续“任务物品不可卖 / 稀有材料可卖但价格特例”会很快失控
- 当前背包 domain service 允许 partial accept，商店必须先立好交易原子性边界
- 当前 `removeItem(slot_index)` 也是“尽量扣除并返回 rejected”，因此 Sell 的 exact-slot 语义必须由交易层补齐
- 无限库存足够覆盖 Milestone D 的玩法闭环，同时能避免过早引入库存存档和刷新规则
- 加载期的价格告警可以更早暴露“卖价高于买价”的数据错误，降低后续调平和排错成本

阶段交付物：

- 可加载的 shop 静态目录
- 可复用的交易核心与错误语义
- 不新增 save schema 的前提下可闭环的商店规则基础

建议后续细化文档：

- `plans/jrpg-milestone-d-stage1-shop-catalog-and-transaction-core.md`

### Stage 2: Merchant 入口与 Scene 路由

目标：

- 让玩家能在世界里与 merchant 交互，并稳定打开指定 shop

本阶段聚焦：

- 新增 `MerchantComponent` 或等价实例组件
- merchant 优先从地图 actor object 的属性附加，例如 `shop_id`
- 新增 `ShopInteractionSystem`，订阅 `InteractCommand`
- `InteractionSystem::chooseFacingTarget()` 显式加入 merchant 分支，并在代码中把 merchant 分支放在 `QuestGiverComponent` 分支之前
- 锁定 merchant 的交互归属：带 `MerchantComponent` 的实体由 `ShopInteractionSystem` 独占处理
- `DialogueSystem` 对带 `MerchantComponent` 的实体显式跳过
- `ShopInteractionSystem` 负责校验 `shop_id` 是否存在，并通过 `PushSceneEvent` 打开 `ShopMenuScene`
- 明确 `ShopMenuScene` 的依赖注入方式：优先采用显式构造注入，而不是只通过 `Context` 间接查找 gameplay 依赖

推荐最小方案：

- Milestone D 先限制为“一个 merchant 绑定一个 shop_id”
- merchant 配置走实例级地图属性，不写进 actor blueprint
- 若实体同时带 `MerchantComponent` 与其他特殊交互组件，本阶段应直接 warn 并按 merchant 独占处理，或在 loader 阶段拒绝这种配置
- Scene 打开参数应显式传入 `player / shop_id / ShopCatalog / ItemCatalog / ShopTransactionService`
- 若后续特殊交互类型继续增长，`chooseFacingTarget()` 可考虑重构为 priority-scored 的通用扫描；但这不是 Milestone D 的当前范围

原因：

- `InteractCommand` 已经是当前项目最稳定的玩法扩展点，商店入口沿用它最自然
- merchant 和普通对话并行消费会产生明显冲突，必须先锁定 owner
- 地图实例级配置更适合后续做“同一 actor blueprint 在不同地图卖不同商品”

阶段交付物：

- merchant NPC 入口闭环
- 世界交互到商店 Scene 的稳定路由
- 不污染 `GameScene` 与 `DialogueSystem` 的商店接线

建议后续细化文档：

- `plans/jrpg-milestone-d-stage2-merchant-interaction-and-scene-routing.md`

### Stage 3: Shop Scene 骨架与 Buy 闭环

目标：

- 让玩家可以在商店里浏览 buy list、确认数量并完成购买

本阶段聚焦：

- 新增独立 `ShopMenuScene`
- `ShopMenuScene` 使用 `RmlDocumentController`、独立 data model 和事件绑定
- 定义最小 ViewModel：merchant title、gold label、buy entries、detail panel、数量选择、状态提示
- 锁定 Buy 模式下的输入流：`menu_up/down/left/right/confirm/cancel`
- 展示价格、可负担状态、不可购买原因
- 购买成功后刷新 wallet / buy list / 背包相关视图
- 保持商店内反馈在 Scene 本地完成，不依赖世界通知气泡覆盖当前 UI
- 显式定义关闭商店路径：`menu_cancel` / UI cancel 走 `requestPopScene()`，并清理未提交的数量选择或局部状态

推荐最小方案：

- `ShopMenuScene` 做成新的覆盖式菜单 Scene，不作为 `InventoryMenuScene` 的额外 tab
- UI 结构优先选择“左侧商品列表 + 右侧详情/价格 + 底部操作区”的稳定双栏布局
- 数量选择先只支持最小必需语义：`1` 与可扩展的 `xN`，不提前做复杂批量采购面板
- 购买动作只通过 `ShopTransactionService` 执行；Scene 只负责把结果映射成提示文本和刷新
- 关闭商店默认对齐 `InventoryMenuScene / RestDialogScene` 的模式：取消键直接 Pop Scene，未提交交易不产生任何写回
- 若 item 不可负担、超出堆叠上限或背包无空间，必须给出明确的本地失败提示

原因：

- 商店除了背包外还要展示 merchant 语义，直接塞进背包菜单会让模型迅速失控
- Buy 是 Milestone D 的第一条核心玩家价值链，先闭环它最能验证目录与交易核心设计
- 采用独立 Scene，后续扩展 Sell / 装备店 / 价格比较也更自然

阶段交付物：

- 可导航的商店主界面
- 从 buy list 到真实入包与金币扣减的闭环
- 商店内可见的成功/失败反馈

建议后续细化文档：

- `plans/jrpg-milestone-d-stage3-shop-scene-and-buy-flow.md`

### Stage 4: Sell 流程与背包同步

目标：

- 让玩家可以出售背包里的可卖物品，并让 Hotbar / 背包状态保持一致

本阶段聚焦：

- 构建 Sell 模式下的玩家物品列表
- 过滤或禁用不可卖物品
- 锁定 sell 的数量规则与选择来源
- 成功卖出后移除背包物品、增加金币、刷新 UI
- 确保 `InventoryChanged` 继续驱动现有 Hotbar 一致性逻辑
- 防止“remove 不完整但 gold 已增加”这类脏状态
- 基于 Stage 1 锁定的预检语义，确保 Sell 在 exact-slot 不满足时整笔失败，而不是局部扣除后再补救

推荐最小方案：

- Sell 模式优先基于玩家当前真实背包槽位或可稳定映射回槽位的 view model 构建，不做第二套独立库存真相
- Sell 资格只由显式 sell rule 控制，不从 category、description 或 battle_use 隐式推断
- 数量确认先支持 `1` 和 `all` 两档即可，后续再扩更细粒度步进
- 卖出成功后保持留在商店内，并即时刷新 gold label、sell list 与详情面板
- 若当前物品绑定在 hotbar，上层同步继续依赖现有 `InventoryChanged -> HotbarSystem` 路径，不新增商店专用 hotbar 补丁

原因：

- Sell 是对当前背包真相的消费行为，最好直接锚定现有 inventory 模型，避免产生“商店镜像库存”
- 现有 Hotbar 已经绑定 inventory slot 引用，复用既有事件链路能大幅减少额外同步逻辑
- 显式 sell rule 可以更轻松地保留“任务物品 / 关键素材不可卖”的控制力
- 先把 exact-slot 卖出语义锁稳，后续才有空间再扩“按 item 聚合视图卖出”的高级 UI

阶段交付物：

- 从背包到卖出获得金币的完整闭环
- 与现有 Hotbar / Inventory 一致性保持兼容
- 商店买卖两个方向都可用

建议后续细化文档：

- `plans/jrpg-milestone-d-stage4-sell-flow-and-inventory-sync.md`

### Stage 5: UI 收尾、文档与测试补强

目标：

- 让商店系统达到“结果可见、行为稳定、后续可继续扩展”的状态

本阶段聚焦：

- 完成 `shop_menu.rml / rcss` 的禁用态、选中态、价格信息与空列表表现
- 补充 `ShopCatalog` 解析与校验测试
- 补充 `ShopTransactionService` 测试：
  - 金币不足
  - 背包空间不足
  - 不可卖物品
  - 数量上限 / 数量 clamp
  - 买卖成功后的 gold / inventory 结果
- 补充 merchant 入口与 Scene push 测试
- 视需要补充 RmlUi 绑定或 layout smoke test
- 更新相关文档，并明确 Milestone D 推荐范围下“不新增商店专用 save schema”

推荐最小方案：

- 测试优先覆盖 domain 与 interaction 边界，UI 只做关键 smoke test
- 若推荐范围保持“无限库存”，则不新增商店 save data；存档回归只需覆盖“交易后的金币与背包变化能正常被已有 save 流程保存”
- 视实现情况更新 `docs/game/inventory_hotbar.md`、`docs/game/interaction_and_dialogue.md`，必要时新增 `docs/game/shop_flow.md`

原因：

- 商店是高度数据驱动的系统，目录和交易规则比纯 UI 更值得优先锁定测试
- 当前 wallet/inventory save 闭环已经存在，Milestone D 的目标应是复用而不是重开存档结构
- 没有基本 smoke test 的商店 UI 很容易在后续样式调整中退化

阶段交付物：

- 稳定的商店 UI 呈现
- 商店关键路径回归测试
- 与现有文档一致的实现边界

建议后续细化文档：

- `plans/jrpg-milestone-d-stage5-ui-docs-and-tests.md`

## 需要新增的文件

以下为推荐新增文件，是否最终拆分为独立文件，可在各阶段细化时再确认：

- `plans/jrpg-milestone-d-stage1-shop-catalog-and-transaction-core.md`
- `plans/jrpg-milestone-d-stage2-merchant-interaction-and-scene-routing.md`
- `plans/jrpg-milestone-d-stage3-shop-scene-and-buy-flow.md`
- `plans/jrpg-milestone-d-stage4-sell-flow-and-inventory-sync.md`
- `plans/jrpg-milestone-d-stage5-ui-docs-and-tests.md`

若按推荐方案实施，代码层后续大概率会新增：

- `assets/data/shops.json`
- `src/game/data/shop_catalog.h`
- `src/game/data/shop_catalog.cpp`
- `src/game/component/merchant_component.h`
- `src/game/domain/shop_transaction_service.h`
- `src/game/domain/shop_transaction_service.cpp`
- `src/game/system/shop_interaction_system.h`
- `src/game/system/shop_interaction_system.cpp`
- `src/game/scene/shop_menu_scene.h`
- `src/game/scene/shop_menu_scene.cpp`
- `ui/rmlui/scenes/shop_menu.rml`
- `ui/rmlui/scenes/shop_menu.rcss`

但这不是当前索引文档必须立即锁定的唯一命名。

## 实现步骤

### Step 1

完成 Stage 1 细化计划，先锁定 `ShopCatalog` schema、sell rule 形式与交易原子性规则。

### Step 2

完成 Stage 2 细化计划，确定 `MerchantComponent` 数据来源、交互 owner 规则与 Scene 路由。

### Step 3

完成 Stage 3 细化计划，确定 `ShopMenuScene` 的 ViewModel、Buy 模式导航与数量确认。

### Step 4

完成 Stage 4 细化计划，锁定 Sell 模式的列表来源、slot 语义和 hotbar 同步不变量。

### Step 5

完成 Stage 5 细化计划，统一测试策略、文档更新与 UI 收尾范围。

当前索引计划的推荐结论是：

- 先立 `ShopCatalog + ShopTransactionService`，再做 merchant 入口和 UI
- 商店入口继续复用 `InteractCommand`，不直接塞进 `DialogueSystem` 或 `GameScene`
- 商店应是新的覆盖式 Scene，不挤进 `InventoryMenuScene`
- Buy / Sell 交易必须是原子的，不能直接裸调 `InventoryDomainService` 做半交易
- Milestone D 默认采用“静态无限库存”，不在本阶段新增商店库存存档
- Sell 资格应走显式规则表，不靠 item category 猜测
- 在 Stage 1 锁定交易语义后，Stage 2 和 Stage 3 可以并行细化；Stage 4 依赖前两者稳定后再落地更合适

## ToDo

- [x] Stage 1: 细化 shop 目录、sell rule 与交易原子性方案 → `plans/jrpg-milestone-d-stage1-shop-catalog-and-transaction-core.md`
- [x] Stage 1: 锁定 preview 返回结构、价格告警策略与 `removeItem(slot_index)` 的交易语义
- [ ] Stage 2: 细化 merchant 入口、交互 owner 与 Scene 路由
- [ ] Stage 3: 细化 `ShopMenuScene`、Buy 列表与数量确认
- [ ] Stage 4: 细化 Sell 流程、slot 语义与 hotbar 同步边界
- [ ] Stage 5: 细化 UI 收尾、测试补强与文档更新

## 备注

本索引计划采用的推荐范围是：

- 先做“merchant 打开商店 -> 购买商品 -> 卖出物品”的最小商店闭环
- 商店交易只依赖现有 `PlayerWalletComponent`、`InventoryDomainService` 和 `InteractCommand` 扩展链路
- 默认 merchant 为无限库存，不提前做 restock/save
- 默认全局 sell rule 对所有商店统一生效，不提前做 per-shop sell price
- 商店先服务基础 JRPG Demo 闭环，不提前捆绑装备对比、声望折扣和复杂经济

当前额外设计结论：

- 不推荐把价格字段直接塞进 `ItemData`，否则 item identity 与经济规则会过度耦合
- 不推荐把 shop 数据塞进 `RpgCatalog`，否则 JRPG 战斗目录与世界经济目录会互相污染
- 不推荐让 `InventoryMenuScene` 直接承担商店 UI；商店的 merchant 语义和交易反馈会让菜单模型迅速失控
- 不推荐在没有 transaction service 的情况下让 Scene 直接调用 `addItem/removeItem`；当前 inventory service 的 partial accept 语义不适合裸用于商店
- 不推荐把 `removeItem(slot_index)` 直接当成 Sell 的 exact-slot 原子原语；它当前更接近“尽量移除 + 返回 rejected”而不是“要么全成要么全败”
- 不推荐在 Milestone D 首批支持同一 NPC 的“任务 + 商店 + 闲聊”多态交互；先锁定单一 owner 才能保证行为确定
- 不推荐在 Milestone D 做限量库存与补货；它们会立刻把问题扩展到 save、日历和经济刷新规则
- 不推荐把“全局 sell rule”误当成最终形态；它只是 Milestone D 的范围收敛，后续仍可扩为 per-shop sell rule
- 若特殊交互类型继续增长，不推荐继续在线性 `best_xxx` 变量上无止境叠分支；但这属于交互系统后续重构题，不在本里程碑中处理
- 若 Stage 1 没有先锁定交易原子性和 sell rule 表，后续 Buy/Sell UI 很容易变成一堆临时特判

这样可以保证 Milestone D 形成一个真正可持续扩展的“merchant 入口 + buy/sell 规则 + 商店 UI”闭环。
