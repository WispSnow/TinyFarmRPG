# Milestone D / Stage 2: Merchant 入口与 Scene 路由细化计划

## 实现思路

- Stage 2 的目标不是完成商店 UI 本体，而是先打通这条稳定链路：
  - 地图 actor 实例属性 `shop_id`
  - `MerchantComponent`
  - `InteractionSystem` 选中 merchant
  - `ShopInteractionSystem` 消费 `InteractCommand`
  - `PushSceneEvent`
  - 最小 `ShopMenuScene` 骨架打开 / 关闭
- merchant 的运行时真相继续放在地图实例实体上，不写回 actor blueprint，不并入 `DialogueComponent`。
- `MerchantComponent` 推荐保持最小：
  - `shop_id_`
  - `shop_id_hash_`
- `shop_id` 的来源继续复用 `EntityBuilder::buildActor()` 的实例属性读取模式，对齐已有 `quest_offer_id` 路径。
- Stage 2 锁定“特殊交互 owner 唯一”规则：
  - merchant 是一种特殊交互 owner
  - 带 `MerchantComponent` 的实体由商店链路独占处理
  - 普通 `DialogueComponent` 可以继续共存于实体结构上，但不能再消费同一次 `InteractCommand`
- 对于 `MerchantComponent` 与 `QuestGiverComponent` 的冲突，Stage 2 直接采取最小确定性方案：
  - 若地图实例同时声明 `shop_id` 与 `quest_offer_id`
  - `EntityBuilder` 记录 `warn`
  - 仅附加 `MerchantComponent`
  - 不再附加 `QuestGiverComponent`
- 除了 loader 阶段的冲突收敛，还应在系统层做防御式 owner 保护：
  - `DialogueSystem` 对 `MerchantComponent` 显式跳过
  - `QuestInteractionSystem` 也对 `MerchantComponent` 显式跳过
  - 即使未来出现非法组合实体，也不会让 quest/dialogue 与 shop 同时消费一次交互
- `InteractionSystem::chooseFacingTarget()` 继续沿用当前线性扫描结构，不在 Stage 2 提前重构为通用 priority-scored 框架。
- 由于当前 `chooseFacingTarget()` 依赖 `if (...) { ...; continue; }` 维持类型互斥，merchant 分支必须插在 `QuestGiverComponent` 分支之前；否则优先级不会真正生效。
- Stage 2 明确把交互优先级锁定为：
  - `Merchant`
  - `QuestGiver`
  - `Dialogue NPC`
  - `Chest`
  - `Rest`
- `ShopInteractionSystem` 的职责只包括：
  - 校验当前交互目标是否为合法 merchant
  - 校验 `shop_id` 是否能在 `ShopCatalog` 中找到
  - 构造并 push `ShopMenuScene`
- `ShopInteractionSystem` 不直接处理买卖逻辑，不直接修改 inventory / gold。
- `ShopInteractionSystem` 虽然只直接消费 `ShopCatalog` 做合法性校验，但 Stage 2 仍保持显式注入 `ItemCatalog / ShopTransactionService`：
  - 二者需要继续透传给 `ShopMenuScene`
  - 可避免 system 反向依赖 `GameRuntimeServices` 这类更重的装配知识
  - 也更贴近当前项目已有的显式构造注入习惯
- `ShopMenuScene` 在 Stage 2 只做最小路由骨架，不提前承担 Buy / Sell 列表、数量选择、交易反馈。
- 但 `ShopMenuScene` 的构造注入接口在 Stage 2 就应锁定到后续可扩展形态，避免 Stage 3 再重拆入口：
  - `std::string_view name`
  - `engine::core::Context&`
  - `entt::registry&`
  - `entt::entity player`
  - `std::string shop_id`
  - `const ShopCatalog*`
  - `ItemCatalog*`
  - `ShopTransactionService*`
- Scene 内部应自行持有复制后的 `shop_id_`，不要把 `string_view` 直接悬挂到外部临时对象上。
- `ShopMenuScene` 初始化时重新通过 `ShopCatalog` 查 `ShopData`，并只展示最小信息：
  - `title`
  - `greeting`
  - `cancel / close`
- 关闭路径在 Stage 2 直接锁定：
  - `menu_cancel`
  - 或 UI cancel
  - 调用 `requestPopScene()`
  - 不产生任何交易写回
- RAII 边界保持与现有 runtime 一致：
  - `ShopInteractionSystem` 放在 `GameSystemBundle` 的 `unique_ptr`
  - `ShopMenuScene` 通过 `std::unique_ptr<Scene>` 交给 `PushSceneEvent`
  - 不引入全局单例或静态 scene cache
