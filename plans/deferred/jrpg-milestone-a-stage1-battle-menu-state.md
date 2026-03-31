# Milestone A / Stage 1: BattleScene 菜单状态与 ViewModel 细化计划

## Context

当前 `BattleScene` 已有稳定的顶层战斗流程状态机：

- `WaitingForInput`
- `ExecutingAction`
- `AnimatingResult`
- `CheckVictory`
- `NextTurn`
- `BattleEnd`

当前战斗 UI 仍处于原型态：

- `battle.rml` 只有固定六个动作按钮
- `Skill` / `Item` 仍直接走硬编码默认值
- `BattleScene` 的 data model 只绑定了标量字段

同时，当前项目里已经有两类可直接复用的基础：

- `InputContext::Battle` 已包含 `menu_left / menu_right / menu_up / menu_down / menu_confirm / menu_cancel`
- `InventoryMenuScene` / `SaveSlotSelectScene` 已演示了 `RegisterStruct<T>() + RegisterArray<>() + data-for` 的 RmlUi 列表绑定模式

因此，Stage 1 的目标不是重写战斗流程，也不是接真实技能/物品数据，而是先把 `BattleScene` 改造成能承载“主菜单 -> 列表 -> 目标选择”层级的骨架。

## 范围

### 本阶段包含

- 明确 `FlowState` 与菜单子状态的分层关系
- 定义战斗菜单的最小 ViewModel 与绑定约束
- 定义动作草稿结构与菜单流转规则
- 明确 `Confirm / Cancel / Back` 的输入捕获路径
- 设计 `battle.rml/rcss` 的三层菜单骨架
- 为空列表占位态定义可验证的占位行为
- 补充与菜单状态结构对应的 smoke / 接线测试建议

### 本阶段不包含

- 真实技能数据来源实现
- 真实物品数据来源实现
- 真实目标候选生成规则实现
- `BattleActionResolver` 的物品效果扩展
- AI、掉落、奖励、任务推进

## 实现思路

### 1. 采用双层状态机，而不是改写顶层战斗 FSM

推荐方案：

- 顶层保留现有 `FlowState`
- 在 `FlowState::WaitingForInput` 内新增 `MenuState`

不采用“把 `MainMenu / SkillList / ItemList / TargetSelect` 提升为顶层 `FlowState`”的方案。

原因：

- 顶层 `FlowState` 负责战斗节拍、动作执行、结算与胜负推进
- 菜单层状态只负责输入期内部的 UI 流转
- 两者职责不同，强行合并会污染 `ExecutingAction / AnimatingResult / CheckVictory`

建议菜单层状态如下：

- `MenuState::None`
- `MenuState::MainMenu`
- `MenuState::SkillList`
- `MenuState::ItemList`
- `MenuState::TargetSelect`

其中：

- `MenuState::None` 只在不处于 `WaitingForInput` 时使用
- 进入 `WaitingForInput` 时切回 `MainMenu`
- 离开 `WaitingForInput` 时重置为 `None`

### 2. 用动作草稿承接菜单中的中间选择

建议在 `BattleScene` 中新增“动作草稿”结构，用于保存玩家在菜单中逐步确定的信息。

建议字段：

- `BattleActionType pending_type`
- `std::optional<std::string> selected_skill_id`
- `std::optional<std::string> selected_item_id`
- `std::optional<game::battle::BattleUnitId> selected_target_id`
- `bool requires_target_selection`

说明：

- Stage 1 不强制把 `actor_id` 存进草稿
- 当前输入期内行动者不会变化，正式 `BattleAction` 可在最终提交时直接读取 `session_.currentActorId()`
- 如果后续异步动画或多层确认导致“当前行动者可能漂移”，再把 `actor_id` 加回草稿即可

职责边界：

- 主菜单阶段决定 `pending_type`
- 技能/物品列表阶段补全 `selected_skill_id` / `selected_item_id`
- 目标选择阶段补全 `selected_target_id`
- 草稿满足提交条件时，再转换为正式 `BattleAction`

这样可以把“菜单中的中间状态”和“真正提交给 `BattleSession` 的动作”解耦。

### 3. ViewModel 需要按 RmlUi 列表绑定约束设计

Stage 1 的核心不是“先塞数据”，而是先把可绑定结构搭出来。这里需要明确 RmlUi 的约束，避免 Stage 2~4 再返工。

建议 ViewModel 分三类：

- `MainActionViewModel`
  - 字段建议：`action_id / label / enabled`

