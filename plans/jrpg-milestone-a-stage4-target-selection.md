# Milestone A / Stage 4: 目标选择与动作提交闭环计划

## Context

Stage 1-3 已完成战斗菜单骨架、技能候选、物品候选与战斗物品库存写回基础。当前状态是：

- `BattleScene` 已有 `MainMenu / SkillList / ItemList / TargetSelect` 子状态机
- `TargetEntryViewModel` 已绑定到 `battle.rml` 的 `target_entries_`
- `menu_up/down/left/right/confirm/cancel` 已由 `BattleScene` 场景级处理
- `SkillList` 会写入 `ActionDraft::selected_skill_id`
- `ItemList` 会写入 `ActionDraft::selected_item_id`
- `TargetSelect -> cancel` 已根据 `ActionDraft::pending_type` 返回来源菜单，并保留来源列表数据
- `BattleActionResolver` 已支持 `Scope::OneEnemy / AllEnemies / OneAlly / AllAllies / Self / None`
- `BattleActionResolver` 已支持 `BattleActionType::Attack / Skill / Item` 的最终执行
- `BattleScene::queueAttackAction()` 当前仍会用 `selectDefaultTarget()` 直接选择第一个敌人并提交
- `BattleScene::handleSkillEntry()` / `handleItemEntry()` 对需要目标的动作仍进入 `"Target selection coming in Stage 4"` 占位态
- `BattleScene::handleTargetEntry()` 当前只更新 target cursor，还没有写入 `selected_target_id` 或提交动作

因此，Stage 4 的核心是建立这条闭环：

`Attack / Skill / Item selection -> target entries or direct scope submit -> BattleAction -> BattleSession::submitAction()`

## 范围

### 本阶段包含

- 根据当前行动者与 action scope 生成真实 `target_entries_`
- `Attack` 改为显式 `OneEnemy` 目标选择，不再默认提交第一个敌人
- `Skill` / `Item` 的 `OneEnemy / OneAlly` scope 进入真实目标选择
- `Skill` / `Item` 的 `Self / AllEnemies / AllAllies` scope 直接构造并提交动作
- 鼠标点击和 `menu_confirm` 在 `TargetSelect` 下走同一套确认逻辑
- 目标确认后写入 `ActionDraft::selected_target_id` 并构造最终 `BattleAction`
- 目标选择失败或草稿失效时给出 `menu_hint_` 反馈，不崩溃、不提交半成品 action
- 保持 `TargetSelect -> cancel` 返回来源菜单的不变量
- 补充 BattleScene smoke / resolver 关键路径测试

### 本阶段不包含

- 敌方 AI 自动行动
- 多目标选择 UI 或全体目标预览动画
- 复活类目标规则
- 根据技能效果动态允许选择 KO 目标
- 目标详情面板、弱点提示、伤害预测
- 技能/物品描述 UI
- 结果文案 / 战斗日志优化
- 战斗动画、粒子、音效
- Stage 5 的 RML/RCSS 视觉收尾与导航补强

## 设计决策

### 1. Resolver 的 scope 语义保持为真相，BattleScene 只做 UI 预筛

当前 `BattleActionResolver::collectTargets()` 已经是执行层的最终判定点：

- `OneEnemy`: 显式 target 必须是存活敌方；若 action 无 target，resolver 会 fallback 到第一个敌人
- `OneAlly`: 显式 target 必须是存活友方；若 action 无 target，resolver 会 fallback 到 actor 自身
- `Self`: 使用 actor 自身
- `AllEnemies`: 所有存活敌方
- `AllAllies`: 所有存活友方
- `None`: rejected

Stage 4 不修改这套执行层语义，但 UI 不应继续依赖 resolver 的 fallback。`BattleScene` 应在用户可交互路径中明确：

- `OneEnemy / OneAlly` 必须由 UI 选定目标后提交
- `Self / AllEnemies / AllAllies` 不弹出目标选择，直接提交
- `Scope::None` 保持 disabled，不进入提交路径

这样可以让玩家行为和 resolver 行为一致，同时保留 resolver 对脚本 / 测试 / 未来 AI 的保护能力。

### 2. 目标列表从当前 actor side 与 session units 生成

推荐新增 helper：

```cpp
void populateTargetEntries(game::data::Scope scope, const game::battle::BattleUnit& actor);
```

