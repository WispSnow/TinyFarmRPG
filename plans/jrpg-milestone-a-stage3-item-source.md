# Milestone A / Stage 3: 物品候选数据接线计划

## Context

Stage 1 已完成战斗菜单骨架，Stage 2 已完成技能候选数据链路：

- `BattleScene` 已有 `MainMenu / SkillList / ItemList / TargetSelect` 子状态机
- `BattleScene` 已用 `ListEntryViewModel` 承载技能/物品列表候选
- `BattleScene` 已通过 `ActionDraft` 保存 `selected_skill_id` / `selected_item_id` / `selected_target_id`
- `BattleUnit.skill_ids` 已成为技能候选来源
- `SkillList -> TargetSelect -> cancel` 已明确保持 `list_entries_` 不变

当前 Stage 3 要解决的是物品列表的数据来源、战斗物品数据语义、以及物品消耗后的库存真相来源。

现状约束：

- `GameScene::onEnterBattleCommand()` 会把玩家背包聚合成 `BattleSessionOptions::item_stocks`
- `BattleSession` 会把 `item_stocks` 拷入 `BattleRuntimeState::item_stocks`
- `BattleActionResolver` 已支持 `BattleActionType::Item`，但当前只读取 `ItemData::on_use_`
- `ItemData::on_use_` 当前用于场外物品使用，效果类型只有 `AddItem`
- `assets/data/item_config.json` 中已有 crop -> seed 的 `on_use`，不能直接视为战斗可用物品
- `ItemData` 当前只保留 hash id，没有保留原始 string id；但 `BattleAction.item_id` 需要 string id
- `BattleEndedEvent` 当前只带 outcome 和 final units，没有携带战斗后的物品库存

因此，Stage 3 的核心是建立一条最小、明确、可测试的物品来源链路：

`Inventory aggregate stocks -> BattleRuntimeState.item_stocks -> BattleScene ItemList -> ActionDraft.selected_item_id`

并补齐后续提交时不会丢库存变化的基础设施：

`BattleRuntimeState.item_stocks -> BattleEndedEvent.remaining_item_stocks -> GameScene inventory delta writeback`

## 范围

### 本阶段包含

- 为 `ItemData` 保留原始 string id，供 UI / `BattleAction.item_id` 使用
- 新增独立的 `battle_use` 配置，不复用场外 `on_use`
- 为战斗物品定义最小目标 scope 与恢复效果
- 在 `BattleActionResolver` 中改用 `battle_use` 处理 `BattleActionType::Item`
- 暴露 `BattleSession` 当前战斗物品库存快照
- 在 `BattleScene` 中根据 `item_stocks + ItemCatalog` 生成 `ItemList`
- 根据库存数量 / `battle_use` / scope 计算物品 entry enabled 状态
- 鼠标点击和 `menu_confirm` 选择物品时写入 `ActionDraft::selected_item_id`
- 增加 `BattleEndedEvent` 的 remaining item stocks，并由 `GameScene` 写回真实背包 delta
- 补充 item catalog / resolver / session / GameScene / BattleScene smoke 测试

### 本阶段不包含

- 真实目标候选生成
- `ItemList -> TargetSelect -> final BattleAction` 的 UI 提交闭环
- 技能和攻击的目标选择
- 物品图标显示
- 复活、解除状态、添加状态、造成伤害类战斗物品
- 战斗奖励和掉落结算
- 战斗中打开完整背包或分页

## 设计决策

### 1. 新增 `battle_use`，不复用 `on_use`

当前 `on_use` 是场外物品使用语义，例如 crop 消耗后添加 seed。若 Stage 3 直接筛选 `on_use_.has_value()`，会把草莓/土豆这类“农场转换物”暴露到战斗物品菜单中。

推荐新增独立字段：

```json
{
  "id": "item.potion",
  "category": "consumable",
  "battle_use": {
    "consume": 1,
    "scope": "one_ally",
    "effects": [
      { "type": "recover_hp", "amount": 50 }
    ]
  }
}
```

建议 C++ 数据结构：

