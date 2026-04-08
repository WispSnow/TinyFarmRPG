# Milestone A / Stage 2: 技能候选数据接线计划

## Context

Stage 1 已完成战斗菜单骨架：

- `BattleScene` 保留顶层 `FlowState`，新增 `MenuState`
- 主菜单、列表、目标三层 ViewModel 已通过 `RmlDocumentController + Rml::DataTypeRegister` 绑定到 RmlUi
- `menu_up/down/left/right/confirm/cancel` 已由 `BattleScene` 场景级处理
- `Skill` / `Item` 已从硬编码默认动作改为进入空列表占位态

当前 Stage 2 要解决的是技能列表的数据来源，而不是目标选择和最终技能提交。

现状约束：

- `BattleActionResolver` 已能执行 `BattleActionType::Skill`，前提是 `BattleAction.skill_id` 非空且 `BattleSessionOptions.rpg_catalog` 可用
- `SkillData` 已包含 `id_ / id_hash_ / display_name_ / description_ / scope_ / hit_type_ / mp_cost_ / success_rate_ / repeats_ / damage_ / effects_`
- `EnemyData` 已有 `actions_`，其中每个 `EnemyActionData` 带 `skill_id_ / rating_`
- `ActorData` 目前没有技能列表字段
- `BattleUnit` 目前只有战斗数值，没有技能列表桥接字段
- `SaveData::skill_state.learned_skills` 已存在，但当前是全局保存字段，尚未接入战斗单位构建，也不是 actor 粒度

因此，Stage 2 的核心是先建立一条最小、明确、可测试的技能来源链路：

`RPG catalog actor/enemy data -> BattleUnit.skill_ids -> BattleScene skill list_entries_`

## 范围

### 本阶段包含

- 为战斗单位增加最小技能列表桥接字段
- 为玩家 actor 建立数据驱动的技能来源
- 从敌人 action 列表提取战斗内可用技能 id
- 在 `BattleScene` 中根据当前行动者生成 `SkillList`
- 根据 catalog 技能数据生成 `ListEntryViewModel`
- 根据当前 MP / skill scope 计算技能 entry enabled 状态
- 鼠标点击和 `menu_confirm` 选择技能时写入 `ActionDraft::selected_skill_id`
- 补充 catalog / battle unit factory / BattleScene 接线 smoke 测试

### 本阶段不包含

- 目标候选生成
- 单体 / 全体 / 自身 scope 的最终提交分支
- 物品候选数据接线
- 敌方 AI 权重选择
- 技能学习界面
- 技能升级、冷却、装备技能栏
- 从存档 `SkillStateSaveData` 读取真实 actor learned skills

## 设计决策

### 1. 技能列表挂在 `BattleUnit`，不在 UI 临时深查 catalog

推荐新增字段：

```cpp
struct BattleUnit {
    ...
    std::vector<std::string> skill_ids{};
};
```

原因：

- `BattleScene` 只应该知道当前行动者“战斗内可用技能 id 列表”，不应该临时从 `ActorData -> ClassData -> SaveData` 推导
- 敌人和玩家可以共用同一个 `BattleUnit.skill_ids` 表示
- `BattleActionResolver` 仍只关心最终 `BattleAction.skill_id`，不会被 UI 候选逻辑污染
- 后续如果引入 actor learned skills / equipment skills / class skills，也可以只改 battle unit 构建层

### 2. 玩家技能来源先扩展 `ActorData::skill_ids_`

当前 `ActorData` 没有技能字段，而 `SaveData::skill_state.learned_skills` 也还没有 actor 粒度关系。Stage 2 采用最小数据驱动方案：

- 在 `ActorData` 增加 `std::vector<std::string> skill_ids_`
- `RpgCatalog::loadActors()` 解析可选 `skill_ids` 数组
- `RpgCatalog::validateReferences()` 校验 actor 引用的 skill 存在
- `assets/data/rpg/actors.json` 为 demo actor 填入初始技能

不建议本阶段从 class 深查技能：

- 当前 `ClassData` 只有基础参数，没有学习表
- 先从 actor 直接携带初始技能，最贴近“当前战斗单位已学技能”这个战斗层输入
- Stage 2 目标是技能菜单可用，不是完整成长系统建模