- `ListEntryViewModel`
  - 字段建议：`entry_id / label / sublabel / enabled`
  - `Skill` 与 `Item` 共用，先不要拆成两套

- `TargetEntryViewModel`
  - 字段建议：`unit_id / label / enabled / is_ally / is_dead`

绑定约束要写死：

- ViewModel 成员只使用 `Rml::String / int / float / bool`
- 不把 `std::optional`、复杂 enum、领域对象直接暴露给 RmlUi
- 若领域层使用 `BattleUnitId`、`std::string` 等类型，可在 ViewModel 层转成 `int` 或 `Rml::String`

绑定方式约定如下：

- `constructor.RegisterStruct<T>()` 注册字段
- `constructor.RegisterArray<decltype(vec_)>()` 注册数组
- `constructor.Bind("main_actions", &main_actions_)`
- `constructor.Bind("list_entries", &list_entries_)`
- `constructor.Bind("target_entries", &target_entries_)`
- RML 中通过 `data-for="entry : list_entries"` 循环渲染

视觉选中态的第一版建议：

- 先依赖 `:focus` 伪类与现有导航焦点
- 不把 `selected` 作为 Stage 1 的强制字段
- 只有当后续确认需要“焦点”和“已选值”分离时，再补 `selected`

### 4. Cancel / Back 应走输入动作，而不是隐藏按钮

这一点要在 Stage 1 就锁定。

推荐方案：

- `Confirm` 继续走现有按钮点击与 `menu_confirm`
- `Cancel / Back` 统一走 `InputManager` 的 `menu_cancel`
- 由 `BattleScene` 自己监听或轮询 `menu_cancel`，而不是在 RML 里塞隐藏按钮

原因：

- `Cancel` 本质是场景输入动作，不是具体 UI 元素点击
- 当前 `InputContext::Battle` 已存在 `menu_cancel`
- 这条路径与 `PauseMenuScene`、`InventoryMenuScene` 等场景更一致

状态回退规则：

- `TargetSelect` -> 返回来源菜单
- `SkillList` / `ItemList` -> 返回 `MainMenu`
- `MainMenu` -> 保持在当前战斗场景，不关闭战斗

补充：

- `TargetSelect` 的“来源菜单”由当前草稿动作类型决定
- 若来源是 `Attack`，则 `Cancel` 回 `MainMenu`
- 若来源是 `Skill`，则回 `SkillList`
- 若来源是 `Item`，则回 `ItemList`

### 5. `requires_target_selection` 的规则在 Stage 1 先统一

虽然真实技能/物品数据接线在后续阶段，但 Stage 1 先把动作类型到“是否需要目标选择”的规则锁定，避免后续理解不一致。

| 动作类型 | 是否需要目标选择 | Stage 1 约定 |
| --- | --- | --- |
| `Attack` | 是 | 固定按单体敌方处理 |
| `Skill` | 取决于 `scope` | `OneEnemy / OneAlly` 需要；`AllEnemies / AllAllies / Self` 直接提交 |
| `Item` | 取决于解析后的 battle scope | Stage 1 先保留 `requires_target_selection` 字段；Stage 3 由物品候选补出 `resolved_scope`，若当前 schema 仍无显式 scope，则先按 `Self` 兜底 |
| `Guard` | 否 | 直接提交 |
| `Escape` | 否 | 直接提交 |
| `EndTurn` | 否 | 直接提交 |

说明：

- 这里的 `Item` 先使用“解析后的 battle scope”表述，而不是假定当前 `ItemData` 已自带 `scope`
- 这样文档能和当前代码保持一致，也给 Stage 3 保留“扩展 item use schema”或“建立最小映射层”两种实现空间

### 6. RML 结构用 `data-if`，三层面板互斥显示

Stage 1 不追求最终视觉稿，但要先把结构改成能支持后续迭代的骨架。

推荐结构：

- `#battle-main-menu`
- `#battle-list-menu`
- `#battle-target-menu`
- `#battle-menu-title`
- `#battle-menu-hint`
- `#battle-back-hint`

显示策略：

- 同屏只显示一层主交互面板
- 三个面板用 `data-if` 控制显隐，而不是只靠 class 切换
- 这样隐藏面板不会继续参与导航焦点，也更符合“菜单层互斥”的语义

Stage 1 的占位行为也应明确：