- Stage 2 不包含：
  - buy list 渲染
  - sell list 渲染
  - 数量确认
  - 钱包 / 背包实时刷新
  - 商店 UI 细节样式

## 需要新增的文件

- `src/game/component/merchant_component.h`
- `src/game/system/shop_interaction_system.h`
- `src/game/system/shop_interaction_system.cpp`
- `src/game/scene/shop_menu_scene.h`
- `src/game/scene/shop_menu_scene.cpp`
- `ui/rmlui/scenes/shop_menu.rml`
- `ui/rmlui/scenes/shop_menu.rcss`
- `tests/game/shop_interaction_system_test.cpp`
- `tests/game/shop_menu_scene_smoke_test.cpp`

## 实现步骤

### Step 1. 定义 MerchantComponent 与地图实例属性入口

- 在 `merchant_component.h` 中定义最小 merchant 数据：
  - `shop_id_`
  - `shop_id_hash_`
- 在 `game/loader/tiled_conventions.h` 中新增 actor property 常量：
  - `ACTOR_PROP_SHOP_ID = "shop_id"`
- 在 `EntityBuilder::buildActor()` 中读取 `shop_id`。
- `EntityBuilder` 不负责查 `ShopCatalog` 做目录合法性校验；它只负责把地图实例属性变成运行时组件。
- 若 `shop_id` 为空字符串或缺失，则不附加 `MerchantComponent`。
- `shop_id` 的检查顺序应放在 `quest_offer_id` 之前，推荐直接使用 `if (shop_id) { ... } else if (quest_offer_id) { ... }` 保证互斥。
- 若同一 actor 同时声明 `shop_id` 与 `quest_offer_id`：
  - 记录 `warn`
  - merchant 优先
  - 仅附加 `MerchantComponent`
  - 跳过 `QuestGiverComponent`

### Step 2. 调整 Interaction owner 与目标优先级

- 在 `InteractionSystem::chooseFacingTarget()` 中新增 merchant 分支：
  - 新增 `best_merchant`
  - 新增 `best_merchant_distance`
- merchant 分支必须插在 quest giver 分支之前。
- 目标选择完成后的最终返回顺序锁定为：
  - merchant
  - quest giver
  - dialogue npc
  - chest
  - rest
- 本阶段不对 `chooseFacingTarget()` 做结构性重构，只做最小插桩。
- 在 `DialogueSystem::onInteractCommand()` 中新增：
  - `if (registry_.all_of<game::component::MerchantComponent>(event.target)) return;`
- 在 `QuestInteractionSystem::onInteractCommand()` 中新增同等 defensive guard：
  - 带 `MerchantComponent` 的实体直接跳过
- 这样即使非法组合实体漏过了 loader，也不会破坏单一 owner 规则。

### Step 3. 实现 ShopInteractionSystem

- `ShopInteractionSystem` 推荐依赖：
  - `entt::registry&`
  - `engine::core::Context&`
  - `const ShopCatalog&`
  - `ItemCatalog&`
  - `ShopTransactionService&`
- `ShopInteractionSystem` 从 `Context` 获取 `dispatcher`，保持与 `RestSystem` 一致。
- `ShopInteractionSystem` 订阅 `InteractCommand`，最小处理规则：
  - 游戏若已暂停，直接返回
  - `event.player / event.target` 无效时返回
  - 目标若没有 `MerchantComponent`，返回
  - `shop_id_hash_ == entt::null` 或 `shop_id_` 为空时返回并记录 `warn`
  - 若 `ShopCatalog.findShop(shop_id)` 失败，返回并记录 `warn`
- 当 merchant 合法时，构造 `ShopMenuScene` 并触发 `PushSceneEvent`。
- Scene 构造时显式注入：
  - 当前玩家实体
  - 当前 `shop_id`
  - `ShopCatalog`
  - `ItemCatalog`
  - `ShopTransactionService`
- `ShopInteractionSystem` 不做额外 debounce；scene stack 与 pause state 继续复用现有机制。

### Step 4. 落最小 ShopMenuScene 路由骨架

- Stage 2 就新增 `ShopMenuScene`，但只实现最小可见骨架，不提前做完整商店 UI。
- `ShopMenuScene` 推荐对齐 `InventoryMenuScene` 的显式构造注入，而不是把 gameplay 依赖都从 `Context` 临时查出来。
- `ShopMenuScene` 的最小成员建议包括：
  - `entt::registry& game_registry_`
  - `entt::entity player_`
  - `std::string shop_id_`
  - `const ShopCatalog* shop_catalog_`
  - `ItemCatalog* item_catalog_`
  - `ShopTransactionService* shop_transaction_service_`
  - `RmlDocumentController`
  - 最小 data model 字段：`shop_title_ / shop_greeting_`