目标候选规则：

- `Scope::OneEnemy`: `unit.side != actor.side`
- `Scope::OneAlly`: `unit.side == actor.side`
- dead unit 可以出现在列表中，但 `enabled = false`，`is_dead = true`
- 当前 Stage 4 没有复活物品/技能，所以 dead unit 不可确认
- `TargetEntryViewModel::label` 建议包含 `name HP current/max`，dead unit 追加 `(KO)`
- `TargetEntryViewModel::is_ally` 使用 `unit.side == actor.side`
- `TargetEntryViewModel::unit_id` 继续使用现有 `int` RmlUi 字段；提交时转换回 `BattleUnitId`
- `target_entry_cursor_` 指向第一个 enabled target；如果没有 enabled target，可 fallback 到第一个条目用于显示焦点，但确认时必须被 disabled guard 拦截

如果没有任何 matching units：

- `target_entries_` 为空
- `target_empty_text_ = "No valid targets"`
- `target_entry_cursor_ = -1`
- `menu_confirm` 不提交

### 3. Attack 改为目标选择动作

当前 `queueAttackAction()` 会：

- 找当前 actor
- `selectDefaultTarget(actor->side)`
- 直接 `submitAction(BattleAction{Attack, actor_id, target_id})`

Stage 4 应改为：

- 找当前 actor
- 写入 `ActionDraft{ .pending_type = BattleActionType::Attack, .requires_target_selection = true }`
- `populateTargetEntries(Scope::OneEnemy, *actor)`
- `setMenuState(MenuState::TargetSelect)`

`selectDefaultTarget()` 在 UI 提交路径中不再需要。实现时可以删除它，或仅保留给后续 AI / 脚本路径；如果保留，不应继续被 `queueAttackAction()` 使用。

### 4. Attack / Skill / Item 使用统一 scope 分流 helper

Stage 4 锁定新增统一 helper，避免 `Attack / Skill / Item` 三处分流逻辑重复：

```cpp
void continueDraftAfterScopeSelected(game::data::Scope scope, const game::battle::BattleUnit& actor);
```

调用约定：

- `queueAttackAction()` 写入 `ActionDraft{ .pending_type = Attack }` 后调用 `continueDraftAfterScopeSelected(Scope::OneEnemy, *actor)`
- `handleSkillEntry()` 写入 `selected_skill_id` 后用 `SkillData::scope_` 调用 helper
- `handleItemEntry()` 写入 `selected_item_id` 后用 `BattleItemUseConfig::scope` 调用 helper

分流规则：

- `OneEnemy / OneAlly`: 填充 target list，进入 `TargetSelect`
- `Self / AllEnemies / AllAllies`: 直接调用 `submitDraftAction()`
- `None`: 不应发生，因为列表 entry 已 disabled；若发生，提示 `"Action cannot be used."`

注意不变量：

- 从 `SkillList / ItemList` 进入 `TargetSelect` 时不要清空 `list_entries_`
- `TargetSelect -> cancel` 返回 `SkillList / ItemList` 时保持 `selected_skill_id` / `selected_item_id`
- 返回来源菜单时只清除 `selected_target_id`

### 5. 用一个 helper 构造最终 BattleAction

推荐新增：

```cpp
bool submitDraftAction();
```

职责：

- 调用 `prepareActionActor(actor_id)` 获取当前 actor
- 根据 `action_draft_.pending_type` 构造 `BattleAction`
- 对 `Attack` 要求 `selected_target_id.has_value()`
- 对 `Skill` 要求 `selected_skill_id.has_value()`
- 对 `Item` 要求 `selected_item_id.has_value()`
- 若 `requires_target_selection == true`，要求 `selected_target_id.has_value()`
- 对 direct scope action，`target_id` 保持 `std::nullopt`
- 构造完成后调用现有 `submitAction(std::move(action))`
- 实现时按 `pending_type` 使用 `switch` 分别构造 `Attack / Skill / Item`，不要为了统一而填充无关字段；`Attack` 只需要 `type / actor_id / target_id`，`skill_id` / `item_id` 依赖 `BattleAction` 默认空值即可

草稿失效时：

- 不调用 `submitAction()`
- 更新 `menu_hint_`，例如 `"Action is no longer available."`
- `document_controller_.markDirty("menu_hint")`
- 留在当前菜单，允许玩家 cancel 或重新选择

