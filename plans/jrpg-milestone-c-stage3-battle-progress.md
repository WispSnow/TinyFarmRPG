# Milestone C / Stage 3: 战斗击败计数推进细化计划

## 实现思路

- 新增独立的 `QuestBattleProgressResolver`，只负责把 `BattleEndedEvent` 转成 quest progress 写回结果。
- resolver 输入保持最小：
  - `BattleOutcome`
  - `final_units`
  - `QuestCatalog`
  - 玩家 `QuestLogComponent`
- resolver 只处理 `BattleOutcome::Victory`；`Defeat / Escaped / Ongoing` 直接返回空摘要，不推进任务。
- defeated enemy 的判定规则固定为：`side == Enemy && !isAlive() && source_enemy_id.has_value()`。
- defeated enemy counts 先按 `source_enemy_id` 聚合，再匹配 active quest 中的 `DefeatEnemyCount` objective。
- 只推进 active quest；completed quest 和未接取 quest 不参与 battle progress。
- 同一个 enemy kill 可以同时推进多个 active quest / 多个 objective；progress key 仍然统一使用 `(quest_id, objective_id)` 复合 key。
- objective progress 写回推荐在 resolver 内直接落到 `QuestLogComponent`，并返回结构化 summary；不把 battle progress 逻辑塞回 `quest_log_ops`。
- 这与 `BattleRewardResolver` 的“纯计算 -> scene 写回”模式刻意不同：quest progress 写回只是 `QuestLogComponent` 上的原子 int/map 变更，不需要 `InventoryDomainService` 这类中介服务。
- progress 写回建议对每个 objective `clamp` 到 `required_count`，避免计数在完成后继续膨胀。
- 若 active quest 的 progress key 缺失，resolver 用 `0` 作为基线并补写回 map，避免旧存档或测试数据导致 battle progress 失效。
- progress key 缺失的补写回建议只记 `debug`，不升到 `warn`。
- `QuestInteractionSystem` 的 `ReadyToTurnIn` 判定继续复用 `quest_log_ops::isQuestReadyToTurnIn(...)`；Stage 3 只负责把 objective progress 推到正确状态。
- battle progress 写回固定接入 `game_scene_battle_settlement.cpp`，推荐新增 `applyQuestBattleProgress()` 并串进 `processBattleEndedForGameScene()`。
- Stage 3 不再修改 `BattleEndedEvent` 契约，也不在 `GameScene::onBattleEnded()` 开平行 quest 结算路径。
- 现有 `applyVictoryRewards()` 需要拆成“写回并返回 `BattleRewardWritebackResult`，但不立即弹通知”，由 settlement 尾部统一生成一次反馈。
- 战斗奖励反馈与 quest progress 反馈必须合并成同一次 `DialogueShowEvent(channel=1)`，避免同帧覆盖。
- quest battle feedback 保持最小：
  - 有 quest 进度变化时显示 `任务更新：<title>`
  - 某个 quest 本场战斗首次进入 `ReadyToTurnIn` 时显示 `可交付：<title>`
- 反馈文本优先复用现有 `game_scene_reward_feedback.*`，扩展成“奖励 + 任务进度”的统一格式化入口，不新增第二套 battle feedback helper。
- 反馈格式化路径固定选 `(b)`：保留现有 `formatRewardFeedback(...)` 原签名，只新增组合函数 `formatBattleSettlementFeedback(...)`；不把 quest summary 参数直接塞进 reward formatter。

## 需要新增的文件

- `src/game/domain/quest_battle_progress_resolver.h`
- `src/game/domain/quest_battle_progress_resolver.cpp`
- `tests/game/quest_battle_progress_resolver_test.cpp`

## 实现步骤

### Step 1. 定义战斗任务推进摘要类型

- 在 `quest_battle_progress_resolver.h` 中定义最小 summary 类型。
- 同时定义 battle reward 写回结果类型，例如 `BattleRewardWritebackResult`，用于承接从 `applyVictoryRewards()` 拆出来的奖励写回结果。
- summary 推荐至少保留：
  - `updated_quests`
  - `became_ready_to_turn_in_quests`
- quest 级条目建议同时保留 `quest_id / quest_id_hash / quest_title`，方便反馈格式化和后续 UI 复用。
- `quest_title` 在 summary 中直接存 `std::string` 副本，避免把 `QuestCatalog` 生命周期耦合到 formatter / test。
- 若需要断言更细的增量，summary 内可额外保留 objective 级 `before / after / required_count` 信息，但不直接存玩家可见文案。

### Step 2. 实现 Victory-only battle progress resolver