不建议本阶段接 `SaveData::skill_state.learned_skills`：

- 当前字段是全局列表，不区分 actor
- GameScene / runtime services 还没有暴露出“当前参战 actor 对应存档技能”的稳定接口
- 过早接入会把保存系统、角色成长和战斗菜单绑在一起

### 3. 敌人技能来源使用 `EnemyData::actions_`

敌人单位构建时：

- 遍历 `enemy.actions_`
- 按出现顺序提取 `action.skill_id_`
- 去重后写入 `BattleUnit.skill_ids`
- 暂时忽略 `rating_`，它留给后续 AI 选择使用

如果某个 enemy 没有 actions：

- `BattleUnit.skill_ids` 可以为空
- Stage 2 不自动补 `skill.attack`
- 是否给所有敌人默认 attack 应由 catalog 数据保证，而不是 battle factory 暗中补默认值

### 4. BattleScene 需要保留 catalog 指针用于 UI 候选展示

当前 `BattleSessionOptions.rpg_catalog` 只传给 `BattleSession` 的 resolver 依赖，`BattleScene` 自己无法查 `SkillData`。

Stage 2 推荐在 `BattleScene` 中新增非 owning 指针：

```cpp
const game::data::RpgCatalog* rpg_catalog_{nullptr};
```

构造时在 move `BattleSessionOptions` 前复制：

```cpp
BattleScene::BattleScene(..., game::battle::BattleSessionOptions session_options)
    : engine::scene::Scene(name, context),
      rpg_catalog_(session_options.rpg_catalog),
      session_(std::move(units), std::move(session_options)) {
}
```

注意声明顺序：

- `rpg_catalog_` 应声明在 `session_` 之前，避免初始化顺序导致从 moved-from options 读取
- 构造函数初始化列表也应保持 `rpg_catalog_` 在 `session_` 之前，避免 `-Wreorder` 警告
- `BattleSession` 仍保留自己的 resolver catalog 依赖，UI 展示和动作执行各自只持有非 owning 指针

### 5. SkillList 使用现有 `ListEntryViewModel`

Stage 1 已定义：

```cpp
struct ListEntryViewModel {
    int entry_index;
    Rml::String entry_id;
    Rml::String label;
    Rml::String sublabel;
    bool enabled;
};
```

Stage 2 不新增专用 `SkillEntryViewModel`。建议字段映射：

- `entry_index`: 当前列表索引
- `entry_id`: `SkillData::id_`
- `label`: `SkillData::display_name_`，为空时 fallback 到 `id_`
- `sublabel`: `MP <cost>`；若 MP 不足可显示 `MP <cost> / Low MP`
- `enabled`: `actor.mp >= skill.mp_cost_ && skill.scope_ != Scope::None`

`SkillData::description_` 暂不进 ViewModel：

- 当前 `ListEntryViewModel` 没有 detail 字段
- Stage 5 做 UI 收尾时再决定是否扩展描述区
- 先用 `sublabel` 保持列表最小可读

### 6. 缺失或非法技能的处理

构建层与 UI 层分工：

- `RpgCatalog::validateReferences()` 负责发现 actor/enemy 引用不存在的技能
- `BattleUnitFactoryTest` 覆盖 actor/enemy skill ids 正确进入 `BattleUnit`
- `BattleScene` 生成列表时仍应对缺失 catalog / missing skill 做轻量保护

推荐行为：

- `rpg_catalog_ == nullptr`: `SkillList` 显示空列表，文案为 `"No skills available"`
- `BattleUnit.skill_ids` 为空: 显示空列表
- 单个 `skill_id` 在 catalog 中不存在: 跳过该条目并记录 warn
- `Scope::None`: 可显示但 disabled，避免后续提交到 resolver 后才失败
- 如果后续区分主动 / 被动技能或技能类型，`Scope::None` 应直接跳过而不是 disabled；当前保留在列表中主要是临时调试策略
- MP 不足: 显示但 disabled，让玩家知道技能存在但当前不可用

### 7. 技能选择只写草稿，不在 Stage 2 完成提交

Stage 2 的边界是“真实技能列表 + 选择上下文”，不实现目标生成和最终提交。

选择 enabled 技能时：

