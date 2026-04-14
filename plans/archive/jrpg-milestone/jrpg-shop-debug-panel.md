# Shop Debug Panel 集成计划

## 实现思路

- 新增独立 `ShopDebugPanel`，放在 `src/game/debug/`，作为 `Game` 分类下的独立调试面板接入；不把商店调试入口塞进现有 `InventoryDebugPanel`、`QuestDebugPanel` 或 `SaveLoadDebugPanel`。
- 面板继续沿用现有 `engine::debug::DebugPanel + ImGui` 架构，只在 `TF_ENABLE_DEBUG_UI` 下编译和注册，不改正式 RmlUi，不新增调试专用 scene。
- 面板窗口不使用 `ImGuiWindowFlags_AlwaysAutoResize`；推荐设置一个 `FirstUseEver` 初始尺寸并允许开发时手动缩放，避免 buy 列表、sell 槽位列表和 preview 区频繁跳动。
- 面板只读取真实 runtime truth：
  - `PlayerTag + InventoryComponent + PlayerWalletComponent`
  - `const ShopCatalog*`
  - `const ItemCatalog*`
  - `ShopTransactionService*`
- 所有真正的商店交易都必须复用正式路径：
  - `previewBuy / commitBuy`
  - `previewSell / commitSell`
- 面板不自己复制第二套价格、背包容量或 exact-slot 规则；商店调试的目标是“更快地打到正式交易逻辑”，而不是造一个只在 debug 面板里成立的伪商店。
- 第一版不复用 `MerchantComponent`、`InteractCommand` 或 `ShopMenuScene`：
  - 调试面板直接从 `ShopCatalog::listShops()` 选择 shop
  - 这样可以在没有地图 merchant 的情况下直接测试任意商店
  - 正式 UI/scene 导航仍由现有 `ShopMenuScene` 测试覆盖
- 面板建议拆成 4 个稳定区域：
  - 顶部 `Runtime Summary`
  - 左侧 `Shop Picker`
  - 中间 `Buy / Sell` tab
  - 右侧 `Preview + Actions + Status`
- `Sell` 视图必须继续锚定真实背包槽位：
  - 一个非空 slot 对应一条 sell row
  - 不按 `item_id` 聚合
  - `commitSell()` 始终带真实 `slot_index`
- 第一版建议补一个最小 `Wallet Seed` 小区块：
  - 允许 `Set Gold` / `+Step`
  - 这是 debug-only 便捷能力，因为当前没有独立的 gold debug panel，而 buy success path 经常依赖钱包余额
  - 这部分可以直接修改 `PlayerWalletComponent::gold_`，但必须和正式 trade 操作视觉上分区，避免和真实商店路径混淆
- 第一版不重复实现“加物品/删物品”：
  - sell 测试用到的物品准备继续复用现有 `InventoryDebugPanel`
  - Shop 面板只负责商店域相关的 preview / commit / status
- 面板内部保留一个 `status_` 文本，用于显示最近一次 debug 操作结果；不新增商店专用 debug event。
- 当前 milestone 已经有较好的 source/smoke/runtime test 覆盖，调试面板第一版的目标是补“人工高频验证入口”，不是替代这些测试。

## 需要新增的文件

- `src/game/debug/shop_debug_panel.h`
- `src/game/debug/shop_debug_panel.cpp`
- `src/game/debug/shop_debug_panel_helpers.h`
- `src/game/debug/shop_debug_panel_helpers.cpp`
- `tests/game/shop_debug_panel_registration_test.cpp`
- `tests/game/shop_debug_panel_helpers_test.cpp`

## 实现步骤

### Step 1. 定义 ShopDebugPanel 与最小面板状态

- 新增 `ShopDebugPanel`，继承 `engine::debug::DebugPanel`。
- 构造依赖建议固定为：
  - `entt::registry&`
  - `const game::data::ShopCatalog*`
  - `const game::data::ItemCatalog*`
  - `game::domain::ShopTransactionService*`
