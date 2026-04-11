# Milestone C / Stage 4: 任务交付完成与最小奖励写回细化计划

## 实现思路

- Stage 4 把 Stage 2 的 `ReadyToTurnIn` 分支升级为真正的 turn-in；世界交互仍由 `QuestInteractionSystem` 触发，但最终状态迁移与奖励写回统一下沉到新的 `QuestTurnInService`。
- `QuestTurnInService` 只处理一次完整的任务交付：
  - quest 必须处于 `active`
  - `isQuestReadyToTurnIn(...)` 必须为 `true`
  - reward preflight 必须先于任何写回
  - 成功时才允许 `active -> completed` 迁移
- turn-in 规则固定为事务化：
  - 若 quest 没有 reward，也允许正常完成
  - 若任一 item reward 无法完整入包，则整个 turn-in 失败
  - 失败时 quest 保持 `ReadyToTurnIn`，不允许“任务完成但奖励丢失”或“部分奖励到账”
- 任务奖励继续复用 `PlayerWalletComponent + InventoryDomainService`，不创建第二套经济路径。
- `InventoryDomainService::addItem()` 的 `accepted/rejected` 语义不适合作 turn-in 主流程；Stage 4 应先做容量模拟，再执行写回。`simulateAdd(...)` 明确上提到 `inventory_helpers.h` 的 `game::system::detail`，由 `ItemUseSystem` 和 `QuestTurnInService` 共用，避免库存容量规则分叉。
- item reward 的 preflight 必须在同一份 `InventoryComponent::slots_` 副本上累积模拟；不能为每种 reward item 各自拷一份干净背包，否则会高估可用空间。
- `quest_log_ops` 继续只负责 `QuestLogComponent` 的低层读写；Stage 4 可补 `completeQuest(...)` / `eraseQuestProgress(...)` 这类 helper，但不把钱包、背包写回塞回 helper。
- turn-in 成功时按固定顺序处理：
  1. 校验 ready-to-turn-in
  2. 预检全部 item reward 是否可完整放入背包
  3. 写入 gold / items
  4. 把 quest 从 `active_quests` 移到 `completed_quests`
  5. 清理该 quest 的 progress keys
- `QuestInteractionSystem` 的 `ReadyToTurnIn` 分支改为调用 `QuestTurnInService`；成功时显示完成反馈，失败时显示明确原因。
- 本阶段不新增 quest JSON 字段；turn-in 成功后的基础文本优先复用 `QuestData::giver_text.completed_`，为空时回退到 `任务完成：<title>`。
- 任务完成反馈继续固定走 `DialogueShowEvent(channel=1)`，同一次 turn-in 只发一条通知；奖励摘要直接并入同一条文本，不额外新开 channel。
- Runtime 装配上，`QuestTurnInService` 作为 domain service 接入 `GameRuntimeServices`，由 `QuestInteractionSystem` 依赖注入；不在 system 内临时拼装 service。

## 需要新增的文件

- `src/game/domain/quest_turn_in_service.h`
- `src/game/domain/quest_turn_in_service.cpp`
- `tests/game/quest_turn_in_service_test.cpp`
- `tests/data/quest_turn_in_quests.json`

## 实现步骤

### Step 1. 定义 turn-in service 与结果类型

- 新增 `QuestTurnInService`，只负责 quest completion 与 reward writeback。
- 定义最小 `QuestTurnInResult`，推荐至少保留：
  - `status`
  - `gold_reward`
  - `item_rewards`
  - `failure_message`
- `status` 建议覆盖：
  - `Completed`
  - `NotReady`
  - `InventoryFull`
  - `MissingWallet`
  - `MissingInventory`
- `item_rewards` 推荐保留 `item_id / item_name / count`，其中 `item_name` 在 turn-in 过程中通过 `ItemCatalog` 查询并写入 result，方便 `QuestInteractionSystem` 直接拼最小反馈文本。

### Step 2. 实现事务化 turn-in 写回

- `QuestTurnInService` 输入保持最小：
  - `entt::registry&`
  - `game::data::ItemCatalog&`
  - `game::domain::InventoryDomainService&`