- `action_draft_.pending_type = BattleActionType::Skill`
- `action_draft_.selected_skill_id = entry.entry_id`
- `action_draft_.selected_item_id.reset()`
- 根据 `SkillData::scope_` 写入 `action_draft_.requires_target_selection`
- 暂不构造 `BattleAction`
- 暂不调用 `submitAction()`

若需要让 UI 有可见流转，推荐进入 `TargetSelect` 占位态：

- `OneEnemy / OneAlly`: 进入 `TargetSelect`，`target_entries_` 为空，显示 `"Target selection coming in Stage 4"`
- `Self / AllEnemies / AllAllies`: 暂时留在 `SkillList` 并更新 `menu_hint_`，或同样进入占位态；不要在 Stage 2 直接提交
- `Scope::None`: disabled，不可选择

如果 Stage 2 使用 `TargetSelect` 占位态，需要同时修正 `menu_cancel`：

- 当前从 `TargetSelect` 取消应根据 `action_draft_.pending_type` 返回来源菜单
- `Skill` 来源返回 `SkillList`
- `Item` 来源返回 `ItemList`
- `Attack` 来源返回 `MainMenu`
- `SkillList -> TargetSelect` 只修改 `target_entries_` 与 visible flags，不清空 `list_entries_`
- 从 `TargetSelect` 取消返回 `SkillList` / `ItemList` 时，列表数据应保持原状，不需要重新 populate
- 从 `TargetSelect` 取消不应全量 reset `action_draft_`，只清除 target 相关字段，并保留 `pending_type` 与 `selected_skill_id` / `selected_item_id`

推荐写成来源菜单 helper，避免后续 Item / Attack 分支重复：

```cpp
case MenuState::TargetSelect:
    action_draft_.selected_target_id.reset();
    setMenuState(menuStateForActionDraftSource());
    return true;
```

这条修正属于菜单流转补全，不等同于 Stage 4 的目标候选生成。

## 实现步骤

### Step 1: 扩展 catalog actor 技能字段

修改：

- `src/game/data/rpg_data.h`
- `src/game/data/rpg_catalog.cpp`
- `assets/data/rpg/actors.json`

要点：

- `ActorData` 增加 `skill_ids_`
- `loadActors()` 解析可选数组字段 `skill_ids`
- 字段缺失时保持空数组
- 非 string 条目视为 catalog 加载失败
- `validateReferences()` 校验 actor skill id 存在于 `skills_`
- demo actors 至少给一个非空技能列表，避免进入战斗后 Skill 菜单永远为空

### Step 2: 扩展 BattleUnit 技能桥接字段

修改：

- `src/game/battle/battle_types.h`
- `src/game/battle/battle_unit_factory.cpp`
- 相关 battle factory 测试

要点：

- `BattleUnit` 增加 `std::vector<std::string> skill_ids`
- 玩家单位从 `ActorData::skill_ids_` 填入
- 敌人单位从 `EnemyData::actions_` 提取并去重
- 不自动补默认技能
- 直接构造 `BattleUnit` 的测试无需强制改动，默认空列表即可

### Step 3: BattleScene 保存 RpgCatalog 指针并生成 SkillList

修改：

- `src/game/scene/battle_scene.h`
- `src/game/scene/battle_scene.cpp`

要点：

- `BattleScene` 增加 `const game::data::RpgCatalog* rpg_catalog_{nullptr}`
- 构造函数在移动 `BattleSessionOptions` 前复制 `session_options.rpg_catalog`
- `queueSkillAction()` 不再只进入空列表，而是：
  - 读取当前 actor
  - 清空并重建 `list_entries_`
  - 对每个 `actor.skill_ids` 查找 `SkillData`
  - 写入 `ListEntryViewModel`
  - 设置 `list_empty_text_`
  - 设置 `list_entry_cursor_`
  - 直接调用 `setMenuState(MenuState::SkillList)`
- 明确选择实现路径 (a): `queueSkillAction()` populate 后不要调用 `enterListMenu(MenuState::SkillList)`，因为当前 `enterListMenu()` 会清空 `list_entries_`
- Stage 3 开始前，`queueItemAction()` 仍可保留现有 `enterListMenu(MenuState::ItemList)` 空列表占位路径；如果 Stage 3 也改为真实填充，再单独调整物品路径
- `list_entry_cursor_` 进入列表时 clamp 到第一个 enabled 条目；如果全 disabled，可保留第一个条目作为焦点但 `menu_confirm` 不提交