```cpp
enum class BattleItemEffectType {
    RecoverHp,
    RecoverMp,
    Unknown
};

struct BattleItemEffect {
    BattleItemEffectType type{BattleItemEffectType::Unknown};
    int amount{0};
};

struct BattleItemUseConfig {
    int consume{1};
    game::data::Scope scope{game::data::Scope::None};
    std::vector<BattleItemEffect> effects{};
};

struct ItemData {
    entt::id_type id_{entt::null};
    std::string id_str_{};
    ...
    std::optional<ItemUseConfig> on_use_{};
    std::optional<BattleItemUseConfig> battle_use_{};
};
```

Stage 3 只解析 `recover_hp` / `recover_mp`。后续状态类、复活类、伤害类物品另开阶段或并入 Stage 4+。

当前项目已有三套相关 effect 语义：

- `game::data::EffectType` in `rpg_types.h`: `RecoverHp / RecoverMp / AddState / RemoveState / AddItem`
- `ItemUseEffectType` in `item_catalog.h`: `AddItem / Unknown`
- Stage 3 新增的 `BattleItemEffectType`: `RecoverHp / RecoverMp / Unknown`

Stage 3 先保留 `BattleItemEffectType` 作为最小战斗物品语境，避免把场外 `on_use` 和技能 effect 的语义混在一起。后续如果 battle item 需要支持 `AddState / RemoveState / Damage` 等效果，应考虑与 `EffectType` 收敛，避免 effect type enum 继续膨胀。

### 2. `battle_use` 配置非法时硬失败

`on_use` 当前对未知 effect 采取 warn + skip 的宽松策略；Stage 3 不需要沿用这个行为。

`battle_use` 是战斗闭环入口，建议非法配置直接让 `ItemCatalog::loadItemConfig()` 返回 false：

- `battle_use` 不是 object: 失败
- `consume <= 0`: 失败
- `scope` 缺失或非法: 失败
- `scope == none`: 允许加载，但 UI disabled；后续若有 passive/item tag 再改为跳过
- `effects` 缺失或不是数组: 失败
- effect type 未知: 失败
- `recover_hp` / `recover_mp` 的 `amount <= 0`: 失败

### 3. BattleScene 保存 `ItemCatalog` 指针

当前 `BattleSessionOptions.item_catalog` 只传给 resolver，`BattleScene` 自己无法查 `ItemData`。

Stage 3 推荐在 `BattleScene` 中新增非 owning 指针：

```cpp
const game::data::ItemCatalog* item_catalog_{nullptr};
```

构造时在 move `BattleSessionOptions` 前复制：

```cpp
BattleScene::BattleScene(..., game::battle::BattleSessionOptions session_options)
    : engine::scene::Scene(name, context),
      rpg_catalog_(session_options.rpg_catalog),
      item_catalog_(session_options.item_catalog),
      session_(std::move(units), std::move(session_options)) {
}
```

注意声明顺序：

- `rpg_catalog_` / `item_catalog_` 都应声明在 `session_` 之前
- 构造函数初始化列表顺序与声明顺序一致，避免 `-Wreorder`

### 4. ItemList 从战斗运行时库存生成

不要直接从 `InventoryComponent` 读列表；进入战斗时已经建立了 `BattleRuntimeState::item_stocks` 快照，后续 resolver 消耗的也应是这份战斗内库存。

推荐为 `BattleSession` 增加只读访问器：

```cpp
[[nodiscard]] const std::unordered_map<entt::id_type, int>& itemStocks() const;
```

`BattleScene::populateItemEntries()` 使用：

- `session_.itemStocks()`
- `item_catalog_->listItems()`
- `ItemData::id_str_`
- `ItemData::battle_use_`

为保证 UI 和测试稳定，推荐遍历 catalog items 后按 `display_name_` fallback `id_str_` 排序，再根据 `itemStocks()` 过滤当前数量。

字段映射：

- `entry_index`: 当前列表索引
- `entry_id`: `ItemData::id_str_`
- `label`: `ItemData::display_name_`，为空时 fallback 到 `id_str_`
- `sublabel`: `x<count>`；如果 `consume > 1` 可显示 `x<count> / Use <consume>`
- `enabled`: `count >= battle_use.consume && battle_use.scope != Scope::None`

推荐行为：

- `item_catalog_ == nullptr`: `ItemList` 显示空列表，文案为 `"No battle items available"`
- `itemStocks()` 为空: 显示空列表
- stock 中 item id 在 catalog 中不存在: 跳过并 warn
- item 没有 `battle_use_`: 不显示
- `Scope::None`: 可显示但 disabled；后续如果引入 passive/field-only 标记，直接跳过