- service 对外接口明确为 `turnIn(entt::entity player, const QuestData& quest, QuestLogComponent& quest_log)`；钱包、背包和其余运行时依赖统一在方法内部通过 `player + registry` 查询。
- turn-in 前先校验 quest 当前确实处于 active 且 ready-to-turn-in。
- `simulateAdd(...)` 明确抽到 `inventory_helpers.h`，继续使用 `preferred_slot_index` 形态；turn-in 场景固定传 `-1`，不单独做新重载。
- item rewards 先做完整容量预检；做法固定为：
  1. 拷贝一份玩家当前 `slots_`
  2. 在同一份副本上按 reward 顺序连续执行 `simulateAdd(...)`
  3. 任一步失败即整体失败
- turn-in 不再接受 `addItem()` 的部分成功结果。
- 预检失败时直接返回 `InventoryFull`，不写 gold、不写 items、不迁移 quest 状态。
- 预检成功后再执行真实写回：
  - gold 写入 `PlayerWalletComponent::gold_`
  - items 统一走 `InventoryDomainService`
  - quest 状态迁移由 `quest_log_ops::completeQuest(...)` 统一完成，内部同时负责“从 `active_quests` 移除 + 加入 `completed_quests`”
  - progress key 清理由 `quest_log_ops::eraseQuestProgress(quest_log, quest.id_)` 统一完成，按 `quest_id::` 前缀删除，不要求额外传 `QuestData.objectives_`
- 缺少 `PlayerWalletComponent` 或背包运行时依赖时至少 `warn`，并返回失败结果；不能静默完成 quest。

### Step 3. 接入 QuestInteractionSystem 的 ReadyToTurnIn 分支

- `QuestInteractionSystem` 构造依赖补上 `QuestTurnInService`。
- `ReadyToTurnIn` 分支不再只显示 `ready_to_turn_in` 文本，而是直接执行 turn-in。
- turn-in 成功时：
  - quest 进入 `Completed`
  - `ReadyToTurnIn` 分支内直接拼接完成反馈文本
  - 基础文本仍优先取 `completed` 文本或默认完成文案
  - 若有 reward，再追加最小奖励摘要
- turn-in 失败时：
  - quest 继续保持 `ReadyToTurnIn`
  - 使用 `failure_message` 给出玩家可见提示，例如“背包空间不足”
- `showQuestText(...)` 保持 Stage 2 的通用逻辑，不为了 turn-in success 分支改成大而全 formatter。
- `Offerable / InProgress / Completed` 三个分支保持 Stage 2 现有语义，不顺手扩成更复杂状态机。

### Step 4. 接入 runtime 与补齐测试

- `GameRuntimeServices` 新增 `QuestTurnInService`，并在 `game_runtime_assembler.cpp` 中完成装配。
- `QuestInteractionSystem` 的 assembler 接线改为同时接收 `QuestCatalog + QuestTurnInService`。
- 新增 `quest_turn_in_service_test.cpp`，至少覆盖：
  - 无 reward 的 quest 也能完成
  - gold / item rewards 能正确写回
  - 背包空间不足时 turn-in 失败且 quest 保持 ready
  - 完成后 quest 从 `active_quests` 迁到 `completed_quests`
  - 完成后该 quest 的 progress keys 会被清理
- 更新 `quest_interaction_system_test.cpp`：
  - 现有 “ReadyToTurnInShowsReadyTextWithoutCompletingQuest” 需要改成成功完成分支
  - 新增 turn-in 失败仍停留在 ready 状态的交互测试
- save roundtrip 现有接线无需改路径；Stage 4 的重点是确认 quest 完成后 `QuestLogComponent` 的新状态仍能被 `SaveService` 正常捕获。

## ToDo

- [ ] 新增 `QuestTurnInService` 与 `QuestTurnInResult`
- [ ] 固定 turn-in 的事务化规则，禁止部分奖励到账
- [ ] 复用 `PlayerWalletComponent + InventoryDomainService` 完成最小奖励写回
- [ ] 在 turn-in 成功后把 quest 从 `active_quests` 迁到 `completed_quests`
- [ ] 在 turn-in 成功后清理该 quest 的 objective progress keys
- [ ] 让 `QuestInteractionSystem` 的 `ReadyToTurnIn` 分支真正执行 turn-in
- [ ] 继续复用 `DialogueShowEvent(channel=1)` 输出单条完成反馈
- [ ] 在 runtime assembler 中接入 `QuestTurnInService`
- [ ] 补齐 turn-in success / failure / reward writeback / progress cleanup 测试
