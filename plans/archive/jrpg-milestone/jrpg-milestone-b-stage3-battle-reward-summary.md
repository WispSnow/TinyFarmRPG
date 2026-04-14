# Milestone B / Stage 3: 战斗奖励汇总细化计划

## 实现思路

- 在 `game::battle` 层新增独立的 `BattleRewardResolver`，只负责把战斗结束态转换成奖励摘要。
- resolver 输入保持最小：`BattleOutcome`、`final_units`、`RpgCatalog`，以及可注入的掉落随机回调。
- resolver 输出统一的 `BattleRewardSummary`，包含：
  - `gold_total`
  - `exp_total`
  - `item_drops`
- 只在 `BattleOutcome::Victory` 时产出标准奖励；`Defeat / Escaped / Ongoing` 直接返回空摘要。
- defeated enemy 的判定规则固定为：`side == Enemy && !isAlive()`。
- 只有带 `source_enemy_id` 的敌人参与奖励结算；缺来源或目录查表失败时直接跳过，不做猜测。
- 金币、经验直接累加；掉落按每个 defeated enemy 的 `drops_` 独立投掷。
- 奖励汇总按 defeated unit 遍历，不按 `source_enemy_id` 去重；同一 `enemy_id` 的多个 troop 成员被击败时要分别结算。
- `exp_total` 保持 `int`，与 `EnemyData::exp_reward_` 一致。
- `item_drops` 同时保留字符串 id 和 hash，方便 Stage 4 同时做入包与奖励反馈：
  - `std::string item_id`
  - `entt::id_type item_id_hash`
  - `int count`
- Stage 3 不扩展 `BattleEndedEvent`，也不改 `GameScene::onBattleEnded()`；写回和反馈留到 Stage 4。

## 需要新增的文件

- `src/game/battle/battle_reward_resolver.h`
- `src/game/battle/battle_reward_resolver.cpp`
- `tests/game/battle/battle_reward_resolver_test.cpp`

## 实现步骤

### Step 1. 定义奖励摘要类型

- 在 `battle_reward_resolver.h` 中定义 `BattleRewardSummary`。
- `BattleRewardSummary` 推荐保持：
  - `int gold_total`
  - `int exp_total`
  - `std::vector<ItemDrop> item_drops`
- `item_drops` 使用最小结构，如 `{ item_id, item_id_hash, count }`。
- 同一物品的多次掉落在 summary 中合并计数，不保留“第几只敌人掉的”来源信息。

### Step 2. 实现 Victory-only 汇总器

- 新增 `BattleRewardResolver::resolve(...)` 或等价自由函数。
- 非 `Victory` 直接返回空摘要。
- 遍历 `final_units`，只处理已死亡且有 `source_enemy_id` 的敌方单位。
- 明确按 unit 逐个结算，不对 `source_enemy_id` 做 unique 化。
- 从 `EnemyData` 累加 `gold_reward_`、`exp_reward_`，并解析 `drops_`。

### Step 3. 注入掉落随机

- resolver 内部提供默认随机源，接口允许测试注入固定 roll 回调。
- 推荐锁定签名：
  - `using DropRollFn = std::function<float()>;`
  - 返回值语义为 `[0, 1)`。
- 每条 `EnemyDropData` 单独调用一次 `roll()`；`roll() < chance_` 则命中。
- 不在 Stage 3 引入“掉落数量区间”“保底掉落”或额外掉落策略。

### Step 4. 补齐单元测试

- `Victory` 时能正确累加多名敌人的 `gold_total / exp_total`。
- 相同 `source_enemy_id` 的多个 defeated units 会分别结算奖励。
- 同一物品来自多名敌人时会在 `item_drops` 中合并计数。
- 缺少 `source_enemy_id` 的 defeated enemy 会被跳过。
- `source_enemy_id` 查表失败时会被跳过。
- `Defeat / Escaped` 返回空摘要。
- 注入固定 roll 序列后，多个 drop 的独立判定结果可稳定断言。

## ToDo

- [ ] 新增 `BattleRewardSummary`，并为掉落条目同时保留 `item_id` 与 `item_id_hash`
- [ ] 新增 `BattleRewardResolver` 并完成 Victory-only 汇总逻辑
- [ ] 为掉落判定提供 `DropRollFn` 可注入随机回调
- [ ] 明确按 defeated unit 结算，禁止按 `source_enemy_id` 去重
- [ ] 补齐奖励汇总单元测试
- [ ] 保持 `BattleEndedEvent` / `GameScene` 不在 Stage 3 提前接线
