# Milestone B: 敌人 AI 与战斗奖励索引计划

## Context

Milestone A 已经完成了战斗菜单可玩化闭环，当前战斗系统具备这些基础：

- `BattleScene` 已完成 `MainMenu / SkillList / ItemList / TargetSelect` 菜单子状态机
- `BattleSession` / `BattleActionResolver` / `TurnCore` 已能稳定处理 `Attack / Skill / Item / Guard / Escape / EndTurn`
- `RpgCatalog` 已能加载 `EnemyData::actions_ / exp_reward_ / gold_reward_ / drops_`
- `GameScene` 已能处理战斗入口、场景 push/pop，以及战斗物品库存写回

Milestone B 当前真正缺失的是“敌方自动行动 + 胜利奖励闭环”：

- 当前 `BattleScene` 仍把任意当前行动者都视为可输入对象，敌方回合还没有自动生成 `BattleAction`
- `BattleUnit` 当前只保留战斗数值和 `skill_ids`，没有保留 `enemy_id` / `actor_id` 这类来源信息
- `buildBattleUnitsFromCatalog()` 会把 `EnemyActionData::rating_` 压扁成去重后的 `skill_ids`，AI 无法直接复用敌人目录中的权重信息
- `BattleEndedEvent` 目前只负责 `outcome / final_units / remaining_item_stocks`，`GameScene` 也只做了物品库存 delta 写回
- `SaveData::PlayerSaveData` 虽然已有 `gold` 字段，但 `SaveService::capture()/apply()` 还没有把它与运行时状态打通，`InventoryMenuScene` 仍显示 `Gold: --`

因此，Milestone B 建议只解决这几件事：

- 让敌方回合不再进入玩家菜单，而是自动生成并提交行动
- 让胜利后能基于被击败敌人稳定结算金币与掉落
- 让金币和掉落真正写回探索态，而不是只停留在战斗临时结果里
- 为后续任务、商店、角色成长保留可复用的奖励汇总边界

## 范围

### 本阶段包含

- 战斗单位来源信息补齐，支持从战斗单位稳定反查目录数据
- 敌方最小 AI 行动规划
- 战斗奖励汇总：金币、掉落，以及可选的经验汇总值
- 胜利后的掉落入包与金币写回
- 金币运行时真相与最小 UI/存档接线
- 对应的单元测试与 smoke test

### 本阶段不包含

- 完整等级成长、升级、加点、职业成长线
- 复杂 AI 条件树、仇恨系统、弱点分析、技能连携
- 独立的战斗结算界面或长战斗日志系统
- 中途存档恢复战斗（`combat_state.pending_battle` 的完整运行时闭环）
- 任务推进与商店规则

## 实现思路

Milestone B 不适合直接按“先做 AI，再做奖励”线性推进。

原因：

- AI 和奖励都依赖同一个基础问题：战斗单位必须保留稳定的目录来源信息
- 若不先处理来源信息，AI 拿不到 `EnemyActionData::rating_`，奖励也拿不到 `EnemyData::gold_reward_ / drops_`
- 金币写回不能只停在 `SaveData` 字段层，因为当前运行时根本没有可靠的金币真相来源

因此更稳的推进方式是：

1. 先补齐战斗单位来源信息与金币真相边界
2. 再把敌方行动规划抽成独立 helper，而不是塞进 `BattleScene` UI 分支里
3. 再把奖励汇总做成独立 resolver，而不是让 `GameScene` 直接散落目录查表与随机掉落逻辑
4. 最后做 `GameScene` 写回、UI 可见性和测试补强

同时建议锁定这些边界：

- `BattleScene` 继续只做表现层编排，不直接拥有掉落写包或金币写入逻辑
- `GameScene` 继续拥有探索态真相写回职责
- 敌方 AI 与奖励计算优先做成 `game::battle` 下的纯逻辑 helper，便于单测
- 掉落随机与 AI 选择中的随机决策都要提供可注入入口，避免测试只能依赖全局随机
- `WaitingForInput` 的语义收敛为“等待玩家输入”；敌方回合在 `NextTurn` 或等价刷新点直接排入 `ExecutingAction`
- Milestone B 不引入新的“大而全战斗流程状态机”，尽量保持当前 `FlowState` 结构稳定
- Stage 3 与 Stage 4 在索引上拆开便于讨论边界，但细化时若总工作量不大，建议合并实现