### 5. queueItemAction 采用和 Stage 2 相同的 populate 路径

Stage 2 已明确：真实列表填充后不能调用会清空 backing vector 的 `enterListMenu(...)`。

Stage 3 对 `queueItemAction()` 采用同样路径：

- 清空并重建 `list_entries_`
- 设置 `list_entry_cursor_`
- 直接调用 `setMenuState(MenuState::ItemList)`
- 不在 populate 后调用 `enterListMenu(MenuState::ItemList)`

`ItemList -> TargetSelect` 也沿用 Stage 2 不变量：

- 只修改 `target_entries_` 与 visible flags
- 不清空 `list_entries_`
- cancel 只清除 `selected_target_id`
- 保留 `pending_type == Item` 与 `selected_item_id`

### 6. 物品选择只写草稿，不在 Stage 3 通过 UI 提交

Stage 3 的 UI 边界是“真实物品列表 + 选择上下文”，不实现目标生成和最终提交。最终的 `BattleAction` 提交仍放在 Stage 4 和技能/攻击一起收敛。

选择 enabled 物品时：

- `action_draft_.pending_type = BattleActionType::Item`
- `action_draft_.selected_item_id = entry.entry_id`
- `action_draft_.selected_skill_id.reset()`
- `action_draft_.selected_target_id.reset()`
- 根据 `battle_use.scope` 写入 `action_draft_.requires_target_selection`
- 暂不构造 `BattleAction`
- 暂不调用 `submitAction()`

若需要让 UI 有可见流转，推荐进入 `TargetSelect` 占位态：

- `OneEnemy / OneAlly`: 进入 `TargetSelect`，显示 `"Target selection coming in Stage 4"`
- `Self / AllEnemies / AllAllies`: 暂时留在 `ItemList` 并更新 `menu_hint_`，或同样进入占位态；不要在 Stage 3 直接提交
- `Scope::None`: disabled，不可选择

### 7. Resolver 改用 `battle_use`，补最小恢复效果

`BattleActionResolver::resolve(Item)` 推荐改为：

- 查 `ItemData::battle_use_`，没有则 rejected: `"item cannot be used in battle"`
- 校验 `item_stocks[item_id] >= consume`
- 根据 `battle_use.scope` 收集目标，目标规则先复用 Skill 的 scope 语义
- 对目标应用 `RecoverHp / RecoverMp`
- 更新 `BattleActionResult::hp_recovered` / `mp_recovered`
- 消耗库存
- `turn_core.refresh()` 并推进回合

消耗时机建议：

- 先校验 catalog / stock / target / effect
- 校验通过后再扣库存
- 避免目标非法时消耗物品

注意：Stage 3 可以在 resolver/session 测试里直接提交 `BattleActionType::Item`，但 `BattleScene` UI 仍不直接提交。

### 8. 战斗库存写回采用 BattleEndedEvent delta

推荐选择战斗结束写回，而不是让 `BattleScene` 直接持有 `InventoryDomainService`：

- `BattleSession` 暴露 `itemStocks()`
- `BattleEndedEvent` 增加 `remaining_item_stocks`
- `BattleScene::requestBattleEnd()` 把 `session_.itemStocks()` 填入事件
- `GameScene` 在进入战斗前保存 `active_battle_initial_item_stocks_`
- `GameScene::onBattleEnded()` 计算 `remaining - initial` delta
- delta < 0: 通过 `InventoryDomainService::removeItem(player, item_id, -delta)` 从真实背包扣除
- delta > 0: 通过 `InventoryDomainService::addItem(player, item_id, delta)` 写回战斗中产生的新物品

需要新增 GameScene 成员：

```cpp
std::unordered_map<entt::id_type, int> active_battle_initial_item_stocks_{};
bool has_active_battle_item_stocks_{false};
```

写回 helper 建议：

- `findPlayerEntityWithInventory()`
- `applyBattleItemStockDelta(const std::unordered_map<entt::id_type, int>& remaining_stocks)`

若玩家实体或 `InventoryDomainService` 不可用：

- 记录 warn
- 清空 active snapshot
- 不崩溃

## 实现步骤

### Step 1: 扩展 ItemData battle_use schema

