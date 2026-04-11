# Quest Debug Panel 集成计划

## 实现思路

- 新增独立 `QuestDebugPanel`，放在 `src/game/debug/`，作为 `Game` 分类下的独立调试面板接入；不把 quest 调试入口塞进现有 `InventoryDebugPanel`、`PlayerDebugPanel` 或 `SaveLoadDebugPanel`。
- 面板继续沿用现有 `engine::debug::DebugPanel + ImGui` 架构，只在 `TF_ENABLE_DEBUG_UI` 下编译和注册，不改正式 UI，不新增 RmlUi 文档。
- Quest 调试面板窗口不使用 `ImGuiWindowFlags_AlwaysAutoResize`；推荐设置一个 `FirstUseEver` 初始尺寸并允许开发时手动缩放，避免 quest 列表、详情和 objective 列表导致窗口频繁跳动。
- Quest 调试面板只读真实 runtime truth：
  - `PlayerTag + QuestLogComponent`
  - `const QuestCatalog*`
  - 可选 `const ItemCatalog*`
  - `QuestTurnInService`
- 面板不读取 `SaveData`，也不新增 quest 专用 save schema；当前玩家的 quest 状态仍以 `QuestLogComponent` 为唯一真相。
- 面板每帧自动查找当前玩家实体；若未找到玩家、`QuestCatalog` 缺失或 `QuestTurnInService` 不可用，面板进入只读/不可操作状态并显示明确提示。
- quest 列表固定来自 `QuestCatalog`，并提供最小筛选：
  - `All`
  - `Offerable`
  - `Active`
  - `ReadyToTurnIn`
  - `Completed`
- `Offerable` 的判定规则在第一版固定为：`!isQuestActive && !isQuestCompleted`。Milestone C 还没有前置条件或任务链锁，因此不额外引入 `NotStarted` / `Locked` 分类。
- `ReadyToTurnIn` 与状态标签统一复用 `quest_log_ops::isQuestReadyToTurnIn(...)`，不在调试面板里自造第二套规则。
- 选中 quest 后，面板至少显示：
  - `quest_id`
  - `title`
  - `description`
  - 当前状态
  - objectives 的 `current / required`
  - 最小 reward 预览
- 调试动作保持“原子且可复现”，优先覆盖 Milestone C 的核心链路：
  - `Accept`
  - `Reset Selected`
  - `+Progress Objective`
  - `Fill Objective`
  - `Fill All Objectives`
  - `Turn In`
  - `Clear All Quest State`
- 按钮语义固定如下：
  - `Accept` 复用 `quest_log_ops::tryAcceptQuest(...)`
  - `Turn In` 必须复用 `QuestTurnInService::turnIn(...)`，确保真实奖励写回、`active -> completed` 迁移和 progress cleanup 都走正式路径
  - objective 调整与 reset 允许在 debug 面板内部通过小型 helper 直接改 `QuestLogComponent`，但必须统一 `clamp`，并复用 `makeQuestObjectiveProgressKey(...)` / `eraseQuestProgress(...)`
- 为了保持 debug 工具可预测，第一版不做“伪造 `BattleEndedEvent`”或“伪造 NPC `InteractCommand`”。
  - battle progress 测试由面板直接调整 objective progress 完成
  - giver 交互测试继续走现有地图/NPC 入口
- 面板内部保留一个 `status_` 文本，用于显示最近一次操作结果；不额外发 `DialogueShowEvent`，也不引入新的 `QuestChangedEvent`。
- `Turn In` 若发放 item reward，仍由 `QuestTurnInService -> InventoryDomainService` 走正式写回链，因此现有背包/热栏相关事件无需额外复制。
- 第一版不做“一键完成 quest”宏按钮，先保留原子操作，便于开发时区分是“接取、推进还是交付”哪一步出问题。

## 需要新增的文件

- `src/game/debug/quest_debug_panel.h`
- `src/game/debug/quest_debug_panel.cpp`
- `tests/game/quest_debug_panel_registration_test.cpp`
- `tests/game/quest_debug_panel_helpers_test.cpp`

## 实现步骤

### Step 1. 定义 QuestDebugPanel 与最小面板状态

- 新增 `QuestDebugPanel`，继承 `engine::debug::DebugPanel`。
- 构造依赖建议固定为：
  - `entt::registry&`
  - `const game::data::QuestCatalog*`
  - `const game::data::ItemCatalog*`
  - `game::domain::QuestTurnInService*`
- 面板内部状态保持最小：
  - 当前筛选模式
  - 当前选中的 `quest_id`
  - 当前选中的 objective 索引
  - 可调节的 `progress_step_`
  - 最近一次操作状态文本
- 玩家实体不做手工输入；每帧自动查找第一个带 `PlayerTag + QuestLogComponent` 的实体。
- `progress_step_` 默认值固定为 `1`，并通过 `ImGui::InputInt` 或等价控件暴露给开发者；所有“推进当前 objective”按钮都基于这个步长工作。

