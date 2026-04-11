# Milestone C / Stage 5: Quest UI、存档与测试补强细化计划

## 实现思路

- Stage 5 不再重开 quest runtime truth；`QuestLogComponent + QuestCatalog` 继续是任务页与存档的唯一真实来源，UI 不直接读取 `SaveData`。
- Quest UI 继续嵌在现有 `InventoryMenuScene` 里，沿用 `RmlDocumentController + IMenuTabContent` 架构；只把 `Quests` 页从 placeholder 升级成真实 `QuestTabContent`，不新增独立 Quest Scene，也不改 tabset 主结构。
- `QuestTabContent` 的静态依赖固定为：
  - `QuestCatalog*`
  - `entt::registry&`
  - `entt::entity player`
- 因此 `InventoryMenuScene` 构造函数需要补 `QuestCatalog*` 参数，并由打开菜单的调用方透传 runtime 中的 quest catalog；不在 UI 层临时做 service locator。
- `QuestTabContent` 只负责把玩家当前任务状态映射成最小 view model，不承担接任务、交付、战斗推进等 domain 逻辑。
- Quest 页展示固定保持最小闭环：
  - `Active` 区显示 `title + progress summary`
  - `Completed` 区显示最小标题列表
  - 两区都要有 empty state，避免空面板
- progress summary 统一基于 `QuestData.objectives_ + QuestLogComponent.objective_progress` 现算，不在 UI 层额外缓存 quest state，也不把“进度摘要字符串”反写回 runtime/save。
- Milestone C 当前 objective 只有 `DefeatEnemyCount`，Quest 页也只需要支持这一路径；summary 文案可直接格式化为 `Slime 2/3` 或等价最小文本，不提前做多类型 objective formatter。
- `status_label` 不在 UI 层自行推导业务规则；view model 构建时直接复用现有 `quest_log_ops::isQuestReadyToTurnIn(...)` 区分 `进行中` 与 `可交付`。
- `QuestTabContent` 的刷新策略保持最小：
  - `onActivated()` 时全量同步一次
  - `update()` 默认不做额外轮询
  - 不为 Milestone C 新增 `QuestChangedEvent`
- 这样做的前提是 `InventoryMenuScene` 每次打开都是 fresh open，且菜单处于暂停态；打开菜单期间 quest 状态不会继续推进，也不存在“同一个 QuestTabContent 挂着等运行时变更”的需求。若后续要支持菜单内热刷新，再单独引入 quest UI event，不在 Stage 5 提前扩协议。
- Stage 1 已经把 `SaveService::capture()/apply()` 接到 `QuestLogComponent`；Stage 5 不再改 save schema、不再改 migrator，只补“接任务 / 推进 / 完成后”的 roundtrip 回归，确保 active/completed/progress 在读写后仍正确。
- 这里的 save 回归目标不是重复 Stage 1 的“基础字段已接线”，而是验证真实玩法流转后的 quest state 仍能 roundtrip：accept 后的 active 状态、battle progress 后的 objective_progress、turn-in 后的 completed 状态与 progress cleanup。
- Stage 5 的重点是“信息可见”和“结果可回归”，不是 UI 花样：
  - 不做地图追踪、任务排序筛选、分页、动画面板
  - 不新增奖励回顾、giver portrait、剧情分支列表
- Quest 页仍应保持当前菜单的 UI 语言：复用 `inventory_menu.rml / inventory_menu.rcss`，直接替换 `panel-quests` 的 placeholder 内容，不单开新的 RML 文档。

## 需要新增的文件

- `src/game/ui/quest_tab_content.h`
- `src/game/ui/quest_tab_content.cpp`
- `tests/game/quest_tab_content_test.cpp`
- `tests/game/quest_save_roundtrip_test.cpp`

## 实现步骤

### Step 1. 定义 QuestTabContent 与最小 view model

- 新增 `QuestTabContent`，实现 `IMenuTabContent`。
- view model 推荐至少保留：
  - `title`
  - `description`
  - `progress_summary`
  - `status_label`