修改：

- `src/game/data/item_catalog.h`
- `src/game/data/item_catalog.cpp`
- `assets/data/item_config.json`
- 测试 fixture 中的 item json

要点：

- `ItemData` 增加 `id_str_`
- `ItemData` 增加 `battle_use_`
- 解析 `battle_use.consume`
- 解析 `battle_use.scope`，复用 `game::data::scopeFromString`
- 解析 `battle_use.effects[]`
- Stage 3 只支持 `recover_hp` / `recover_mp`
- 至少为 demo / debug test battle 路径配置一个可验证的 battle item
- 不建议给 crop -> seed 这类已有 `on_use` 农场物品直接追加 `battle_use`，除非它确实也是战斗消耗品；否则容易模糊场外/战斗语义
- 如果新增 potion，id 命名按 `item_config.json` 现有风格决定，不强制使用 `item.potion`
- 确保进入 debug panel test battle 时玩家战斗库存里至少有一个 battle item，可选方案：
  - 在 debug battle 入口显式注入 `item_stocks`
  - 给玩家初始背包/测试存档配置 battle item
  - 为一个真正合理的现有 consumable 配置 `battle_use`
- 更新 `tests/game/battle/battle_catalog_fixture.h` 中的 `item.potion`：将旧 `on_use` 改为 `battle_use`，或在保留 `on_use` 的同时追加 `battle_use`
- 同步调整依赖该 fixture 的 resolver/session 测试，不再验证 `empty_bottle` 产出，改为验证 HP/MP 恢复和 battle stock 消耗

### Step 2: 扩展 BattleSession / BattleEndedEvent 库存访问

修改：

- `src/game/battle/battle_session.h`
- `src/game/battle/battle_session.cpp` 如需要
- `src/game/defs/events.h`
- `src/game/scene/battle_scene.cpp`

要点：

- 增加 `BattleSession::itemStocks() const`
- `BattleEndedEvent` 增加 `remaining_item_stocks`
- `BattleScene::requestBattleEnd()` 填入 `remaining_item_stocks`
- 保持 `final_units` 现有语义不变

### Step 3: BattleScene 生成 ItemList

修改：

- `src/game/scene/battle_scene.h`
- `src/game/scene/battle_scene.cpp`

要点：

- 新增 `item_catalog_` 非 owning 指针
- 新增 helper：
  - `populateItemEntries()`
  - `handleItemEntry(const ListEntryViewModel& entry)`
  - `isItemEntryEnabled(int stock_count, const BattleItemUseConfig& use)`
  - `itemSubtitle(int stock_count, const BattleItemUseConfig& use)`
  - `findBattleItemByEntryId(...)`
- `findBattleItemByEntryId(...)` 需要把 `ListEntryViewModel::entry_id` 通过 `game::data::RpgCatalog::hashId(entry.entry_id)` 转成 `entt::id_type`，再查 `session_.itemStocks()` / `ItemCatalog::findItem()`
- `queueItemAction()` populate 后直接 `setMenuState(MenuState::ItemList)`
- `handleListEntry()` 在 `MenuState::ItemList` 时分派到 `handleItemEntry`
- 选择 item 后写 `ActionDraft::selected_item_id`
- 不直接提交 `BattleAction`

### Step 4: Resolver 支持 battle_use 恢复效果

修改：

- `src/game/battle/battle_action_resolver.cpp`
- 相关 resolver/session 测试

要点：

- `BattleActionType::Item` 使用 `ItemData::battle_use_`
- target 收集逻辑使用 battle item scope
- `recover_hp` / `recover_mp` 修改目标 unit 数值
- 更新 `BattleActionResult::hp_recovered` / `mp_recovered`
- 成功后消耗库存并推进回合
- 目标或库存非法时不消耗
- 更新 `tests/game/battle/battle_catalog_fixture.h` 的 `item.potion` fixture，使它具备 `battle_use`
- 更新 `BattleActionResolverTest.ItemConsumesStockAndTriggersOnUseEffects`：切到 `battle_use` 后不再断言 `item.empty_bottle` 增加，改为断言目标 HP/MP 恢复、`item.potion` 库存扣除、回合推进
- 更新 `BattleSessionTest` 中提交 `item.potion` 的用例，确保 action 在新的 `battle_use` 语义下仍为 `Applied`

