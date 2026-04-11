# Milestone C: 最小任务系统索引计划

## Context

Milestone B 已完成“敌方自动行动 + Victory 奖励闭环”，这给 Milestone C 留下了几条很重要的可复用边界：

- `BattleEndedEvent` 现在会稳定带出 `outcome / final_units / remaining_item_stocks`
- `BattleUnit` 已保留 `source_enemy_id`，可以从战斗结果稳定反查“这次击败了哪些 enemy”
- `GameScene` 已经是战斗结束后探索态真实写回的所有者，负责物品库存与金币写回
- `SaveData` 的 `quest_state.active_quests / completed_quests / objective_progress` 已有 schema 预留
- `InteractionSystem` 已经收敛为“选目标 + 发布 `InteractCommand`”，非常适合作为任务领取/交付入口
- `InventoryMenuScene` 已有 `Quests` 标签页占位，但仍是纯 placeholder

当前 Milestone C 真正缺失的不是“再补一个存档字段”，而是完整的最小任务玩法闭环：

- 世界里还没有任务静态数据目录，无法定义 quest / objective / reward / giver
- 运行时也没有 quest 真相持有者，`quest_state` 仍只是 save schema，而不是可操作状态
- 交互系统虽然已经支持多订阅者扩展，但还没有 `QuestGiver` 这类任务入口
- 战斗胜利后虽然能写回奖励，但还没有把“击败了哪些 enemy”转成任务计数推进
- 菜单虽然已有 `Quests` 标签页，但没有 active/completed/progress 的可见 UI

因此，Milestone C 建议只解决这几件事：

- 让玩家可以从 NPC 接到一个最小任务
- 让 Victory 战斗结果可以推进“击败某类敌人 N 次”目标
- 让玩家回到任务 NPC 处交付并完成任务
- 让任务状态能显示、能存档、能恢复

## 范围

### 本阶段包含

- Quest 静态目录：quest 定义、objective 定义、可选 reward 定义
- Quest 运行时真相：active/completed/progress
- 基于 `InteractCommand` 的任务领取/交付入口
- 基于 `BattleEndedEvent.final_units` 的击败敌人计数推进
- 最小任务反馈：接受、推进、可交付、完成
- `InventoryMenuScene` 中的最小任务页显示
- Quest save/load 与对应测试

### 本阶段不包含

- 分支对话树、对话选项、复杂剧情脚本编排
- 采集/交物/护送/限时/探索触发等多类型 objective
- 地图标记、追踪箭头、任务面板动画与独立 Quest Scene
- 完整任务链依赖、条件分支、失败状态、可重复任务
- 大而全的剧情 flag 系统
- 为任务系统引入新的全局流程状态机

## 实现思路

Milestone C 不适合把任务逻辑分别塞进 `DialogueSystem`、`BattleScene` 和 `SaveService`。

原因：

- `DialogueSystem` 当前职责很纯，只负责普通对话文本推进；若把领取/交付/奖励/状态迁移混进去，会迅速变成杂糅系统
- `BattleScene` 继续应只负责战斗表现与会话编排，不应直接拥有 quest 进度写回职责
- `SaveData` 已有 quest schema，但 schema 不等于 runtime owner；若没有运行时真相，后续 UI 和玩法仍会反复绕回 save 结构
- 任务推进需要跨“交互入口”和“战斗结果”两条路径，因此必须有统一的 domain 边界，而不是两个地方各自 patch

因此更稳的推进方式是：

1. 先补齐 quest 静态目录与运行时真相
2. 再把 NPC 领取/交付做成独立交互订阅者，而不是挤进 `DialogueSystem`
3. 再把击败敌人计数推进做成独立 helper / domain 逻辑，由 `GameScene` 在战斗结束时调用
4. 最后接任务 UI、存档与测试补强

同时建议锁定这些边界：

