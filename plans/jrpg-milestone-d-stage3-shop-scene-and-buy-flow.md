# Milestone D / Stage 3: Shop Scene 与 Buy 闭环细化计划

## 实现思路

- Stage 3 不再新建第二套商店 Scene，而是直接扩展当前已落地的 `ShopMenuScene` 骨架。
- 本阶段只做 `Buy` 完整闭环，不提前实现 `Sell` 列表与卖出提交；若界面需要保留 `Sell` 入口，默认以 disabled / coming soon 形式存在。
- `ShopMenuScene` 继续保持“覆盖式菜单 Scene + 显式构造注入 + 本地 data model 驱动”的组织方式，不退回到 world HUD 或 `InventoryMenuScene` tab 模式。
- gameplay 真相继续分层保持清晰：
  - `ShopCatalog` 提供静态 buy list
  - `ShopTransactionService` 负责 preview / commit
  - `ShopMenuScene` 只负责视图状态、输入、提示文案与刷新
- Stage 3 锁定一个最小但完整的 Buy 交互模型：
  - 上下移动当前商品选择
  - 左右调整当前购买数量
  - confirm 对当前条目执行购买
  - cancel 关闭商店
- 不做二次确认弹窗，不做独立数量子 Scene；数量选择直接挂在当前选中商品详情区。
- 为了避免后续 Stage 4 重做列表模型，Stage 3 应尽早把商品列表 ViewModel 从 scene 内零散字符串升级为结构化数组。
- 推荐新增轻量 UI helper，统一承载：
  - `ShopBuyEntryViewModel`
  - RmlUi struct / array 注册
  - 条目 icon / label / 价格文本构建
- `ShopBuyEntryViewModel` 建议至少包含：
  - `index`
  - `item_id_hash`
  - `icon_decorator`
  - `item_name`
  - `price_text`
  - `owned_text`
  - `is_selected`
  - `is_disabled`
- 其中 `item_id_hash` 不要求绑定到 RmlUi，但建议保留在 C++ 侧 view model 中，避免 Scene 每次都只能通过 `selected_buy_index_` 回查 `ShopCatalog::buy_entries_`。
- 条目图标不需要重新造轮子，直接复用现有 `rml_item_icon_helpers` 或等价 icon decorator 构建逻辑。
- `ShopMenuScene` 的本地状态建议至少包含：
  - 当前 `selected_buy_index_`
  - 当前 `requested_buy_quantity_`
  - 当前 `ShopBuyPreview active_buy_preview_`
  - `gold_label_`
  - `status_text_`
  - `detail_name_ / detail_description_ / detail_price_text_ / detail_total_text_ / detail_quantity_text_`
  - `detail_owned_text_`
  - `buy_enabled_`
- `requested_buy_quantity_` 的 UI 语义本阶段锁定为：
  - 最小值为 `1`
  - 非堆叠物品固定为 `1`
  - 堆叠物品允许 `xN`，但 UI 上限不应一律写死为 `99`
  - 推荐上限取 `min(stack_limit, 99)`，或直接取 `stack_limit`
  - 真正能否购买仍以 `ShopTransactionService::previewBuy()` 为准
- 也就是说，UI 层不应允许玩家把数量调到明显超过 `ItemData::stack_limit_` 的值，再由 preview 以“数量无效”拒绝。
- Stage 3 不要求“列表里每个商品都实时预览全部失败原因”；最小方案只需要保证：
  - 当前选中条目会实时 preview
  - 当前选中条目的 buy 按钮禁用态与提示文本正确
- 金币与背包在 Scene 打开期间通常只会被当前商店写入，因此 Stage 3 可先采用“本地 commit 后主动全量刷新”的方案，不强依赖额外 dispatcher 订阅。
- 购买成功后 Scene 应立即刷新：
  - gold label
  - 当前条目的 owned count
  - 当前 preview / total price / failure text
  - 列表 disabled / selected 状态
- 购买失败后不弹世界气泡，所有反馈都留在商店内，例如 `status_text_` 或详情区说明文字。
- `ShopTradeFailureReason` 到玩家可读文案的映射应在 Scene 层集中实现，不把自由字符串塞回 domain service。
- Stage 3 推荐锁定最小失败文案映射：
  - `InsufficientGold` -> 金币不足
  - `InventoryFull` -> 背包空间不足
  - `InvalidQuantity` -> 数量无效
  - `InvalidPlayer` -> 操作无效
  - `ItemNotSoldHere / InvalidItem / InvalidShop` -> 当前商品不可购买
