# Milestone D / Stage 1: Shop 目录与交易核心细化计划

## 实现思路

- 新增独立 `ShopCatalog`，不并入 `ItemCatalog` 或 `RpgCatalog`；静态定义统一放在 `assets/data/shops.json`。
- `ShopCatalog` 只负责静态数据加载、查表、引用校验与轻量数据告警；不持有任何运行时 merchant 库存或玩家交易状态。
- shop 静态模型采用 `flat struct + lookup helper`，不提前引入继承层次或复杂 variant：
  - `ShopBuyEntryData`
  - `ShopSellRuleData`
  - `ShopData`
- Milestone D 的 Stage 1 锁定“全局 sell rule”语义：
  - 每个 `item_id` 最多一条 `sell_price`
  - 同一物品在所有商店的卖出价一致
  - 后续若要做 per-shop sell rule，再单独扩展
- `ShopData` 至少包含：
  - `id`
  - `title`
  - `greeting`
  - `buy_entries`
- `ShopBuyEntryData` 至少包含：
  - `item_id`
  - `buy_price`
  - 可选 `stock`
- `stock` 在 Stage 1 只作为静态 schema 预留字段；Milestone D 默认仍按“无限库存”解释，不新增商店库存存档。
- `ShopCatalog` 加载阶段校验：
  - `schema_version`
  - `shop id` 非空且全局唯一
  - 每个 shop 的 `buy_entries.item_id` 不重复
  - `title` 非空
  - `buy_price > 0`
  - `sell_price > 0`
  - 若配置 `stock`，则 `stock > 0`
- `ShopCatalog` 引用校验只依赖 `ItemCatalog`，不依赖 `RpgCatalog`。
- `ShopCatalog` 额外提供加载期价格告警：
  - 若某物品同时存在 buy entry 和 sell rule，且 `sell_price > 最低 buy_price`，记录 `warn`
  - 该情况不作为 hard error，避免把 Stage 1 变成调表阻塞点
- 交易核心新增 `ShopTransactionService`，职责只包含：
  - buy preview / commit
  - sell preview / commit
  - 原子交易失败原因归一化
- `ShopTransactionService` 不直接发 UI 事件，也不直接持有 Scene 状态；它是纯 gameplay domain service，由上层 Scene 或 system 消费返回结果。
- `ShopTransactionService` 的所有权采用 RAII 方案：
  - `ShopCatalog` 放进 `GameRuntimeServices` 的 `shared_ptr`
  - `ShopTransactionService` 放进 `GameRuntimeServices` 的 `unique_ptr`
  - 不引入全局单例
- Buy 预检直接复用现有 `game::system::detail::simulateAdd()` 与 `stackLimitOrDefault()`，保证“预检能放下”和“真实 addItem 行为”尽量一致。
- 商店购买没有“偏好写入槽位”语义，因此 Buy 预检调用 `simulateAdd()` 时固定传 `preferred_slot_index = -1`。
- Sell 不复用 `removeItem(..., slot_index)` 作为 exact-slot 原子原语，因为它当前是“尽量移除 + 返回 accepted/rejected”语义；Stage 1 必须先在交易层补齐 slot 精确预检。
- `ShopTransactionService` 的 preview 结果结构本阶段直接锁定，至少包含：
  - `requested_quantity`
  - `resolved_quantity`
  - `unit_price`
  - `total_price`
  - `final_gold_after`
  - `can_afford`
  - `has_space` 或等价布尔语义
  - `failure_reason`
- `failure_reason` 建议统一为 enum，不使用自由字符串，至少覆盖：
  - `None`
  - `InvalidPlayer`
  - `InvalidShop`
  - `InvalidItem`
  - `InvalidQuantity`
  - `ItemNotSoldHere`
  - `ItemNotSellable`
  - `InsufficientGold`
  - `InventoryFull`
  - `SlotMismatch`
  - `InsufficientItemCount`
- `ShopTransactionService` 的对外 API 形态本阶段不强行锁死为“传 id”或“传已解析数据引用”两种之一：
  - 若调用侧以 `shop_id + item_id` 更方便，可由 service 内部通过 `ShopCatalog` 解析
  - 若后续实现希望对齐 `QuestTurnInService::turnIn(player, const QuestData&, ...)` 的模式，也可以让内部核心路径接收 `const ShopData& / const ShopBuyEntryData& / const ShopSellRuleData&`
  - Stage 1 真正需要锁定的是交易语义，而不是函数签名风格