- Quest 数据目录独立于 `RpgCatalog`，不要把 quest 定义塞进 `actors/enemies/dialogue_script.json`
- Quest 运行时真相放在玩家实体上，例如 `QuestLogComponent`，不直接把 `SaveData::quest_state` 当运行时对象使用
- `InteractionSystem` 继续只发布 `InteractCommand`，任务逻辑通过新增 `QuestInteractionSystem` 或等价订阅者接入
- Milestone C 必须锁定“一个实体只能有一个交互 owner”的规则：带 `QuestGiverComponent` 的 NPC 由 `QuestInteractionSystem` 负责玩家交互语义，`DialogueSystem` 不再同时处理同一实体的普通对话
- `BattleEndedEvent` 契约本阶段不必再扩；击败计数直接复用 `final_units + source_enemy_id`
- 击败计数默认只在 `BattleOutcome::Victory` 下推进；`Escape / Defeat` 不计任务进度
- 任务提示优先走现有通知/气泡渠道，不在 Milestone C 强行引入复杂对话分支系统
- `objective_progress` 的 key 必须是由 `(quest_id, objective_id)` 生成的稳定复合 key，不使用裸 `objective_id`、显示文案或数组下标作为隐式 key
- `InventoryMenuScene` 的任务页继续遵循当前 `RmlDocumentController + TabContent` 模式，不回退到旧 UI 路径
- 若任务 giver 是场景实例化 NPC，优先从地图对象属性附加 `QuestGiverComponent`，而不是把 quest id 写死到全局 actor blueprint
- `InteractionSystem::chooseFacingTarget()` 在本阶段要显式加入 quest giver 分支，推荐优先级为 `QuestGiver > Dialogue NPC > Chest > Rest`
- quest battle progress 的写回应复用 `game_scene_battle_settlement.cpp` 的现有抽取模式，作为独立步骤串入 `processBattleEndedForGameScene()`，不在 `GameScene` 再开平行结算路径
- 若同一场 Victory 既有战斗奖励反馈又有 quest progress 反馈，必须合并到同一条 settlement 反馈路径，避免同帧重复占用同一个通知 channel 造成覆盖

## 阶段索引

### Stage 1: Quest 目录与运行时真相

目标：

- 让 quest 从“save schema 预留字段”升级为“有静态定义、有运行时 owner 的真实系统”

本阶段聚焦：

- 新增独立 `QuestCatalog` 或等价目录类，加载例如 `assets/data/quests.json`
- 定义最小 quest 数据模型：`quest_id / title / description / objectives / optional rewards / optional giver text`
- objective 数据采用带 kind 的最小 tagged schema，本阶段只落地 `DefeatEnemyCount`
- objective 必须带稳定 `objective_id`
- `objective_progress` 的存档 key 规则在本阶段锁定为复合 key，由 helper 基于 `(quest_id, objective_id)` 生成；不要求 `objective_id` 全局唯一
- 在玩家实体上新增 `QuestLogComponent`
- `QuestLogComponent` 保持与 save schema 接近：`active_quests / completed_quests / objective_progress`
- `SaveService::capture/apply()` 与 `QuestLogComponent` 接线
- `EntityFactory` 或等价玩家创建路径保证玩家始终拥有 `QuestLogComponent`

推荐最小方案：

- quest 目录做成独立 `game::data` 模块，不混入 `RpgCatalog`
- `QuestLogComponent` 只负责任务真相，不扩成大而全剧情 flag 容器
- `objective_progress` 运行时与存档都使用“复合 progress key -> int”的稳定 map，并通过统一 helper 生成 key，禁止业务代码手写字符串拼接
- `QuestCatalog` 只存静态配置，不在目录对象中缓存运行时完成状态

原因：

- 当前 save schema 虽有 `quest_state`，但没有 runtime owner，导致 UI、交互、战斗推进都无从接入
- 若 objective progress key 不稳定，或不同 quest/objective 之间复用同名 key，后续改排序、改文案或新增多目标任务时会直接破坏进度恢复
- 任务定义独立于 RPG 战斗目录，更符合后续“对话 / 探索 / 事件触发”扩展方向

阶段交付物：

- 可加载的 quest 静态目录
- 可持有 active/completed/progress 的 quest runtime owner
- quest save/load 闭环基础

建议后续细化文档：

- `plans/jrpg-milestone-c-stage1-quest-catalog-and-runtime.md`

### Stage 2: 任务领取与交付入口

目标：

- 让玩家能通过现有世界交互链路接任务，并在条件满足后回 NPC 交付