### Step 4: 技能选择写入 ActionDraft

修改：

- `BattleScene::handleListEntry()`
- 可选新增私有 helper，例如 `handleSkillEntry(...)` / `requiresTargetSelection(...)`

要点：

- 只有 `menu_state_ == MenuState::SkillList` 时按技能处理
- disabled 条目不响应确认
- 从 `entry.entry_id` 找回 `SkillData`
- 写入 `ActionDraft`
- 根据 scope 决定下一步占位流转
- 不提交 `BattleAction`
- 如果进入 `TargetSelect` 占位态，只填充 / 清空 `target_entries_` 与切换可见状态，不清空 `list_entries_`
- `TargetSelect` 下 `menu_cancel` 只清除 `selected_target_id` 并返回来源菜单，不全量 reset 动作草稿

建议 helper：

- `populateSkillEntries(const BattleUnit& actor)`
- `findListEntry(int entry_index)`
- `isSkillEntryEnabled(const BattleUnit& actor, const SkillData& skill)`
- `skillSubtitle(const BattleUnit& actor, const SkillData& skill)`
- `requiresTargetSelection(Scope scope)`
- `menuStateForActionDraftSource()`

### Step 5: 补测试

建议覆盖：

- `RpgCatalogTest`:
  - `loadActors()` 能读取 `skill_ids`
  - actor 引用缺失 skill 时 `validateReferences()` 失败

- `BattleUnitFactoryTest`:
  - 玩家单位从 actor skill ids 获得 `BattleUnit.skill_ids`
  - 敌人单位从 enemy actions 获得去重后的 `BattleUnit.skill_ids`

- `BattleSceneSmokeTest`:
  - `BattleScene` 保留 `rpg_catalog_` 指针
  - `queueSkillAction()` 通过 catalog 填充 `list_entries_`
  - `handleListEntry()` 写入 `selected_skill_id`
  - `Scope::None` / MP 不足存在 disabled 逻辑
  - Stage 2 不恢复硬编码 `skill.attack` 默认提交

## ToDo

- [ ] 为 `ActorData` 增加 `skill_ids_` 并解析 `actors.json` 的 `skill_ids`
- [ ] 在 `validateReferences()` 中校验 actor skill references
- [ ] 为 demo actors 配置初始技能列表
- [ ] 为 `BattleUnit` 增加 `skill_ids`
- [ ] 在 `buildBattleUnitsFromCatalog()` 中填充玩家 skill ids
- [ ] 在 `buildBattleUnitsFromCatalog()` 中从 enemy actions 提取并去重 skill ids
- [ ] 在 `BattleScene` 中保存 `rpg_catalog_` 非 owning 指针
- [ ] 确保 `rpg_catalog_` 声明顺序与构造函数初始化列表都在 `session_` 之前
- [ ] 实现 `SkillList` 的真实 `list_entries_` 构建
- [ ] `queueSkillAction()` 填充技能列表后直接 `setMenuState(MenuState::SkillList)`，不要调用会清空列表的 `enterListMenu(MenuState::SkillList)`
- [ ] 根据 MP 与 scope 计算技能 entry enabled
- [ ] 选择 enabled 技能时写入 `ActionDraft::selected_skill_id`
- [ ] 保持 Stage 2 不直接提交技能 `BattleAction`
- [ ] 如进入 `TargetSelect` 占位态，保持 `list_entries_` 不变，并同步修正 `menu_cancel` 只清除 target 字段后返回来源菜单
- [ ] 补充 catalog / unit factory / BattleScene smoke 测试

## 完成标准

- `Skill` 菜单不再固定显示空列表，而是展示当前行动者的真实技能候选
- 玩家单位技能来自 actor catalog 数据，敌人单位技能来自 enemy actions
- 技能 entry 至少显示名称和 MP 消耗
- MP 不足或 `Scope::None` 的技能不可确认
- 选择技能会写入动作草稿，但不会绕过 Stage 4 直接提交
- 不恢复旧的硬编码默认 `skill.attack` 提交路径
- `ninja -C build/debug tests/game_tests` 通过
- `ctest --test-dir build/debug --output-on-failure` 通过