## 阶段索引

### Stage 1: 战斗单位来源信息与金币真相

目标：

- 让 AI 和奖励系统都能从战斗运行时稳定反查“这个单位来自哪个 actor / enemy”

本阶段聚焦：

- 为 `BattleUnit` 补齐最小来源字段，例如 `std::optional<std::string> source_actor_id / source_enemy_id`
- `buildBattleUnitsFromCatalog()` 在构建玩家/敌人单位时写入来源字段
- 锁定“金币运行时真相”的持有位置：在 player entity 上新增最小 `PlayerWalletComponent`
- 明确 `SaveService::apply()` 从 `SaveData::player.gold` 初始化 `PlayerWalletComponent`
- 明确 `InventoryMenuScene` 与 `SaveService::capture()` 后续都从同一个 player entity 读取金币
- 明确预构建 battle units 的兼容策略：来源字段为空 `optional` 就是显式“无来源”，AI/奖励走降级分支而不是猜测显示名或空字符串

推荐最小方案：

- `BattleUnit` 保持领域主类型不拆分，但补充可空来源字段，不使用空字符串作为 sentinel
- 玩家单位写 `source_actor_id`，敌方单位写 `source_enemy_id`
- 新增 `PlayerWalletComponent`，只承担金币真相，不扩成大而全 progress 容器
- `ActorComponent` 继续只承担角色动作/持物语义，不混入经济字段

原因：

- 当前 `BattleUnit` 丢失了目录来源，`EnemyActionData::rating_` 和掉落奖励都无法可靠复原
- 当前项目里没有真正生效的运行时金币字段，且 `SaveData::player.gold` 还没有 runtime owner
- 先锁定来源字段，可以避免后面 AI 和奖励分别做两套补丁

阶段交付物：

- 稳定的 battle unit 来源元数据
- 稳定的金币运行时真相位置
- AI 与奖励后续阶段可复用的基础边界

建议后续细化文档：

- `plans/jrpg-milestone-b-stage1-battle-provenance.md`

### Stage 2: 敌方 AI 最小行动规划

目标：

- 让敌方回合自动生成可执行的 `BattleAction`，不再进入玩家输入菜单

本阶段聚焦：

- `BattleScene` 在 `FlowState::NextTurn` 或等价刷新点区分当前行动者阵营
- 玩家回合保留 `enterInputMenu() -> WaitingForInput`
- 敌方回合跳过 `WaitingForInput`，直接请求 AI planner 生成动作并进入提交流程
- 新增最小 `BattleAiPlanner`
- 调用侧先从 `source_enemy_id` 解析 `const EnemyData*`
- planner 不负责 `enemy id -> catalog` 查表；若需要技能 scope/MP 元数据，改为通过显式参数传入 `RpgCatalog` 或已解析的 skill descriptors
- 明确 AI 的 fallback：无可用技能时退回 `Attack`，若攻击也不可用则 `EndTurn`

推荐最小方案：

- `BattleAiPlanner` 只做“当前敌方回合选一个动作”
- `BattleScene` 或一个薄 adapter 先拿到 `BattleUnit + EnemyData`
- 优先选择当前 MP 足够且 scope 可执行的最高 rating 技能
- `Scope::OneEnemy` 先选存活玩家目标，可采用稳定规则如“最低 HP 比例优先”
- `Scope::OneAlly` 先选最需要收益的敌方单位；若当前还没有足够的收益判定信息，可先用“最低 HP 比例的存活友军”
- `Self / AllEnemies / AllAllies` 直接返回无显式目标的动作

原因：

- 当前代码已经有 `BattleSession::submitAction()` 的稳定执行闭环，缺的是动作生成
- AI planner 抽成独立纯逻辑比把规则散落在 `BattleScene` 更好测，也更方便后续增强
- 不让 planner 自己持有/隐藏 catalog 依赖，可以减少耦合并提升测试可控性
- 先做 deterministic 的最小策略，比一开始引入复杂随机和行为树更稳

阶段交付物：

- 敌方自动回合闭环
- 与现有 `BattleSession` 稳定接线的 AI planner
- 场景层不再让玩家替敌人手动选指令