- `commitBuy()` / `commitSell()` 必须建立在对应 preview 已通过的同等条件上；若 commit 前真实状态已变化，也必须整笔失败，而不是部分成功。
- `PlayerWalletComponent::gold_` 当前是 `int`；Milestone D / Stage 1 不额外引入金币上溢保护，默认认为 demo 范围内数值足够安全。若后续经济规模扩大，再单独补上溢校验或改更大数值类型。
- `ShopBuyPreview / ShopSellPreview` 与 `ShopBuyResult / ShopSellResult` 的字段语义本阶段锁定，但是否共用内部基类/共享 payload 结构属于实现细节，可在编码时再决定。
- Stage 1 不包含：
  - `MerchantComponent`
  - `ShopInteractionSystem`
  - `ShopMenuScene`
  - 商店 UI
  - 商店库存存档

## 需要新增的文件

- `src/game/data/shop_data.h`
- `src/game/data/shop_catalog.h`
- `src/game/data/shop_catalog.cpp`
- `src/game/domain/shop_transaction_service.h`
- `src/game/domain/shop_transaction_service.cpp`
- `assets/data/shops.json`
- `tests/game/shop_catalog_test.cpp`
- `tests/game/shop_transaction_service_test.cpp`

## 实现步骤

### Step 1. 定义 Shop 静态数据模型

- 在 `shop_data.h` 中定义：
  - `ShopBuyEntryData`
  - `ShopSellRuleData`
  - `ShopData`
- `ShopBuyEntryData` 推荐字段：
  - `item_id_`
  - `item_id_hash_`
  - `buy_price_`
  - `std::optional<int> stock_`
- `ShopSellRuleData` 推荐字段：
  - `item_id_`
  - `item_id_hash_`
  - `sell_price_`
- `ShopData` 推荐字段：
  - `id_`
  - `id_hash_`
  - `title_`
  - `greeting_`
  - `std::vector<ShopBuyEntryData> buy_entries_`
- 数据模型保持最小，不提前加入 merchant portrait、分页、分类筛选、折扣等 UI 层字段。

### Step 2. 实现 ShopCatalog

- `ShopCatalog` 提供最小接口：
  - `loadFromFile()`
  - `schemaVersion()`
  - `findShop(std::string_view / entt::id_type)`
  - `findSellRule(std::string_view / entt::id_type)`
  - `listShops()`
  - `validateReferences(const ItemCatalog*, std::string& out_error)`
- 加载阶段校验：
  - `schema_version == 1`
  - `shops` 为数组且非空
  - `sell_rules` 为数组或缺省
  - `shop.id` 非空且唯一
  - `shop.title` 非空
  - `buy_entries` 为数组且非空
  - 同一 shop 内 `buy_entries.item_id` 不重复
  - `buy_price > 0`
  - `sell_price > 0`
  - 若 `stock` 存在，必须为正整数
- 引用校验阶段：
  - 所有 `buy_entries.item_id` 都必须能在 `ItemCatalog` 中找到
  - 所有 `sell_rules.item_id` 都必须能在 `ItemCatalog` 中找到
- 加载完成后执行轻量价格告警：
  - 比较全局 `sell_rules` 与所有 shop buy entries
  - 对 `sell_price > min_buy_price` 的物品记录 `warn`

### Step 3. 定义交易结果与失败语义

- 在 `shop_transaction_service.h` 中定义统一失败枚举，例如 `ShopTradeFailureReason`。
- 定义 buy/sell preview 结果结构，建议分成：
  - `ShopBuyPreview`
  - `ShopSellPreview`
- 定义 buy/sell commit 结果结构，建议分成：
  - `ShopBuyResult`
  - `ShopSellResult`
- 所有结果结构都应避免依赖 UI 类型，只暴露 gameplay 需要的数据。
- 结果结构必须足够支撑后续 UI：
  - 可直接判断按钮禁用态
  - 可直接计算交易后金币显示
  - 可直接映射出失败提示

### Step 4. 实现 ShopTransactionService

