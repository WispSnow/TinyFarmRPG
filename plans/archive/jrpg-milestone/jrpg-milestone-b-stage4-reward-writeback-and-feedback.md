# Milestone B / Stage 4: GameScene 奖励写回与最小反馈细化计划

## 实现思路

- Stage 4 直接消费 Stage 3 的 `BattleRewardResolver`，由 `GameScene::onBattleEnded()` 完成最终写回。
- `BattleRewardResolver` 在 `onBattleEnded()` 中局部构造，直接使用默认随机源；本阶段不放进 `services_` 或 `GameScene` 成员。
- 处理顺序固定为：
  1. 先执行 `applyBattleItemStockDelta()`，对所有 outcome 保留战斗内物品消耗。
  2. 若 `evt.outcome != Victory`，直接结束，不发标准奖励。
  3. 若 `Victory`，调用 `BattleRewardResolver` 生成 `BattleRewardSummary`。
  4. 把 `gold_total` 写入玩家的 `PlayerWalletComponent`。
  5. 把 `item_drops` 逐条写入 `InventoryDomainService`。
  6. 通过现有 `DialogueShowEvent` 通知通道给玩家一个最小奖励反馈。
- `GameScene` 继续做编排与写回，不把 reward resolver 逻辑反向塞回 scene。
- `exp_total` 在本阶段仍然不持久化，也不进入玩家反馈文案，避免出现“显示获得经验但实际无成长”的假闭环。
- 奖励反馈优先复用现有通知链：`DialogueShowEvent + channel=1`。
- 反馈文本建议抽成纯函数，例如 `formatRewardFeedback(...) -> std::string`，避免把文本规则硬塞进 `onBattleEnded()`。
- 反馈文本里物品名优先走 `services_->item_catalog->findItem(drop.item_id_hash)`；查不到时回退到 `drop.item_id`。
- 掉落入包若出现 `rejected > 0`，必须 `warn`，并在反馈里明确提示未入包数量，不能静默丢失。
- `DialogueShowEvent::world_position` 明确来自 player entity 的位置；优先复用 `computeHeadPosition(registry_, player)` 这条现有头顶通知路径，本质上仍是从 `TransformComponent` 读取坐标。

## 需要新增的文件

- 无强制新增运行时代码文件，优先在 `GameScene` 内完成接线。
- 若要把反馈文本做成独立可测 helper，推荐新增：
  - `src/game/scene/game_scene_reward_feedback.h`
  - `src/game/scene/game_scene_reward_feedback.cpp`
- 推荐新增测试文件：
  - `tests/game/game_scene_reward_feedback_test.cpp`
  - `tests/game/game_scene_battle_reward_writeback_test.cpp`

## 实现步骤

### Step 1. 接入 Victory 奖励主线

- 在 `game_scene.cpp` 引入 `battle_reward_resolver.h`。
- `onBattleEnded()` 在物品库存 delta 写回后，增加 `Victory` 分支。
- `Victory` 分支中局部构造 `BattleRewardResolver resolver{};`，直接使用默认随机源。
- 非 `Victory` 明确不执行金币/掉落写回。

### Step 2. 落地金币与掉落写回

- `Victory` 分支先解析玩家实体。
- 金币写回目标固定为 `PlayerWalletComponent::gold_`。
- 掉落统一走 `InventoryDomainService::addItem(player, item_id_hash, count)`。
- `PlayerWalletComponent` 缺失时至少 `warn`，但不影响已能执行的掉落入包。
- `InventoryDomainService` 或玩家背包缺失时至少 `warn`，并跳过对应写回。

### Step 3. 复用通知通道做最小反馈

- 反馈文本保持最小汇总，不新增结算 Scene。
- 建议把文本拼接抽成纯函数，如 `formatRewardFeedback(summary, writeback_results, item_catalog)`。
- 推荐格式为多行摘要：
  - 金币一行
  - 每个成功入包的掉落一行
  - 若有 rejected 掉落，再追加“背包已满/未获得”行
- 掉落显示名明确通过 `item_catalog->findItem(item_id_hash)` 读取 `display_name_`。
- 通知目标直接复用 player entity。
- `DialogueShowEvent::world_position` 明确使用 player 的 `TransformComponent` 推导；优先复用现有 `computeHeadPosition(registry_, player)` helper。

### Step 4. 补齐写回测试

- 纯格式化函数可单测，不必每次都启动完整 `GameScene`。
- `Victory` 时会累加金币，并把掉落写进背包。
- `Defeat / Escaped` 不写金币、不发标准掉落。
- 掉落部分入包失败时会保留 accepted 部分，并对 rejected 部分给出显式反馈。
- `exp_total` 在 Stage 4 不写存档、不改玩家状态。

## ToDo

- [x] 在 `GameScene::onBattleEnded()` 接入 `BattleRewardResolver`
- [x] 明确在 `onBattleEnded()` 内局部构造 `BattleRewardResolver`
- [x] 固定 `Victory / Defeat / Escaped` 的奖励写回规则
- [x] 把金币写入 `PlayerWalletComponent`
- [x] 把掉落统一接到 `InventoryDomainService`
- [x] 让奖励反馈位置明确走 player 的 `TransformComponent/head position`
- [x] 抽出可测的 `formatRewardFeedback(...)` 纯函数
- [x] 复用 `DialogueShowEvent` 做最小奖励反馈
- [x] 对掉落 reject 场景补 `warn` 与玩家可见提示
- [x] 补齐 Stage 4 写回测试