- 面板内部状态保持最小：
  - 当前选中的 `selected_shop_id_ : std::string`
  - 当前 tab：`Buy / Sell`
  - 当前选中的 buy entry 索引
  - 当前选中的 sell row 索引
  - `requested_buy_quantity_`
  - `requested_sell_quantity_`
  - `show_unsellable_slots_`
  - `gold_seed_step_`
  - 最近一次操作状态文本 `status_`
- `selected_shop_id_` 明确按 `std::string` 存储，与 `ShopData::id_` 对齐；调用 `ShopCatalog` / `ShopTransactionService` 时直接按 `std::string_view` 传递即可。
- 玩家实体不做手工输入；每帧自动查找第一个带 `PlayerTag + InventoryComponent + PlayerWalletComponent` 的实体。
- 若玩家、`ShopCatalog`、`ItemCatalog` 或 `ShopTransactionService` 缺失，面板进入只读/不可操作状态并显示明确提示。

### Step 2. 抽取 shop debug helper，统一列表与选择逻辑

- 新增 `shop_debug_panel_helpers.h/.cpp`，但边界保持轻量，只承载“值得单测的纯数据逻辑”。
- helper 建议覆盖：
  - buy row 数据结构与构建：`item_id / display_name / owned_count / buy_price`
  - sell row 数据结构与构建：`slot_index / item_id / display_name / count / sell_price / is_sellable`
  - quantity `clamp`
  - buy/sell failure reason 文案映射
  - sellable 判定与 disabled 语义
- `Sell` row helper 必须显式保留 `slot_index`，锁定 exact-slot 语义。
- 若 sell rule 缺失，row 仍可显示，但标记为 disabled，方便开发时直接看到“这件物品为何不能卖”。
- 只服务于 `draw()` 的粘合逻辑，例如：
  - shop 列表收集与排序
  - 当前选中项同步
  - 切换 shop 后的索引回收
  - 被卖空后的当前选择回退
  继续留在 `shop_debug_panel.cpp` 的匿名命名空间中，不额外暴露到 helper 头文件。

### Step 3. 实现只读 runtime summary 与 shop picker

- 面板顶部显示当前玩家摘要：
  - entity id
  - 当前 gold
  - 背包已用槽位 / 总槽位
  - 当前选中的 shop title / id
- 左侧提供 shop 下拉框或列表，数据源固定为 `ShopCatalog::listShops()`。
- 切换 shop 时：
  - buy 选择索引回到首项
  - `requested_buy_quantity_` 重置为 `1`
  - `status_` 清空
- 若 `ShopCatalog` 为空或没有任何 shop，面板直接显示空状态，不做伪造占位数据。

### Step 4. 实现 Buy tab

- `Buy` tab 的数据源固定为当前选中 `shop.buy_entries_`。
- 每条 buy row 建议显示：
  - item name
  - `item_id`
  - buy price
  - 当前 owned 数量
- 右侧详情区显示：
  - item name / description
  - 请求购买数量
  - 单价
  - 总价
  - 交易后 gold
  - 当前 preview 状态
- preview 不需要单独“刷新”按钮；第一版明确保持简单实现：
  - 每帧按当前 `shop_id + item_id + requested_buy_quantity_` 调 `previewBuy()`
  - 不额外引入 dirty flag
  - `Commit Buy` 按钮只在 `preview.canCommit()` 时启用
- `Commit Buy` 必须直接调用 `commitBuy()`，成功后刷新 gold、owned count、preview 和状态文本。
- 交易成功后，`requested_buy_quantity_` 回到 `1`，与当前正式 `ShopMenuScene` 保持一致。

### Step 5. 实现 Sell tab

- `Sell` tab 的数据源固定为当前玩家真实背包。
- 第一版默认显示所有非空 slot，并提供 `show_unsellable_slots_` 开关：
  - 打开时显示所有非空 slot，并标注哪些物品无 sell rule
  - 关闭时只显示可卖 slot，方便高频 sell 测试
- 每条 sell row 建议显示：
  - slot index
  - item name
  - stack count
  - sell price 或 `--`
  - disabled 标记
- 右侧详情区显示：
  - 当前 slot 的物品描述
  - 当前 slot 持有数量
  - 请求卖出数量
  - 单价
  - 总价
  - 交易后 gold
  - 当前 preview 状态