本阶段聚焦：

- 新增 `QuestGiverComponent` 或等价实例组件
- Quest giver 优先从地图 actor object 的属性附加，例如 `quest_offer_id`
- 新增 `QuestInteractionSystem`，订阅 `InteractCommand`
- 锁定 quest NPC 的交互归属：Milestone C 中带 `QuestGiverComponent` 的实体由 `QuestInteractionSystem` 独占处理任务相关交互，不与 `DialogueSystem` 并行响应同一次 `InteractCommand`
- `InteractionSystem::chooseFacingTarget()` 显式加入 quest giver 分支，推荐优先级为 `QuestGiver > Dialogue NPC > Chest > Rest`
- 规则最小化：
  - 未接取：领取任务
  - 已接取但未完成：显示进度/提示
  - 已满足目标但未交付：执行 turn-in
  - 已完成：显示完成后文本
- 任务状态变更由 domain/helper 统一处理，`QuestInteractionSystem` 只做交互适配
- 接受/交付反馈先用现有通知渠道，不要求复杂分支对话

推荐最小方案：

- Milestone C 先限制为“一个 quest giver 绑定一个主 quest id”
- `QuestInteractionSystem` 作为 quest giver 的唯一交互 owner，负责接任务/交付/进度提示文本
- `DialogueSystem` 对带 `QuestGiverComponent` 的实体显式跳过，作为防御式兜底，避免 quest giver 同时触发普通对话和任务逻辑
- quest giver 的任务反馈优先走现有通知/气泡事件，由 `QuestInteractionSystem` 直接发出，不复用 `DialogueComponent.active_` 状态机
- `QuestGiverComponent` 用实例级配置，不写进全局 actor blueprint
- turn-in 默认要求返回同一个 giver
- Milestone C 不支持“同一 NPC 同时拥有独立普通闲聊和 quest 交互状态机”；若确有文本需要，由 quest 配置直接提供对应状态文案

原因：

- `InteractCommand` 本来就是当前项目的玩法扩展点，任务最适合沿用这条边界
- 当前 `dispatcher.trigger()` 是同步 fan-out，多订阅者若同时处理 quest NPC 会直接产生文本/提示冲突，必须先锁定交互 owner 才能保证行为确定
- 若强行把任务状态机揉进 `DialogueSystem`，后续商店、剧情、任务都会互相缠绕
- 使用地图实例级 giver 配置，后续更容易支持同 blueprint 多个 NPC、不同地图不同任务

阶段交付物：

- 任务 NPC 入口闭环
- 接任务 / 回报任务的最小交互状态机
- 不污染 `InteractionSystem` 与 `DialogueSystem` 的任务接线

建议后续细化文档：

- `plans/jrpg-milestone-c-stage2-quest-giver-interaction.md`

### Stage 3: 战斗击败计数推进

目标：

- 让 Victory 战斗结果可以稳定推进“击败某 enemy N 次”类 objective

本阶段聚焦：

- 新增 `QuestProgressResolver` / `QuestDomainService` 中的 battle progress 逻辑
- 复用 `BattleEndedEvent.outcome + final_units`
- 从 `final_units` 中统计 `side == Enemy && !isAlive() && source_enemy_id.has_value()` 的 defeated enemy counts
- 只推进 active quest，completed quest 不再重复计数
- objective 满足后返回可见摘要，例如“Quest Updated / Ready To Turn In”
- quest battle progress 写回固定接入 `game_scene_battle_settlement.cpp`，推荐新增 `applyQuestBattleProgress()` 并由 `processBattleEndedForGameScene()` 串联调用

推荐最小方案：

- 不再修改 `BattleEndedEvent` 契约
- 击败计数逻辑优先做成纯逻辑 helper，输出 progress delta summary
- 只在 `BattleOutcome::Victory` 下推进 quest progress
- `source_enemy_id` 缺失时显式跳过，不从名字或 troop 位置反推
- battle quest progress 不从 `GameScene::onBattleEnded()` 额外分叉调用，而是跟随现有 settlement helper 链路统一编排
- 若本场战斗同时产出金币/掉落反馈与 quest 进度反馈，优先合并成同一条 battle settlement 摘要文本，避免 channel 覆盖