- 实现时建议让失败文案映射覆盖全部 `ShopTradeFailureReason` 枚举值，避免遗漏分支只靠默认兜底。
- 关闭商店的语义继续与 Stage 2 保持一致：
  - `menu_cancel` / UI close 直接 `requestPopScene()`
  - 未提交数量选择不写回
  - 不引入“离开前确认”
- Stage 3 不包含：
  - sell list
  - sell quantity
  - per-entry 批量 preview 缓存优化
  - 分类筛选、搜索、排序
  - 复杂价格比较、装备对比、试穿

## 需要新增的文件

- `src/game/ui/shop_menu_support.h`
- `src/game/ui/shop_menu_support.cpp`
- `tests/game/shop_menu_buy_flow_test.cpp`
- `plans/jrpg-milestone-d-stage3-shop-scene-and-buy-flow.md`

说明：

- `src/game/scene/shop_menu_scene.h/.cpp`
- `ui/rmlui/scenes/shop_menu.rml`
- `ui/rmlui/scenes/shop_menu.rcss`

以上文件在 Stage 2 已存在，Stage 3 以扩展它们为主，不另起新 Scene。

## 实现步骤

### Step 1. 抽出 Shop Buy ViewModel 与 Rml 注册辅助

- 在 `shop_menu_support.h/.cpp` 中定义 `ShopBuyEntryViewModel`。
- 提供最小注册接口，例如：
  - `registerShopBuyEntryViewModelType(Rml::DataModelConstructor&)`
- 提供最小填充辅助，例如：
  - 根据 `ShopBuyEntryData + ItemData + 当前持有数量 + 选中态` 生成条目文本与 icon decorator
- 建议在同一 helper 中顺手补一个轻量 owned count 聚合函数，例如：
  - `countOwnedItems(const InventoryComponent&, entt::id_type item_id_hash)`
- 若 Stage 3 要把数量上限直接绑定到当前选中条目，也可以在 helper 中统一解析：
  - `resolveBuyQuantityUiMax(const ItemData&)`
- 不要直接复用 `SlotGridViewModel`：
  - shop list 不是 inventory slot
  - 不需要 slot index / drag 语义
  - 直接造一个更贴 buy list 的 view model 会更清晰

### Step 2. 扩展 ShopMenuScene 的本地状态与刷新入口

- 给 `ShopMenuScene` 新增最小 buy state：
  - `std::vector<ShopBuyEntryViewModel> buy_entries_`
  - `int selected_buy_index_{0}`
  - `int requested_buy_quantity_{1}`
  - `ShopBuyPreview active_buy_preview_{}`
  - `Rml::String gold_label_`
  - `Rml::String status_text_`
  - `Rml::String detail_name_`
  - `Rml::String detail_description_`
  - `Rml::String detail_price_text_`
  - `Rml::String detail_total_text_`
  - `Rml::String detail_quantity_text_`
  - `Rml::String detail_owned_text_`
  - `bool buy_enabled_{false}`
- 推荐补齐一组显式刷新函数：
  - `syncGoldLabel()`
  - `rebuildBuyEntries()`
  - `refreshSelectedBuyEntry()`
  - `refreshBuyPreview()`
  - `refreshStatusText()`
  - `refreshAll()` 或等价总入口
- 刷新职责除了改本地状态，还必须显式触发 RmlUi dirty：
  - 推荐让每个 `refresh*()` 在末尾调用自己负责字段的 `document_controller_.markDirty(...)`
  - 或者由 `refreshAll()` 统一 `markDirty(...)`
  - 但不要只改成员变量而忘记 dirty，否则 RmlUi 不会自动刷新
- `ShopMenuScene::init()` 中在校验完 `shop_id` 后，直接构建第一版列表与详情，不再停留在纯占位文本。

### Step 3. 锁定 Buy 输入流与数量规则

- Stage 3 的键盘 / 手柄导航建议锁定为：
  - `menu_up`：上一商品
  - `menu_down`：下一商品
  - `menu_left`：数量 -1
  - `menu_right`：数量 +1
  - `menu_confirm`：购买当前选中商品
  - `menu_cancel`：关闭商店
- 为了避免引入复杂焦点系统，本阶段不做“列表焦点 / 按钮焦点 / 数量焦点”三套切换。
- 鼠标点击仍应支持：
  - 选中某个商品
  - 点 `- / + / Buy / Close`
- 数量调整规则建议：
  - 无选中商品时忽略
  - 非堆叠物品固定 1，不响应左右调整
  - 堆叠物品在 `[1, 99]` 内调整
  - 实际上限应进一步受当前 `ItemData::stack_limit_` 限制
  - 切换商品后数量重置为 `1`