### Step 5: GameScene 写回战斗库存 delta

修改：

- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- 相关 GameScene battle entry 测试

要点：

- 进入战斗前保存初始 aggregate stocks
- 战斗结束时读取 `BattleEndedEvent.remaining_item_stocks`
- 通过 `services_->inventory_domain_service` 访问 `InventoryDomainService`
- player entity 查找复用 `collectPlayerItemStocks()` 中的 `registry.view<game::component::PlayerTag, game::component::InventoryComponent>()` 模式
- 用 `InventoryDomainService` 应用 delta
- 清理 active snapshot
- 对缺失 player / inventory domain service 做 warn + skip

### Step 6: 补测试

建议覆盖：

- `ItemCatalogTest` 或现有 item 相关测试：
  - `loadItemConfig()` 能读取 `id_str_`
  - `battle_use` 能读取 scope / consume / recover effect
  - 非法 `battle_use` 会加载失败
  - 只有 `on_use`、没有 `battle_use` 的物品不会被视为战斗物品

- `BattleActionResolverTest`:
  - item recover hp 生效并消耗库存
  - item recover mp 生效
  - stock 不足时 rejected 且不消耗
  - target 非法时 rejected 且不消耗
  - 没有 `battle_use` 的 item rejected

- `BattleSessionTest`:
  - `itemStocks()` 能反映 resolver 消耗后的剩余库存

- `BattleSceneSmokeTest`:
  - `BattleScene` 保留 `item_catalog_` 指针
  - `queueItemAction()` 通过 session item stocks + catalog 填充 `list_entries_`
  - `handleListEntry()` 在 `ItemList` 写入 `selected_item_id`
  - Stage 3 不恢复旧的 `enterListMenu(MenuState::ItemList)` 空列表路径
  - Stage 3 不直接提交 item `BattleAction`

- `GameSceneBattleEntryTest`:
  - `BattleEndedEvent` 携带 remaining item stocks
  - `GameScene` 记录初始 battle item stocks
  - `GameScene` 在 battle end 通过 inventory domain service 应用 delta

## ToDo

- [ ] 为 `ItemData` 增加原始 string id 字段
- [ ] 新增并解析 `battle_use` schema
- [ ] 为 demo item 配置至少一个战斗恢复物品
- [ ] 确保 debug/test battle 运行时能拿到至少一个 battle item 以验证 `ItemList` 非空
- [ ] 更新 `battle_catalog_fixture.h` 中的 `item.potion` 和依赖它的 resolver/session 测试
- [ ] `BattleActionResolver` 改用 `battle_use_`，并支持 `RecoverHp / RecoverMp`
- [ ] `BattleSession` 暴露只读 `itemStocks()`
- [ ] `BattleEndedEvent` 携带 `remaining_item_stocks`
- [ ] `BattleScene` 保存 `item_catalog_` 非 owning 指针
- [ ] 实现 `ItemList` 的真实 `list_entries_` 构建
- [ ] `queueItemAction()` populate 后直接 `setMenuState(MenuState::ItemList)`
- [ ] 选择 enabled 物品时写入 `ActionDraft::selected_item_id`
- [ ] 保持 Stage 3 UI 不直接提交 item `BattleAction`
- [ ] `TargetSelect` 占位态 cancel 保持 `list_entries_` 与 `selected_item_id`
- [ ] `GameScene` 保存 battle 初始库存并在 battle end 应用 delta
- [ ] 补充 item catalog / resolver / session / GameScene / BattleScene smoke 测试

## 完成标准

- `Item` 菜单不再固定显示空列表，而是展示当前战斗库存中的真实 battle items
- 只有带 `battle_use` 的物品会出现在战斗物品菜单
- item entry 至少显示名称和数量
- 库存不足或 `Scope::None` 的物品不可确认
- 选择物品会写入动作草稿，但不会绕过 Stage 4 直接提交
- resolver 直接提交 battle item action 时能正确恢复 HP/MP、消耗库存并推进回合
- 战斗结束事件携带剩余库存，`GameScene` 能把 delta 写回真实背包
- 不把场外 `on_use` crop -> seed 物品误暴露为战斗物品
- `ninja -C build/debug tests/game_tests` 通过
- `ctest --test-dir build/debug --output-on-failure` 通过