`submitDraftAction()` 不负责重新计算 skill/item enabled 状态；enabled guard 仍留在列表选择阶段。若提交时 stock / MP 已变化，resolver 会再次拒绝并通过现有 result flow 显示失败。

### 6. 目标确认必须检查 enabled

当前 `handleTargetEntry(int entry_index)` 只设置 cursor。Stage 4 应改为：

- 使用 `findTargetEntry(entry_index)` 找回 ViewModel
- 更新 `target_entry_cursor_`
- 若 entry 不存在或 `!entry.enabled`，直接返回
- `action_draft_.selected_target_id = static_cast<BattleUnitId>(entry.unit_id)`
- 调用 `submitDraftAction()`

建议新增：

```cpp
const TargetEntryViewModel* findTargetEntry(int entry_index) const;
int firstEnabledTargetEntryIndex() const;
```

`menu_confirm` 和 RML `target_entry_select(target.entry_index)` 均调用 `handleTargetEntry()`，因此鼠标与键盘/手柄路径自然复用。

### 7. Stage 4 不改 result text

当前 `BattleScene::refreshView()` 对 skill result 主要显示 damage，对 item result 只显示 `"Item used"`。

Stage 4 明确不修改 result text：

- 当前 result flow 已经能显示 rejected failure reason，并能对 applied action 给出基本反馈
- `"Item used"` 虽然简略，但不影响目标选择与动作提交闭环的验证
- 更完整的 skill/item recover 文案、逐目标 breakdown、战斗 log 统一留到 Stage 5 的 UI 收尾

## 实现步骤

### Step 1: 新增目标列表 helper

修改：

- `src/game/scene/battle_scene.h`
- `src/game/scene/battle_scene.cpp`

建议新增：

- `populateTargetEntries(Scope scope, const BattleUnit& actor)`
- `findTargetEntry(int entry_index) const`
- `firstEnabledTargetEntryIndex() const`
- `targetLabel(const BattleUnit& unit) const` 可选

要点：

- `target_entries_.clear()`
- `target_empty_text_ = "No valid targets"`
- 遍历 `session_.units()`
- 只加入 scope 匹配 side 的单位
- dead unit `enabled = false`
- `target_entry_cursor_ = firstEnabledTargetEntryIndex()`
- 不触碰 `list_entries_`
- 填充后进入 `setMenuState(MenuState::TargetSelect)`

### Step 2: 改造 Attack 入口

修改：

- `BattleScene::queueAttackAction()`
- 可选删除或停用 `selectDefaultTarget()`

要点：

- 不再直接 `submitAction()`
- 写入 `ActionDraft::pending_type = BattleActionType::Attack`
- `selected_skill_id / selected_item_id / selected_target_id` 清空
- `requires_target_selection = true`
- 调用 `continueDraftAfterScopeSelected(Scope::OneEnemy, *actor)` 进入目标选择
- 没有有效敌方目标时留在目标菜单并显示空态；不要提交默认 action

### Step 3: 改造 Skill / Item scope 分流

修改：

- `BattleScene::handleSkillEntry(...)`
- `BattleScene::handleItemEntry(...)`

要点：

- 选择 enabled entry 后继续写入 `ActionDraft`
- 统一调用 `continueDraftAfterScopeSelected(scope, *actor)`
- `continueDraftAfterScopeSelected()` 内部负责 `OneEnemy / OneAlly` 的 target populate 和 `Self / AllEnemies / AllAllies` 的 direct submit
- 移除 `"Target selection coming in Stage 4"` 占位文案
- 删除 `enterTargetPlaceholder()` 方法，避免 Stage 4 后残留无人使用的占位路径
- 若 catalog 或 item stock 重新查询失败，更新 `menu_hint_` 并留在当前列表

### Step 4: 目标确认提交最终动作

修改：

- `BattleScene::handleTargetEntry(...)`
- 新增 `submitDraftAction()`

要点：

- `handleTargetEntry()` 必须检查 entry enabled
- 目标确认后写入 `selected_target_id`
- `submitDraftAction()` 统一构造 Attack / Skill / Item
- direct scope 的 Skill / Item 不带 `target_id`
- target scope 的 Attack / Skill / Item 带 `target_id`
- `submitDraftAction()` 内部按 `pending_type` 分支构造 action，不给 `Attack` 填 `skill_id` / `item_id`
- 成功构造后调用现有 `submitAction(...)`