- 每次“切换商品 / 调整数量”后都立即刷新一次 `previewBuy()`。
- 非堆叠物品时，数量选择器建议保持布局不变：
  - `detail_quantity_text_` 仍显示 `x1`
  - `- / +` 按钮显示但为 disabled
  - 不建议直接隐藏按钮，避免详情面板跳布局

### Step 4. 实现 Buy preview 到 UI 绑定

- 当前选中商品的详情面板至少展示：
  - 物品名
  - 描述
  - 单价
  - 当前数量
  - 总价
  - 当前持有数量
- `previewBuy()` 结果直接驱动：
  - `buy_enabled_`
  - 当前详情区失败提示
  - 价格 / 总价 / 交易后金币的显示
- `owned_count` 建议通过 Step 1 中的 helper 从玩家当前 `InventoryComponent` 聚合统计，不引入第二套缓存真相，也不要把循环统计逻辑散落在 Scene 各处。
- 若 `ShopCatalog` 中 buy entry 指向的 item 在 `ItemCatalog` 中缺失，Scene 不负责兜底修复；记录错误并跳过该条目即可。

### Step 5. 实现 commitBuy 与成功 / 失败刷新

- `menu_confirm` 或 UI buy 按钮只调用 `ShopTransactionService::commitBuy()`，不允许 Scene 直接扣 gold / add item。
- commit 前不需要额外手写业务校验，直接以 `active_buy_preview_.canCommit()` 做前置门。
- commit 成功后：
  - 生成本地成功提示，例如 “购买 Potion x2”
  - 立即刷新 gold / owned count / preview / buy button state
  - 保持停留在商店内，不自动关闭 Scene
- commit 失败后：
  - 将 `ShopBuyResult.failure_reason` 映射为本地失败提示
  - 不改变当前选中商品
  - 不关闭 Scene
- Stage 3 默认无限库存，因此成功购买后 buy list 本身不需要减少商品数量或删除条目。

### Step 6. 扩展 RML / RCSS 为真实 Buy 页面

- 在现有 `shop_menu.rml / rcss` 基础上扩成最小双栏布局：
  - 左侧：buy list
  - 右侧：detail panel
  - 底部或右下：`- / + / Buy / Close`
- 推荐沿用当前项目已有主题资源，不引入另一套视觉系统。
- RML 至少需要：
  - `data-for` 渲染 buy list
  - 选中条目点击事件
  - `adjust_quantity(-1/+1)` 事件
  - `buy_confirm` 事件
  - `close` 事件
- RCSS 至少需要：
  - 选中态
  - disabled 态
  - 按钮 `tab-index: auto`
  - 小屏逻辑分辨率下不溢出 640x360dp
- Stage 3 优先保证可读、稳定、可导航；不追求最终 polish。

### Step 7. 补齐 Stage 3 测试

- `shop_menu_buy_flow_test.cpp` 覆盖最关键的 scene 行为：
  - 打开合法 shop 后能构建 buy list
  - 切换商品会刷新详情
  - 调整数量会刷新 preview / total price
  - 金币不足时 buy disabled
  - commit 成功后 gold / inventory / status_text 刷新
  - commit 失败时 scene 保持打开且 status_text 正确
- 现有 `shop_menu_scene_smoke_test.cpp` 继续保留，补充 source 断言：
  - 绑定 `menu_confirm`
  - 绑定 quantity 调整事件
  - 使用 `ShopTransactionService::previewBuy / commitBuy`
  - 包含 `data-for` 关键字
  - 包含 `buy_entries` 绑定名
  - 包含 `document_controller_.markDirty(...)` 或等价 dirty 调用
- 如测试成本较高，可混合采用：
  - source/smoke test 锁 UI 接线
  - runtime test 锁 buy 行为
- Stage 3 不要求新增 Playwright 或端到端 UI 自动化。

## ToDo

- [ ] 新增 `ShopBuyEntryViewModel` 与 RmlUi 注册辅助
- [ ] 扩展 `ShopMenuScene` 的 buy list / detail / quantity / status 本地状态
- [ ] 锁定 `menu_up/down/left/right/confirm/cancel` 的 Buy 输入流
- [ ] 将 `ShopTransactionService::previewBuy()` 绑定到详情区与按钮禁用态
- [ ] 将 `ShopTransactionService::commitBuy()` 接入 Buy 按钮与确认输入
- [ ] 扩展 `shop_menu.rml / rcss` 为最小双栏 Buy UI
- [ ] 补齐 `ShopMenuScene` Buy 流程相关测试

## 疑问

- 暂无必须先澄清的问题；按当前 Stage 2 代码结构，Stage 3 可以直接开始实现。
