# Milestone C / Stage 2: Quest Giver 交互入口细化计划

## 实现思路

- 新增实例级 `QuestGiverComponent`，只绑定一个 `quest_id`；数据来源固定为 Tiled actor object 属性 `quest_offer_id`。
- `buildActor()` 这是首次为 actor object 引入自定义 property 读取；读取逻辑只用于地图实例，不回写 actor blueprint。
- `QuestGiverComponent` 只挂在地图实例实体上，不写进 actor blueprint；同一个 actor blueprint 可以在不同地图实例上绑定不同 quest。
- `EntityBuilder::buildActor()` 只负责解析 `quest_offer_id` 并挂载 `QuestGiverComponent`，不在 build 时查 `QuestCatalog`；无效 quest id 的处理下放给 `QuestInteractionSystem`，避免为 loader 新增 catalog 注入链路。
- `reuse_player_if_exists_` 的 player 复用分支保持 early return；这是有意行为，玩家实例不参与 quest giver 属性解析。
- 锁定单一交互 owner：带 `QuestGiverComponent` 的实体由 `QuestInteractionSystem` 独占处理交互语义，`DialogueSystem` 不再并行响应同一次 `InteractCommand`。
- 本阶段不修改 `InteractCommand` 契约，也不引入 consumed flag / dispatcher priority；冲突通过组件过滤和目标优先级解决。
- `InteractionSystem::chooseFacingTarget()` 显式加入 quest giver 分支，目标优先级固定为 `QuestGiver > Dialogue NPC > Chest > Rest`。
- `QuestInteractionSystem` 订阅 `InteractCommand`，直接消费：
  - `QuestCatalog`
  - 玩家 `QuestLogComponent`
  - giver 实例上的 `QuestGiverComponent`
- 任务状态判定先收敛为最小四态：
  - `Offerable`
  - `InProgress`
  - `ReadyToTurnIn`
  - `Completed`
- `QuestInteractionSystem` 只负责世界交互适配与反馈，不负责 JSON 加载、存档、战斗推进。
- quest 状态判断与接取写入先抽成小型 `quest_log_ops` helper，避免把 `active_quests / completed_quests / objective_progress` 读写细节散落在系统里。
- `quest_log_ops` 只聚焦 `QuestLogComponent` 的状态查询和原子写入；Stage 3/4 若需要战斗推进或 turn-in 聚合逻辑，再决定是否上提为 `QuestDomainService`。
- 接取任务时只做最小写入：
  - 把 `quest_id` 追加到 `active_quests`
  - 为该 quest 的每个 objective 初始化 progress key，默认值为 `0`
- 本阶段建立 turn-in 分支入口，但不完成最终 completed 迁移和奖励写回；`ReadyToTurnIn` 先只负责反馈，真正完成逻辑留到 Stage 4。
- quest giver 文本优先读取 `QuestData::giver_text`：
  - `offer`
  - `progress`
  - `ready_to_turn_in`
  - `completed`
- 若对应文本为空，回退到最小默认文案，避免交互后没有玩家可见反馈。
- 反馈链路复用现有 `DialogueShowEvent` 通知通道：
  - channel `0`：普通对话
  - channel `1`：通用世界通知（农场提示 / 开箱 / 战斗奖励 / quest giver）
  - channel `2`：物品使用提示
- Stage 2 的 quest giver 提示固定复用 channel `1`，与 `ChestSystem / FarmSystem / BattleSettlement` 共用同一条“通知槽位”，不新增独立 UI channel。
- `QuestInteractionSystem` 使用 `NotificationTimer` 维护 channel `1` 的 quest 提示，不复用 `DialogueComponent.active_` 状态机。

## 需要新增的文件

- `src/game/component/quest_giver_component.h`
- `src/game/domain/quest_log_ops.h`
- `src/game/domain/quest_log_ops.cpp`
- `src/game/system/quest_interaction_system.h`
- `src/game/system/quest_interaction_system.cpp`
- `tests/game/quest_interaction_system_test.cpp`

## 实现步骤

### Step 1. 定义 QuestGiverComponent 与地图属性约定

- 新增 `QuestGiverComponent`，最小字段保持为 `quest_id + quest_id_hash`。
- 在 `tiled_conventions.h` 中新增 `Actor instance properties` 分区，并定义 `ACTOR_PROP_QUEST_OFFER_ID`。
- `EntityBuilder::buildActor()` 在实例化 actor 后读取 object properties；这是 actor object 首次引入自定义 property 读取。
- `reuse_player_if_exists_` 的 player 复用路径保持提前返回，不进入 quest giver 属性解析。
- 若 `quest_offer_id` 缺失、为空、或类型不是 string，直接跳过挂载，不阻断地图加载。
- 本阶段不在 loader 做 `QuestCatalog` 查表校验，只把 `quest_id + hash` 写进实例组件。