- `ShopTransactionService` 推荐依赖：
  - `entt::registry&`
  - `game::data::ItemCatalog&`
  - `game::data::ShopCatalog&`
  - `game::domain::InventoryDomainService&`
- Buy preview 规则：
  - 玩家必须存在且带 `PlayerWalletComponent`
  - `shop_id` 和 `item_id` 必须有效，且该 item 必须在该 shop 的 buy list 中
  - `quantity > 0`
  - `total_price = unit_price * quantity`
  - 钱包余额足够
  - 使用 `simulateAdd(simulated_slots, -1, ...)` 在 inventory slots 副本上验证是否能完整放入
  - 只要无法完整放入，就返回 `InventoryFull`
- Buy commit 规则：
  - 重新执行关键预检，防止 preview 与 commit 之间状态变化
  - 先调用 `InventoryDomainService::addItem()`
  - 只有在 `accepted == requested_quantity` 时才允许扣钱
  - 若 add 结果不完整，直接视为失败并返回，不允许出现“扣钱但只进部分物品”
- Sell preview 规则：
  - 玩家必须存在且带 `PlayerWalletComponent + InventoryComponent`
  - item 必须存在于全局 sell rule 中
  - `slot_index` 必须有效
  - 指定 slot 的 `item_id` 必须与请求一致
  - 指定 slot 的 `count` 必须 >= requested quantity
  - `total_price = sell_price * quantity`
- Sell commit 规则：
  - 重新执行 exact-slot 预检
  - 调用 `removeItem(player, item_id, quantity, slot_index)`
  - 只有在 `accepted == requested_quantity` 时才允许加钱
  - 若 remove 结果不完整，整笔失败并返回，不允许局部扣物后再补偿金币

### Step 5. 接入 Runtime Services 装配

- `GameRuntimeServices` 新增：
  - `std::shared_ptr<game::data::ShopCatalog> shop_catalog`
  - `std::unique_ptr<game::domain::ShopTransactionService> shop_transaction_service`
- `system_bundle.h/.cpp` 补齐前置声明与 include。
- `GameRuntimeAssembler` 新增 `ensureShopCatalog()`：
  - 若 `services.item_catalog == nullptr`，直接 `error return`，不做隐式补救
  - 在 `ensureItemCatalog()` 成功后调用
  - 加载 `assets/data/shops.json`
  - 完成 `ItemCatalog` 引用校验
- `shop_transaction_service` 明确在 `assembleSystems()` 中创建，并放在 `inventory_domain_service` 初始化之后，保持与 `quest_turn_in_service` 的现有装配风格一致
- Stage 1 不新增任何 system 装配；交易核心只先作为 runtime service 存在。

### Step 6. 提供最小项目配置与测试

- 在 `assets/data/shops.json` 中提供最小可用样例，至少覆盖：
  - 一个 shop
  - 两个 buy entries
  - 两条 sell rules
- `shop_catalog_test.cpp` 覆盖：
  - 成功加载项目配置
  - 重复 `shop.id`
  - 同 shop 重复 `buy_entries.item_id`
  - 非法价格
  - 缺失 title
  - 缺失 item 引用
  - `sell_price > min_buy_price` 时仅告警不失败
- `shop_transaction_service_test.cpp` 覆盖：
  - buy preview 成功
  - 金币不足
  - 背包空间不足
  - item 不在当前 shop 中
  - sell preview 成功
  - item 不可卖
  - `slot_index` 不匹配
  - 指定槽位数量不足
  - buy commit 成功后 gold / inventory 正确变化
  - sell commit 成功后 gold / inventory 正确变化
  - commit 前状态变化时整笔失败

## ToDo

- [ ] 新增 Shop 静态数据模型与 `assets/data/shops.json`
- [ ] 实现 `ShopCatalog` 的加载、查表、引用校验与价格告警
- [ ] 锁定 buy/sell 的 preview 结果结构与失败枚举
- [ ] 实现 `ShopTransactionService` 的 buy/sell preview 与原子 commit
- [ ] 在 `GameRuntimeServices / GameRuntimeAssembler` 接入 `shop_catalog`
- [ ] 在 `assembleSystems()` 中于 `inventory_domain_service` 之后接入 `shop_transaction_service`
- [ ] 补齐 `ShopCatalog` 与 `ShopTransactionService` 相关测试