建议后续细化文档：

- `plans/jrpg-milestone-b-stage2-enemy-ai.md`

### Stage 3: 战斗奖励汇总与掉落解析

目标：

- 让战斗胜利后能根据被击败敌人稳定生成奖励汇总

本阶段聚焦：

- 新增独立的 `BattleRewardResolver` 或等价 helper
- 基于 `final_units + source_enemy_id + RpgCatalog` 汇总 defeated enemies
- 聚合 `gold_reward_`
- 解析并投掷 `drops_`
- 可选聚合 `exp_reward_`，先只保留在 `BattleRewardSummary` 中，为后续成长系统预留
- 明确只在 `BattleOutcome::Victory` 时产出标准奖励

推荐最小方案：

- 奖励计算放在 battle/application helper 中，不塞进 `GameScene`
- 输出聚合后的 `gold_total / exp_total / item_drops`
- 掉落随机通过可注入 roll 回调驱动，测试中固定结果
- `exp_total` 暂不持久化，也不要求扩进事件，除非出现明确消费者
- 细化时若确认 Stage 3 与 Stage 4 工作量较小，建议合并成一个端到端奖励闭环阶段

原因：

- 当前 `EnemyData` 的奖励数据已就位，真正缺的是运行时聚合器
- 如果把奖励计算直接塞进 `GameScene::onBattleEnded()`，后续任务、结算 UI、日志复用都会变差
- `exp_total` 虽然本阶段不一定落地到成长系统，但一并汇总能减少未来重复遍历 defeated enemies

阶段交付物：

- 独立可测试的奖励汇总器
- 胜利后的稳定金币/掉落结果
- 后续成长、任务推进可复用的奖励摘要

建议后续细化文档：

- `plans/jrpg-milestone-b-stage3-battle-reward-summary.md`

### Stage 4: GameScene 写回与最小奖励反馈

目标：

- 让奖励真正进入探索态，并对玩家可见

本阶段聚焦：

- `GameScene::onBattleEnded()` 在处理完 item stock delta 后接入奖励写回
- 金币写入运行时真相
- 掉落通过 `InventoryDomainService::addItem()` 入包
- 处理背包容量不足时的最小策略与日志反馈
- 给玩家一个最小可见的奖励反馈，不要求独立结算 Scene
- 明确 `Victory / Escape / Defeat` 三种 outcome 下的写回规则

推荐最小方案：

- 仍由 `GameScene` 执行最终写回
- `GameScene` 在 `Victory` 分支调用 `BattleRewardResolver`，基于 `evt.final_units` 计算奖励摘要
- 金币直接累加到 player entity 上的 `PlayerWalletComponent`
- 物品掉落统一走 `InventoryDomainService`
- 奖励反馈先用现有通知/提示渠道做最小汇总文本，不新增复杂结算界面

本阶段必须同时覆盖：

- `applyBattleItemStockDelta()` 继续对所有 outcome 生效，保留战斗内已发生的物品消耗
- 奖励只在 Victory 时写回
- Escape 不发金币/掉落/经验，但保留本场战斗中的物品消耗
- Defeat 不发金币/掉落/经验，但同样保留本场战斗中的物品消耗；若未来要做 game over/continue 回滚，应单独立项
- 掉落入包失败时至少有 warn 或提示，不允许静默丢失
- 金币写回后 `InventoryMenuScene` 之类的 UI 能读到真实值，而不是继续显示 `Gold: --`

原因：

- `GameScene` 当前已经是战斗结束后真实状态写回的所有者，继续沿用这条边界最自然
- 若奖励只算出来不写回，Milestone B 仍然不构成玩法闭环
- 金币 UI 不接线的话，后续商店与菜单层仍然会卡在占位状态

阶段交付物：

- 金币写回闭环
- 掉落入包闭环
- 最小奖励反馈可见

建议后续细化文档：

- `plans/jrpg-milestone-b-stage4-reward-writeback-and-feedback.md`

### Stage 5: UI/存档与测试补强

目标：

- 让 Milestone B 的运行时结果可见、可存、可回归

本阶段聚焦：