- 点击 `Skill` -> 进入 `SkillList`
- `SkillList` 暂时显示空列表与 `"No skills available"`
- 点击 `Item` -> 进入 `ItemList`
- `ItemList` 暂时显示空列表与 `"No items available"`
- 两者都可以通过 `menu_cancel` 返回 `MainMenu`

这样即使还没接真实数据，Stage 1 结束时也有完整可验证的状态流转闭环。

### 7. 代码组织先收敛在 BattleScene 内部

本阶段优先采用“在 `BattleScene` 内部新增结构与私有辅助函数”的方案。

暂不建议一开始就拆独立 `battle_menu_flow.*`，原因：

- Stage 1 仍在收敛状态边界
- Stage 2~4 大概率还会调整 ViewModel 与草稿结构
- 过早拆文件会增加搬运成本

若到 Stage 4 后结构稳定，再考虑拆出：

- `battle_menu_view_models.h`
- `battle_menu_flow.h/.cpp`

## 需要新增的文件

本阶段计划文档：

- `plans/jrpg-milestone-a-stage1-battle-menu-state.md`

本阶段实现时，预计不强制新增代码文件。

可选新增：

- `tests/game/scene/battle_scene_menu_smoke_test.cpp`

如果后续决定直接扩展现有 smoke test，也可以不单独拆测试文件。

## 实现步骤

### Step 1: 明确双层状态机边界

在 `BattleScene` 中保留现有 `FlowState`，新增 `MenuState`，并明确其只在 `FlowState::WaitingForInput` 内流转。

### Step 2: 定义动作草稿与最小 ViewModel

新增动作草稿结构、主菜单/列表/目标三类 ViewModel，以及菜单标题、提示、返回提示等绑定字段。

### Step 3: 建立列表绑定骨架与输入捕获路径

为 `main_actions_ / list_entries_ / target_entries_` 建立 `RegisterStruct<T>()`、`RegisterArray<>()` 与 `Bind()` 接线；同时把 `menu_cancel` 的场景输入处理接入 `BattleScene`。

### Step 4: 重构输入期菜单流转

将现有 `queueAttackAction / queueSkillAction / queueItemAction` 从“直接构造待提交动作”改为“先驱动菜单子状态流转”。

第一版允许：

- `Skill` / `Item` 必须进入各自空列表面板
- `Guard / Escape / EndTurn` 暂时保留直接提交
- `Attack` 可保留现有直达路径，或半步接入目标选择骨架，但不要求在本阶段完成真实目标候选生成

### Step 5: 调整 battle.rml/rcss 为三层骨架

把当前固定按钮区升级为主菜单 / 列表 / 目标三层容器；面板使用 `data-if` 互斥显示，并为空列表预留占位文案区域。

### Step 6: 补充结构性测试

至少补两类验证：

- `BattleScene` 仍保留现有顶层 `FlowState`
- `Skill` / `Item` 可进入空列表面板，并能通过 `menu_cancel` 返回 `MainMenu`

## ToDo

- [ ] 定义 `MenuState`，并明确其只在 `FlowState::WaitingForInput` 内流转
- [ ] 定义动作草稿结构，并补上 `requires_target_selection`
- [ ] 定义 `MainActionViewModel / ListEntryViewModel / TargetEntryViewModel`
- [ ] 按 RmlUi 约束完成 `RegisterStruct<T>() + RegisterArray<>() + Bind()` 设计
- [ ] 为 `BattleScene` 增加 `menu_title / menu_hint / back_hint / main_actions / list_entries / target_entries` 等绑定字段
- [ ] 将 `menu_cancel` 明确接入 `BattleScene`，作为唯一的 `Cancel / Back` 输入路径
- [ ] 把 `Skill` / `Item` 从直接提交改成进入空列表占位态
- [ ] 重构 `battle.rml/rcss` 为主菜单 / 列表 / 目标三层骨架，并用 `data-if` 控制显隐
- [ ] 补充场景级 smoke / 接线测试，验证进入列表与返回主菜单的闭环

## 备注

Stage 1 的完成标准是：

- `BattleScene` 顶层 `FlowState` 保持不变
- 新增 `MenuState`，且只在 `WaitingForInput` 内工作
- `BattleScene` data model 可以绑定至少一组列表型 ViewModel
- `Skill` / `Item` 进入各自空列表面板时，UI 可显示占位文案
- `menu_cancel` 可以把 `SkillList / ItemList / TargetSelect` 正确退回上一层
- 三个菜单面板互斥显示，隐藏面板不参与导航焦点
- 本阶段仍不要求真实技能、物品、目标数据接线