原因：

- Milestone B 已经提供了足够的 defeated enemy 来源信息，再扩 battle event 契约收益不大
- 击败计数是 quest 和 battle 的接缝点，最适合抽成独立 helper 方便单测
- `game_scene_battle_settlement.cpp` 已经是战斗写回编排边界；沿用现有 `applyBattleItemStockDelta() / applyVictoryRewards()` 模式，后续更容易维护
- 若 `Escape / Defeat` 也推进，会在规则上引入额外歧义，不利于 Milestone C 最小闭环

阶段交付物：

- 基于战斗结果的 kill objective 推进闭环
- 可复用的 defeated enemy -> objective progress 解析逻辑
- Quest 与 Battle 的稳定接缝

建议后续细化文档：

- `plans/jrpg-milestone-c-stage3-battle-progress.md`

### Stage 4: 交付完成与最小奖励写回

目标：

- 让任务在目标满足后可以真正“交付完成”，并可选发放最小奖励

本阶段聚焦：

- 定义“ready to turn in”的判定：所有 active objectives 达到 required count
- turn-in 时把 quest 从 `active_quests` 移到 `completed_quests`
- 清理该 quest 对应的 objective progress 临时项，避免 runtime/save 中残留无意义累积
- 若 quest 配置带最小 reward（gold / items），复用 `PlayerWalletComponent + InventoryDomainService` 写回
- 奖励写回失败时必须至少给出 warn/提示，不允许静默丢失
- 给予最小完成反馈：通知文本或任务状态更新提示

推荐最小方案：

- 任务奖励做成可选字段；无 reward 配置时也允许正常完成 quest
- 交付完成的最终状态迁移仍由 quest domain 逻辑统一处理
- 金币/物品奖励沿用 Milestone B 已有写回真相，不新建第二套经济路径
- “满足目标”与“已完成”明确区分，防止战斗一结束就自动 completed

原因：

- 里程碑描述明确要求“交付完成”，说明 turn-in 是状态机的一部分，而不是目标达成后自动结束
- 奖励若沿用已有钱包和背包真相，后续商店与任务经济来源会更统一
- 清理完成任务的进度 map，可以让 UI 和 save 数据都更干净

阶段交付物：

- quest turn-in 闭环
- completed quest 状态稳定落地
- 可选的最小 gold/item reward 写回

建议后续细化文档：

- `plans/jrpg-milestone-c-stage4-turn-in-and-rewards.md`

### Stage 5: Quest UI、存档与测试补强

目标：

- 让任务系统结果可见、可存、可回归

本阶段聚焦：

- 用真实 `QuestTabContent` 替换 `InventoryMenuScene` 的 `Quests` placeholder
- active quest 列表至少显示 `title + progress summary`
- completed quest 至少有一个最小列表或状态区
- `SaveService::capture/apply()` 覆盖 quest runtime truth
- 补充 `QuestCatalog`、quest domain、battle progress、giver interaction、save roundtrip、quest tab 的测试
- 视需要更新 `docs/game/save_and_flow.md` 与 `docs/game/interaction_and_dialogue.md`

推荐最小方案：

- Quest UI 继续嵌入 `InventoryMenuScene`
- Quest 页先做“信息可见”而不是“复杂筛选/分页/地图追踪”
- 测试优先覆盖：
  - 目录加载
  - 接任务/重复接任务保护
  - Victory kill count 推进
  - turn-in 完成与奖励写回
  - save/load roundtrip
  - RmlUi 任务页接线

阶段交付物：

- 任务显示闭环
- 任务存档闭环
- Milestone C 关键路径回归测试

建议后续细化文档：

- `plans/jrpg-milestone-c-stage5-ui-save-and-tests.md`

## 需要新增的文件

以下为推荐新增文件，是否最终拆分为独立文件，可在各阶段细化时再确认：

- `plans/jrpg-milestone-c-stage1-quest-catalog-and-runtime.md`
- `plans/jrpg-milestone-c-stage2-quest-giver-interaction.md`
- `plans/jrpg-milestone-c-stage3-battle-progress.md`
- `plans/jrpg-milestone-c-stage4-turn-in-and-rewards.md`
- `plans/jrpg-milestone-c-stage5-ui-save-and-tests.md`