- 新增 `QuestBattleProgressResolver::apply(...)` 或等价自由函数。
- 非 `Victory` 直接返回空摘要。
- 先从 `final_units` 统计 defeated enemy counts，只认 `Enemy + dead + source_enemy_id`。
- 遍历 `QuestLogComponent.active_quests`，从 `QuestCatalog` 查 quest 定义；缺失 quest 定义时 `warn` 并跳过。
- 只处理 `QuestObjectiveKind::DefeatEnemyCount`。
- 对命中的 objective：
  - 读取 progress key 当前值
  - 按 defeated enemy count 累加
  - `clamp` 到 `required_count`
  - 只有 `after > before` 时才记入 summary
- 若 progress key 缺失，按 `0` 基线补写回，并记录 `debug`。
- 某个 quest 在 battle 前未 ready、battle 后变成 ready 时，记入 `became_ready_to_turn_in_quests`。

### Step 3. 接入 battle settlement 编排

- 在 `game_scene_battle_settlement.cpp` 中新增 `applyQuestBattleProgress()`。
- 现有 `applyVictoryRewards()` 改成只做 reward writeback，返回 `BattleRewardWritebackResult`，不再在函数内部调用 `formatRewardFeedback()` 或 `showTimedNotification()`。
- `processBattleEndedForGameScene()` 的推荐顺序固定为：
  1. `applyBattleItemStockDelta()`
  2. 若非 `Victory`，直接结束
  3. 执行奖励写回，得到 `BattleRewardWritebackResult`
  4. 执行 quest battle progress，得到 quest progress summary
  5. 合并两类结果，统一生成一次 battle settlement feedback
- 推荐伪代码骨架固定为：
  1. `applyBattleItemStockDelta(...)`
  2. `if (evt.outcome != Victory) return;`
  3. `const auto reward_result = applyVictoryRewards(...);`
  4. `const auto quest_result = applyQuestBattleProgress(...);`
  5. `const auto feedback = formatBattleSettlementFeedback(reward_result, quest_result, item_catalog);`
  6. `showTimedNotification(..., channel=1, feedback)`
- quest progress 的 runtime truth 固定写回玩家 `QuestLogComponent`。
- 缺少玩家或 `QuestLogComponent` 时至少 `warn`，但不阻断已能执行的奖励写回。

### Step 4. 合并 battle feedback

- 保留现有 `formatRewardFeedback(...)` 原签名不变。
- 在 `game_scene_reward_feedback.*` 中新增组合函数 `formatBattleSettlementFeedback(...)`，内部先复用 `formatRewardFeedback(...)`，再追加 quest progress 行。
- 奖励文本规则保持不变；quest progress 行追加到奖励文本之后。
- quest feedback 推荐格式：
  - `任务更新：<quest_title>`
  - `可交付：<quest_title>`
- 若本场既无奖励写回、也无 quest progress 变化，仍回退到现有 `战斗胜利`。
- 通知继续固定走 `DialogueShowEvent(channel=1)`，只发一次。

### Step 5. 补齐测试

- `Victory` 时能正确统计 multiple defeated enemies 并推进 objective progress。
- 相同 `enemy_id` 的多个 defeated units 会分别计数，不按 enemy id 去重为单次击杀。
- 缺少 `source_enemy_id` 的 defeated enemy 会被跳过。
- 非 `Victory` 不推进 quest progress。
- inactive quest / completed quest 不会被 battle resolver 修改。
- progress key 缺失时会按 `0` 基线补写回。
- objective progress 会 `clamp` 到 `required_count`。
- quest 首次进入 ready-to-turn-in 时会出现在 summary 中。
- `processBattleEndedForGameScene()` 在奖励和 quest 都变化时只发一次 `DialogueShowEvent(channel=1)`。
- battle feedback 会同时包含奖励行和 quest progress 行。
- 现有 `game_scene_battle_reward_writeback_test.cpp` 在“无 active quest”场景下应继续通过，因为 quest progress 为空、不追加文本。
- 由于选择新增 `formatBattleSettlementFeedback(...)`，现有 `formatRewardFeedback(...)` 调用点与 reward formatter 测试不需要签名适配。

## ToDo

- [ ] 新增 `QuestBattleProgressResolver` 与 battle progress summary 类型
- [ ] 固定 `Victory-only` 的 defeated enemy 统计规则
- [ ] 只推进 active quest，跳过 completed / inactive quest
- [ ] 用复合 progress key 写回 objective progress，并对结果 `clamp` 到 `required_count`
- [ ] 把 `applyVictoryRewards()` 拆成“写回并返回 `BattleRewardWritebackResult`，但不立即通知”
- [ ] 在 `game_scene_battle_settlement.cpp` 新增 `applyQuestBattleProgress()`
- [ ] 把 battle settlement 反馈改成奖励与 quest progress 的单次合并通知
- [ ] 保持 `formatRewardFeedback(...)` 原签名不变，并新增 `formatBattleSettlementFeedback(...)`
- [ ] 继续复用 `DialogueShowEvent(channel=1)`，禁止 quest feedback 另开并行通知
- [ ] 补齐 resolver、battle settlement、feedback 合并相关测试