### Step 2. 锁定 quest giver 的交互归属

- `InteractionSystem::chooseFacingTarget()` 增加 quest giver 候选，且优先于 `DialogueComponent`。
- `DialogueSystem::onInteractCommand()` 对带 `QuestGiverComponent` 的目标直接跳过，作为防御式兜底。
- 保持 `InteractionSystem` 继续只负责“选目标 + 发布 `InteractCommand`”，不把任务状态机塞回去。

### Step 3. 实现 quest log helper 与最小状态判定

- 在 `quest_log_ops` 中集中实现：
  - `isQuestActive(const QuestLogComponent&, entt::id_type quest_id_hash)`
  - `isQuestCompleted(const QuestLogComponent&, entt::id_type quest_id_hash)`
  - `isQuestReadyToTurnIn(const QuestLogComponent&, const QuestData&)`
  - `tryAcceptQuest(QuestLogComponent&, const QuestData&)`
- `isQuestReadyToTurnIn(...)` 通过 `QuestData.objectives_` 判断所有 objective 是否满足 required count，其余 helper 不依赖 `QuestCatalog`。
- `tryAcceptQuest(...)` 负责初始化 objective progress key，禁止 `QuestInteractionSystem` 手写 vector/map 读写细节。

### Step 4. 实现 QuestInteractionSystem

- `QuestInteractionSystem` 订阅 `InteractCommand`，只处理 `target` 带 `QuestGiverComponent` 的实体。
- `QuestInteractionSystem` 先用 `QuestGiverComponent.quest_id_hash` 查 `QuestCatalog`；查不到时记录 `warn` 并提前返回，不在交互层猜测默认 quest。
- 分支规则固定为：
  - `Offerable`：接取任务并显示 `offer`
  - `InProgress`：显示 `progress`
  - `ReadyToTurnIn`：显示 `ready_to_turn_in`
  - `Completed`：显示 `completed`
- 通知显示走 `NotificationTimer + DialogueShowEvent(channel=1)`，明确复用现有“通用世界通知”槽位。
- 系统提供 `update(float delta_time)`，只负责通知气泡位置刷新与超时隐藏。

### Step 5. 接入 Runtime 装配与调度

- `GameSystemBundle`、`system/fwd.h`、`game_runtime_assembler.cpp` 接入 `QuestInteractionSystem`。
- `QuestInteractionSystem` 构造依赖最小化，推荐只注入：
  - `entt::registry&`
  - `entt::dispatcher&`
  - `const QuestCatalog&`
- `SystemScheduler` 的接线点显式列为：
  - `SchedulerStage` 新增 `QuestInteraction`
  - `exploration_profile()` 在 `Dialogue` 后追加 `QuestInteraction`
  - `execute_stage_main_thread()` 新增 `QuestInteractionSystem::update()` case
  - `toString()` 新增 `QuestInteraction` case
- `QuestInteraction` 放在 `Dialogue` 之后，专门维护 quest 通知定时器；不改 battle / pause / cutscene profile。

### Step 6. 补齐测试

- `quest_offer_id` 能把 actor 实例挂成 `QuestGiverComponent`。
- quest giver 在 `chooseFacingTarget()` 中优先于普通 `DialogueComponent / Chest / Rest`。
- `DialogueSystem` 不再处理 quest giver 的 `InteractCommand`。
- 首次交互会接取 quest，并初始化 objective progress key。
- 已接未完成时显示 `progress` 文本。
- 已满足条件时进入 `ReadyToTurnIn` 分支，但不在 Stage 2 提前写 completed 状态。
- 已完成 quest 时显示 `completed` 文本。
- quest giver 提示固定走 `DialogueShowEvent(channel=1)`。

## ToDo

- [ ] 新增 `QuestGiverComponent`，锁定实例级 `quest_offer_id` 配置入口
- [ ] 在 `EntityBuilder::buildActor()` 挂载 quest giver 实例组件
- [ ] 在 `tiled_conventions.h` 新增 `Actor instance properties` 分区与 `ACTOR_PROP_QUEST_OFFER_ID`
- [ ] 把交互优先级升级为 `QuestGiver > Dialogue NPC > Chest > Rest`
- [ ] 让 `DialogueSystem` 显式跳过 quest giver 目标
- [ ] 新增 `quest_log_ops`，统一接取与状态判定 helper
- [ ] 新增 `QuestInteractionSystem` 并接入 `InteractCommand`
- [ ] 复用 `DialogueShowEvent(channel=1)` + `NotificationTimer` 做任务提示
- [ ] 在 runtime assembler / scheduler 中接入 `QuestInteractionSystem` 与 `SchedulerStage::QuestInteraction`
- [ ] 补齐 quest giver 交互与优先级测试