若按推荐方案实施，代码层后续大概率会新增：

- `src/game/data/quest_catalog.h`
- `src/game/data/quest_catalog.cpp`
- `src/game/component/quest_log_component.h`
- `src/game/component/quest_giver_component.h`
- `src/game/domain/quest_domain_service.h`
- `src/game/domain/quest_domain_service.cpp`
- `src/game/system/quest_interaction_system.h`
- `src/game/system/quest_interaction_system.cpp`
- `src/game/ui/quest_tab_content.h`
- `src/game/ui/quest_tab_content.cpp`

但这不是当前索引文档必须立即锁定的唯一命名。

## 实现步骤

### Step 1

完成 Stage 1 细化计划，先锁定 quest 数据模型、objective key 规则与 runtime truth。

### Step 2

完成 Stage 2 细化计划，确定 quest giver 数据来源与 `InteractCommand` 交互状态机。

### Step 3

完成 Stage 3 细化计划，锁定 Victory kill count 的 battle -> quest 进度推进边界。

### Step 4

完成 Stage 4 细化计划，明确 turn-in、completed 迁移与可选 reward 写回规则。

### Step 5

完成 Stage 5 细化计划，统一 quest UI、存档接线与测试策略。

当前索引计划的推荐结论是：

- 先立 quest catalog 与 runtime truth，再做入口和推进
- 任务逻辑不直接塞进 `DialogueSystem` 或 `BattleScene`
- 交互入口复用 `InteractCommand`，战斗推进复用 `BattleEndedEvent.final_units`
- Milestone C 先只做 `DefeatEnemyCount` objective
- Quest UI 先落在 `InventoryMenuScene`，不单开大场景
- 在 Stage 1 完成 runtime truth 后，Stage 2（领取/交付入口）和 Stage 3（战斗击败计数推进）可以并行细化或并行实现；文档顺序仍按玩家体验流程排列

## ToDo

- [x] Stage 1: 细化 quest 目录、objective key 与 runtime truth 方案 → `plans/jrpg-milestone-c-stage1-quest-catalog-and-runtime.md`
- [ ] Stage 2: 细化 quest giver 入口与任务交互状态机
- [ ] Stage 3: 细化 Victory kill count 推进与 battle 接缝
- [x] Stage 4: 细化 turn-in、completed 状态迁移与可选 reward 写回
- [ ] Stage 5: 细化 quest UI、存档与测试补强方案

## 备注

本索引计划采用的推荐范围是：

- 先做“接任务 -> 杀怪计数 -> 回 NPC 交付”的最小任务闭环
- 任务推进只依赖现有 `InteractCommand` 与 `BattleEndedEvent`
- Quest 运行时真相必须显式存在，不能继续停留在 save schema 占位层
- UI 只求先看得见 active/completed/progress，不提前做复杂追踪系统
- 不把剧情脚本、分支对话、商店、复杂任务链混入 Milestone C

当前额外设计结论：

- 不推荐把 quest 数据塞进 `RpgCatalog`，否则任务会与战斗目录过度耦合
- 不推荐把 quest giver 写死在 actor blueprint；实例级地图属性更适合后续扩展
- 不推荐让 `DialogueSystem` 同时承担任务状态迁移；它现在的职责边界已经足够清晰
- 不推荐让 quest giver 继续和普通 `DialogueComponent` 并行消费同一次 `InteractCommand`；Milestone C 必须先锁定单一交互 owner
- 不推荐为了任务推进再扩 `BattleEndedEvent`；Milestone B 留下的 `final_units + source_enemy_id` 已经够用
- 不推荐让任务目标完成后自动 completed；Milestone C 应保留“回去交付”的 JRPG 手感
- 不推荐在 Milestone C 首批支持多种 objective 类型；先把 `DefeatEnemyCount` 的闭环和测试打稳更重要
- 若 Stage 1 没有先锁定复合 progress key 规则，后续 UI 排序、配置重构或多目标任务都会很容易破坏进度恢复

这样可以保证 Milestone C 形成一个真正可持续扩展的“接任务 + 杀怪计数 + 交付完成”闭环。