- `init()` 阶段完成：
  - push `engine::input::InputContextId::Menu`
  - 依赖判空
  - `shop_id_` 查表
  - 初始化最小 Rml document
  - 绑定 cancel 行为
- `clean()` 阶段应与现有覆盖式菜单 Scene 对齐：
  - 断开 `menu_cancel`
  - pop input context
  - 恢复先前 game state
  - 卸载 document
- 当前 `SceneManager` 在 `scene->init() == false` 时会直接触发 `QuitEvent`，因此 Stage 2 不应把 `ShopMenuScene::init()` 失败当作常规用户路径。
- 普通配置错误应尽量在 `ShopInteractionSystem` push scene 之前完成拦截；`ShopMenuScene::init()` 中的失败检查仍然保留，但它只用于防御式处理“理论上不该发生”的 runtime 不一致或 UI 初始化硬失败。
- 若 `shop_id` 在 scene init 时仍然无效，应记录错误并返回 `false`，将其视为装配/数据不一致，而不是用 `requestPopScene()` 做常规回退。
- Stage 2 的 `shop_menu.rml` 只要求能验证 route 已打通，建议包含：
  - 标题
  - greeting 文本
  - 一条“Buy / Sell coming soon”类占位提示
  - 关闭按钮或取消提示
- `clean()` / 析构中应断开输入与 UI 绑定，保持与现有 scene 生命周期对齐。
- Stage 3 将在同一 Scene 上继续扩充 Buy ViewModel，而不是推翻这个骨架重写。

### Step 5. 接入 Runtime system 装配

- `game/system/fwd.h` 新增 `ShopInteractionSystem` 前置声明。
- `GameSystemBundle` 新增：
  - `std::unique_ptr<game::system::ShopInteractionSystem> shop_interaction_system`
- `system_bundle.h/.cpp` 补齐声明和 include。
- `GameRuntimeAssembler::assembleSystems()` 中创建 `shop_interaction_system`。
- 创建顺序建议放在：
  - `quest_interaction_system` 之后
  - `interaction_system / rest_system` 同一交互簇附近
- `ShopInteractionSystem` 依赖 Stage 1 已接好的：
  - `shop_catalog`
  - `item_catalog`
  - `shop_transaction_service`
- Stage 2 不新增新的 runtime service；只新增 system 与 scene 路由接线。

### Step 6. 补齐 Stage 2 测试

- `shop_interaction_system_test.cpp` 覆盖：
  - 合法 merchant 触发 `PushSceneEvent`
  - 非法 `shop_id` 不 push scene
  - pause 状态下不 push scene
  - 非 merchant target 不触发
- 交互优先级 case 优先并入现有 [interaction_command_pipeline_test.cpp](/Users/ziyu/Workspace/GameDev/TEST/TinyFarmRPG-feature/tests/game/interaction_command_pipeline_test.cpp)，除非后续用例数明显增长再拆独立测试文件：
  - 面前同时有 merchant / quest giver / dialogue 时，`InteractionSystem` 选 merchant
  - merchant 分支插在 quest giver 前面后，`continue` 互斥逻辑仍成立
- 地图实例属性覆盖建议至少有一条测试，验证：
  - actor `shop_id` 能附加 `MerchantComponent`
  - `shop_id + quest_offer_id` 同时存在时 merchant 胜出
- 若现有 `EntityBuilder` 直测成本过高，可通过一个最小 map/load 集成测试覆盖同样语义，但测试目标不能缺失。
- `shop_menu_scene_smoke_test.cpp` 覆盖：
  - 使用合法 `shop_id` 时 scene 能 init
  - `menu_cancel` 或等价关闭路径能 pop
  - 最小标题 / greeting 能成功绑定到 document

## ToDo

- [ ] 新增 `MerchantComponent` 与 `shop_id` 实例属性约定
- [ ] 在 `EntityBuilder` 中附加 `MerchantComponent`，并锁定 merchant 与 quest giver 的冲突处理
- [ ] 在 `InteractionSystem` 中加入 merchant 目标优先级，且插入位置位于 `QuestGiverComponent` 分支之前
- [ ] 在 `DialogueSystem` 与 `QuestInteractionSystem` 中加入 `MerchantComponent` owner skip
- [ ] 实现 `ShopInteractionSystem` 并接入 `InteractCommand -> PushSceneEvent`
- [ ] 新增最小 `ShopMenuScene` 骨架与 `shop_menu.rml / rcss`
- [ ] 在 `GameRuntimeAssembler / GameSystemBundle` 中接入 `shop_interaction_system`
- [ ] 补齐 merchant 路由、优先级与 scene 打开相关测试

## 疑问

- 暂无必须先澄清的问题；按当前代码结构，Stage 2 可以直接开始实现。