### Step 2. 实现只读 quest 检视区

- 顶部显示当前玩家 quest 总览：
  - active 数量
  - completed 数量
  - ready-to-turn-in 数量
- 左侧或上方提供 quest 下拉框/列表，数据源固定为 `QuestCatalog::listQuests()` 或等价遍历接口。
- quest 行展示建议至少包含：
  - `title`
  - `quest_id`
  - 当前状态标签
- 选中 quest 后显示详情：
  - description
  - objectives 列表
  - 每个 objective 的当前进度、目标值和 enemy 标识
  - reward 预览；若 `ItemCatalog` 可用则显示物品名，否则回退到 `item_id`

### Step 3. 实现最小调试动作

- `Accept`：
  - 若 quest 处于 `Offerable`，调用 `quest_log_ops::tryAcceptQuest(...)`
  - 若已 active/completed，则显示状态提示而不是重复写入
- `+Progress Objective` / `Fill Objective`：
  - 只修改当前选中 objective 的 progress
  - `+Progress Objective` 实际按 `progress_step_` 递增，默认等价于 `+1`
  - 写回时必须按 `required_count` 做 `clamp`
  - progress key 必须通过 `makeQuestObjectiveProgressKey(...)` 生成
- `Fill All Objectives`：
  - 把选中 quest 的所有 objective 直接补到 `required_count`
  - 不改变 active/completed 列表，只让 quest 进入 `ReadyToTurnIn`
- `Reset Selected`：
  - 从 `active_quests` / `completed_quests` 中移除该 quest
  - 清理该 quest 的全部 progress keys 时必须复用 `quest_log_ops::eraseQuestProgress(...)`
- `Clear All Quest State`：
  - 清空玩家 `QuestLogComponent` 的 `active_quests / completed_quests / objective_progress`
  - 这是 debug-only 便捷操作，不走新的 domain service
- `Turn In`：
  - 只对当前选中 quest 执行
  - 先通过 `QuestCatalog::findQuest(selected_quest_id)` 获取 `const QuestData*`
  - 若 quest 定义缺失，则直接显示错误并中止
  - 必须调用 `QuestTurnInService::turnIn(player, *quest, quest_log)`
  - 成功后在状态栏显示奖励摘要和完成结果
  - 失败时显示 `failure_message`

### Step 4. 接入 GameScene 的 Game 分类调试面板

- 在 `GameScene::registerDebugPanels()` 中注册 `QuestDebugPanel`。
- 注册条件固定为：
  - `services_->quest_catalog` 可用
  - `services_->quest_turn_in_service` 可用
- 面板归类为 `engine::debug::PanelCategory::Game`，默认关闭。
- 这一阶段不修改 `GameRuntimeServices`、scheduler 或 quest system 装配；debug panel 只消费已有服务。

### Step 5. 补齐最小回归测试

- 新增 `quest_debug_panel_registration_test.cpp`，至少覆盖：
  - `GameScene::registerDebugPanels()` 已注册 `QuestDebugPanel`
  - 注册分类为 `PanelCategory::Game`
  - 注册前置条件包含 `quest_catalog` 与 `quest_turn_in_service`
- 继续使用 source-based smoke test，而不是做 ImGui 交互回放测试。
- 推荐把 quest 过滤/状态构建抽成纯 helper，并新增 `quest_debug_panel_helpers_test.cpp`，至少验证：
  - `Offerable / Active / ReadyToTurnIn / Completed` 分类正确
  - `Offerable` 明确等价于 `!active && !completed`
  - objective progress 显示值会 `clamp`
  - 缺失 quest 定义时 helper 会安全跳过并返回可见错误状态

## ToDo

- [ ] 新增独立 `QuestDebugPanel`
- [ ] 让面板自动绑定当前玩家的 `QuestLogComponent`
- [ ] 接入 `QuestCatalog` 驱动的 quest 列表与筛选
- [ ] 在面板中显示选中 quest 的 objectives、状态与 reward 预览
- [ ] 复用 `quest_log_ops::tryAcceptQuest(...)` 实现 `Accept`
- [ ] 实现基于 `progress_step_` 的 objective `+Progress / Fill / Fill All` 调试动作，并统一 `clamp`
- [ ] 实现 `Reset Selected`，并复用 `eraseQuestProgress(...)` 清理 progress keys
- [ ] 实现 `Clear All Quest State`
- [ ] 复用 `QuestTurnInService::turnIn(...)` 实现真实 turn-in 调试入口
- [ ] 在 `GameScene::registerDebugPanels()` 中把面板注册到 `PanelCategory::Game`
- [ ] 补齐 quest debug panel 的 source-based registration smoke test
- [ ] 抽取 quest debug helper 并补齐筛选 / 状态 / clamp 单测