### Step 5: 保持 cancel 与隐藏面板数据不变量

修改：

- `BattleScene::onMenuCancelPressed()` 如需要
- `BattleScene::setMenuState(...)` 如需要

要点：

- `TargetSelect -> cancel` 只清除 `selected_target_id`
- `SkillList -> TargetSelect -> cancel` 返回时 `list_entries_` 保持技能列表
- `ItemList -> TargetSelect -> cancel` 返回时 `list_entries_` 保持物品列表
- `Attack -> TargetSelect -> cancel` 返回 `MainMenu`
- 当前 `menuStateForActionDraftSource()` 已对 `BattleActionType::Attack` 返回 `MenuState::MainMenu`；Stage 4 只需确保 `queueAttackAction()` 正确写入 `pending_type = Attack`，通常不需要额外修改 cancel 分支
- 不在同一帧因为隐藏 target panel 而清空 data-for backing vector

### Step 6: 补测试

建议覆盖：

- `BattleSceneSmokeTest`:
  - 存在 `populateTargetEntries`
  - 存在 `continueDraftAfterScopeSelected`
  - 存在 `submitDraftAction`
  - `handleTargetEntry` 写入 `selected_target_id`
  - `queueAttackAction` 不再调用 `selectDefaultTarget`
  - `queueAttackAction` 通过 `continueDraftAfterScopeSelected(Scope::OneEnemy, *actor)` 进入 target flow
  - `handleSkillEntry` / `handleItemEntry` 不再包含 `"Target selection coming in Stage 4"`
  - `enterTargetPlaceholder` 不再存在或不再被引用
  - `Self / AllEnemies / AllAllies` direct submit 路径存在
  - `TargetSelect -> cancel` 仍调用 `menuStateForActionDraftSource()`

- `BattleActionResolverTest`:
  - 现有 scope 测试可继续作为执行层回归
  - 可补一个 item `OneAlly` 显式 target 成功用例，避免只覆盖默认 actor fallback

- `BattleSessionTest`:
  - 可补一个 `Skill` / `Item` 带显式 target 的 session action applied 用例

若没有合适的 headless RmlUi 场景测试环境，本阶段先用 source smoke + resolver/session 行为测试覆盖核心路径，Stage 5 再补 UI 导航层测试。

## ToDo

- [ ] 新增真实 `target_entries_` 构建、target 查找与第一个 enabled target helper
- [ ] `Attack` 改为显式 `OneEnemy` 目标选择
- [ ] `Skill` 的 `OneEnemy / OneAlly` scope 进入真实目标选择
- [ ] `Item` 的 `OneEnemy / OneAlly` scope 进入真实目标选择
- [ ] `Skill` 的 `Self / AllEnemies / AllAllies` scope 直接提交
- [ ] `Item` 的 `Self / AllEnemies / AllAllies` scope 直接提交
- [ ] 新增 `continueDraftAfterScopeSelected()` 并让 Attack / Skill / Item 复用
- [ ] 新增统一 `submitDraftAction()` 构造最终 `BattleAction`
- [ ] `handleTargetEntry()` 写入 `selected_target_id` 并提交草稿
- [ ] 移除 Stage 2/3 的 `"Target selection coming in Stage 4"` 占位路径
- [ ] 删除或彻底停用 `enterTargetPlaceholder()`
- [ ] 保持 `TargetSelect -> cancel` 返回来源菜单且不清空 `list_entries_`
- [ ] 补充 BattleScene smoke / resolver / session 测试

## 完成标准

- `Attack` 不再自动选择第一个敌人，而是进入目标选择
- `Skill` / `Item` 的单体 scope 能显示真实目标列表并在确认后提交
- `Skill` / `Item` 的 self / all scope 能不弹目标菜单直接提交
- disabled / dead target 不会被确认提交
- 鼠标点击 target 和 `menu_confirm` target 走同一条提交路径
- 从 target 菜单 cancel 能回到正确来源菜单并保持列表数据
- `BattleSession::submitAction()` 收到的 `BattleAction` 带正确 `type / actor_id / skill_id / item_id / target_id`
- resolver / session 行为测试通过
- `ninja -C build/debug tests/game_tests` 通过
- `ctest --test-dir build/debug --output-on-failure` 通过