- 第一版继续保持简单实现：每帧按当前 `item_id + quantity + slot_index` 调 `previewSell()`，不额外引入 dirty flag。
- `previewSell()` / `commitSell()` 必须始终带真实 `slot_index`。
- 交易成功后：
  - `requested_sell_quantity_` 回到 `1`
  - 若当前 slot 被卖空，`shop_debug_panel.cpp` 的本地选择同步逻辑要负责把选择安全回收到下一个可见 row
  - 若 slot 仍存在，则保持选中当前 row

### Step 6. 补最小 wallet seed 区块

- 在 summary 或 actions 区加入一个明确标记为 `Debug Wallet Seed` 的小区块。
- 第一版建议提供：
  - `InputInt("Gold Step")`
  - `+Step`
  - `Set Gold`
- `gold_seed_step_` 推荐默认值直接设为 `1000`，这样面板只保留一套步进语义，不再叠加固定档位按钮。
- 这部分直接改 `PlayerWalletComponent::gold_`，不新建 wallet domain service。
- 该区块只负责“让 shop success path 更容易打到”，不承担正式经济系统职责。
- 不在本面板里重复加入“加物品/删物品”按钮，避免与 `InventoryDebugPanel` 职责重叠。

### Step 7. 接入 GameScene 的 Game 分类调试面板

- 在 `GameScene::registerDebugPanels()` 中注册 `ShopDebugPanel`。
- 注册条件建议固定为：
  - `services_->shop_catalog`
  - `services_->shop_transaction_service`
- `item_catalog` 不写进 registration guard，和现有 `QuestDebugPanel` 的风格保持一致；当前 runtime 下它是基础常驻依赖，面板内部再做 defensive check 即可。
- 面板归类为 `engine::debug::PanelCategory::Game`，默认关闭。
- 这一阶段不修改 `GameRuntimeServices` 的 ownership 结构；debug panel 只消费已有服务。
- 若将来要补 `Open Shop Scene` 按钮，再单独评估是否让 panel 额外依赖 `Context / dispatcher`；第一版先不把 scene routing 耦进来。

### Step 8. 补齐最小回归测试

- 新增 `shop_debug_panel_registration_test.cpp`，至少覆盖：
  - `GameScene::registerDebugPanels()` 已注册 `ShopDebugPanel`
  - 注册分类为 `PanelCategory::Game`
  - 注册前置条件包含 `shop_catalog / shop_transaction_service`
- 新增 `shop_debug_panel_helpers_test.cpp`，至少验证：
  - buy row 构建能正确显示 owned count 与价格
  - sell row 构建保留真实 `slot_index`
  - unsellable row 的 disabled 状态正确
  - quantity clamp 正确
  - 交易结果文案映射覆盖 buy/sell 两套主要 failure reason
- 继续使用 source-based registration smoke test + helper unit test，不做 ImGui 交互回放测试。

## ToDo

- [x] 新增独立 `ShopDebugPanel`
- [x] 让面板自动绑定当前玩家的 `InventoryComponent + PlayerWalletComponent`
- [x] 抽取 `shop_debug_panel_helpers`，统一纯数据逻辑：row 构建、quantity clamp、sellable 判定与 failure 文案
- [x] 将 `draw()` 粘合逻辑中的 shop/sell 选择同步继续保留在 panel `.cpp`
- [x] 接入 `ShopCatalog` 驱动的 shop picker
- [x] 实现 `Buy` tab 的实时 preview 与 `commitBuy()` 调试入口
- [x] 实现 `Sell` tab 的 exact-slot preview 与 `commitSell()` 调试入口
- [x] 为 `Sell` tab 加入 `show_unsellable_slots_` 开关
- [x] 补一个最小 `Debug Wallet Seed` 区块
- [x] 在 `GameScene::registerDebugPanels()` 中把面板注册到 `PanelCategory::Game`
- [x] 补齐 `shop_debug_panel_registration_test.cpp`
- [x] 补齐 `shop_debug_panel_helpers_test.cpp`

## 疑问

- 当前没有必须先澄清的问题。