- `Active` 与 `Completed` 使用两组独立数组绑定，建议变量名固定为 `active_quest_entries` / `completed_quest_entries`，避免和 inventory model 命名冲突。
- Quest 列表继续通过 `data-for` 渲染，不单独引入新的 UI 绑定模式。
- `progress_summary` 在 C++ 内部统一生成；UI 只消费已经格式化好的最小字符串。
- `status_label` 推荐最小值集合：
  - active 且未满足目标：`进行中`
  - active 且已满足目标：`可交付`
  - completed：`已完成`
- 对 `Completed` quest，`progress_summary` 可留空或直接不显示，不重复展示已清理掉的 progress keys。
- `QuestTabContent::onCancel()` 固定返回 `false`，让 `InventoryMenuScene` 继续沿用现有的“没有子面板时直接关闭菜单”行为。

### Step 2. 把 Quests 页接入 InventoryMenuScene

- `InventoryMenuScene` 构造函数补 `QuestCatalog*` 参数，`GameScene` 打开菜单时透传 `services_->quest_catalog.get()`。
- `InventoryMenuScene::initUI()` 用 `QuestTabContent` 替换当前 `MenuTabId::Quests` 的 `PlaceholderTabContent`。
- 不新增新的 `RmlDocumentController`；Quest 页继续和 Inventory 页共用 `inventory_menu.rml`。
- `panel-quests` 改成真实任务面板，建议最小结构为：
  - `Active Quests` 标题
  - active quest 列表
  - active empty state
  - `Completed Quests` 标题
  - completed quest 列表
  - completed empty state
- Quest 页不需要 slot grid、tooltip、drag/drop 或 action menu；避免把 InventoryTabContent 的交互复杂度复制进去。

### Step 3. 统一 progress summary 与 empty state 规则

- active quest 的 summary 固定从 `QuestCatalog` 查定义，再按 objective 顺序拼接。
- 每个 objective 的显示值要对 `required_count` 做 `clamp`，避免旧存档或异常数据把 UI 显示成 `7/3`。
- quest 定义缺失时不让 UI 崩溃；建议 `warn` 后跳过该条 quest，而不是在任务页里展示半残数据。
- empty state 固定最小文案即可，例如：
  - active 为空：`暂无进行中的任务`
  - completed 为空：`暂无已完成的任务`

### Step 4. 补齐 save 与 UI 回归测试

- 新增 `quest_tab_content_test.cpp`，至少覆盖：
  - active/completed quest 能正确映射到 view model
  - `status_label` 会复用 ready-to-turn-in 判定区分 `进行中 / 可交付`
  - progress summary 能根据 objective progress 正确格式化
  - completed quest 不依赖已被清理的 progress keys
  - empty quest log 会落到 empty state
- 新增 `quest_save_roundtrip_test.cpp`，至少覆盖：
  - accept 后 active quest 的 capture/apply roundtrip
  - battle progress 后 objective_progress 的 capture/apply roundtrip
  - Stage 4 完成后的“completed + 已清理 progress”状态 roundtrip
- 现有 `inventory_menu_scene_slot_grid_registration_test.cpp` 已存在，可同步更新，确认 `Quests` 页不再走 placeholder。
- 若 Quest 页 RML 结构变动较多，可补一个 source-based test，确保 `panel-quests` 已绑定真实列表与 empty state 区块。

## ToDo

- [ ] 新增 `QuestTabContent` 与 quest 页最小 view model
- [ ] 给 `InventoryMenuScene` / 菜单打开路径补 `QuestCatalog*` 注入
- [ ] 在 `InventoryMenuScene` 中用 `QuestTabContent` 替换 `Quests` placeholder
- [ ] 在 `inventory_menu.rml` 中落地 active/completed quest 列表与 empty state
- [ ] 统一 active quest 的 progress summary 格式化规则
- [ ] 复用 `quest_log_ops::isQuestReadyToTurnIn(...)` 生成 `status_label`
- [ ] 对 UI 显示值按 `required_count` 做 `clamp`
- [ ] 缺失 quest 定义时在任务页安全跳过并记录 `warn`
- [ ] 补齐 quest 页 view model / RML 接线测试
- [ ] 补齐 quest save roundtrip 测试