- `InventoryMenuScene` 用 `PlayerWalletComponent` 的真实金币值替换 `Gold: --`
- `SaveService::capture/apply()` 接入 `PlayerWalletComponent`
- 补充 `SaveData` roundtrip 测试，确保金币不再只是 schema 占位
- 补充 AI planner / reward resolver / GameScene 写回 / battle unit provenance 测试
- 视需要更新 `docs/gameplay/turn-based-battle.md`

阶段交付物：

- 金币显示闭环
- 金币存档闭环
- Milestone B 关键路径回归测试

建议后续细化文档：

- `plans/jrpg-milestone-b-stage5-ui-save-and-tests.md`

## 需要新增的文件

以下为推荐新增文件，是否最终拆分为独立文件，可在各阶段细化时再确认：

- `plans/jrpg-milestone-b-stage1-battle-provenance.md`
- `plans/jrpg-milestone-b-stage2-enemy-ai.md`
- `plans/jrpg-milestone-b-stage3-battle-reward-summary.md`
- `plans/jrpg-milestone-b-stage4-reward-writeback-and-feedback.md`
- `plans/jrpg-milestone-b-stage5-ui-save-and-tests.md`

若按推荐方案实施，代码层后续大概率会新增：

- `src/game/battle/battle_ai_planner.h`
- `src/game/battle/battle_ai_planner.cpp`
- `src/game/battle/battle_reward_resolver.h`
- `src/game/battle/battle_reward_resolver.cpp`

若金币运行时真相采用独立组件，推荐新增：

- `src/game/component/player_wallet_component.h`

但这不是当前索引文档必须立即锁定的唯一命名。

## 实现步骤

### Step 1

完成 Stage 1 细化计划，先锁定 `BattleUnit` 来源字段与金币真相边界。

### Step 2

完成 Stage 2 细化计划，确定敌方 AI 的动作选择与目标选择规则。

### Step 3

完成 Stage 3 细化计划，确定奖励汇总结果、掉落随机和 Victory gating。

### Step 4

完成 Stage 4 细化计划，锁定 `GameScene` 的金币/掉落写回与最小反馈方案。

### Step 5

完成 Stage 5 细化计划，统一金币 UI、存档接线与测试策略。

## ToDo

- [x] Stage 1: 细化 battle unit 来源信息与金币真相方案 → `plans/jrpg-milestone-b-stage1-battle-provenance.md`
- [ ] Stage 2: 细化敌方 AI 动作规划与目标选择规则 → `plans/jrpg-milestone-b-stage2-enemy-ai.md`
- [ ] Stage 3: 细化奖励汇总、掉落随机与 Victory 结算边界 → `plans/jrpg-milestone-b-stage3-battle-reward-summary.md`
- [ ] Stage 4: 细化 `GameScene` 奖励写回与最小反馈方案 → `plans/jrpg-milestone-b-stage4-reward-writeback-and-feedback.md`
- [ ] Stage 5: 细化金币 UI、存档与测试补强方案 → `plans/jrpg-milestone-b-stage5-ui-save-and-tests.md`

## 备注

本索引计划采用的推荐范围是：

- 先把敌方 AI 与奖励闭环做出来
- 经验值只先汇总，不在 Milestone B 强行落地完整成长系统
- 金币必须进入运行时真相，并同步到 UI 与存档
- 不把任务、商店、完整升级系统混入本阶段

当前额外设计结论：

- 共享依赖优先级高于功能顺序，`BattleUnit` 来源信息必须先补
- 不推荐从显示名、数组顺序或 troop 位置反推奖励来源，这种做法后续很容易返工
- 不推荐让 `BattleScene` 直接持有 Inventory/Gold 写回职责，否则 battle 与 exploration 会再次耦合
- 敌方回合不应先进入 `WaitingForInput` 再绕过菜单；推荐在 `NextTurn` 切分玩家/敌方分支
- 默认保留当前“战斗消耗即时成立”的物品语义：Escape / Defeat 不回滚 battle item delta
- `exp_total` 默认只保留在奖励摘要里，不主动扩进事件或存档，直到出现明确消费者
- 若 Stage 1 没有先锁定金币真相，Milestone B 做完后 `Gold: --` 仍然会阻塞商店阶段

这样可以保证 Milestone B 形成一个真正可持续扩展的“敌方自动行动 + 战斗奖励闭环”。
