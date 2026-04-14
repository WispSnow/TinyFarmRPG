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
- `InventoryMenuScene` / `SaveSlotSelectScene` 已演示了 `RmlDocumentController + RegisterStruct<T>() + RegisterArray<>() + data-for` 的 RmlUi 列表绑定模式

RmlUi 集成现状需要在本阶段直接遵守：

- 生产场景不再直接持有 `RmlDataBridge`，也不直接调用 `loadRmlDocument()` / `unloadAllRmlDocuments()`
- `BattleScene` 应继续通过 `engine::ui::rmlui::RmlDocumentController` 管理 data model、事件、文档加载、脏标记与卸载
- 当前 `BattleScene::initUI()` 已使用 `RmlDocumentController`，但尚未传入 `Rml::DataTypeRegister`；涉及列表绑定时，需要参考 `InventoryMenuScene` / `SaveSlotSelectScene`，为 `BattleScene` 新增 `Rml::DataTypeRegister` 与 `data_types_registered_`，并改为通过 `document_controller_.createModel(MODEL_NAME, &type_register_)` 创建模型
- 通过 `data-if` 隐藏包含 `data-for` 的面板时，不要在同一帧清空 backing vector；先切显隐并 mark dirty，避免 RmlUi 短暂访问旧 `data-for` 实例造成数组越界警告

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
  - 字段建议：`action_id / entry_index / label / enabled`

- `ListEntryViewModel`
  - 字段建议：`entry_index / entry_id / label / sublabel / enabled`
  - `Skill` 与 `Item` 共用，先不要拆成两套

- `TargetEntryViewModel`
  - 字段建议：`entry_index / unit_id / label / enabled / is_ally / is_dead`

绑定约束要写死：

- ViewModel 成员只使用 `Rml::String / int / float / bool`
- 不把 `std::optional`、复杂 enum、领域对象直接暴露给 RmlUi
- 若领域层使用 `BattleUnitId`、`std::string` 等类型，可在 ViewModel 层转成 `int` 或 `Rml::String`

绑定方式约定如下：

- `BattleScene` 持有 `RmlDocumentController document_controller_`
- `BattleScene` 持有 `Rml::DataTypeRegister type_register_` 与 `bool data_types_registered_`
- `document_controller_.attach(runtime, instanceId())`
- `document_controller_.createModel(MODEL_NAME, &type_register_)`
- `constructor.RegisterStruct<T>()` 注册字段
- `constructor.RegisterArray<decltype(vec_)>()` 注册数组
- `constructor.Bind("main_actions", &main_actions_)`
- `constructor.Bind("list_entries", &list_entries_)`
- `constructor.Bind("target_entries", &target_entries_)`
- `document_controller_.bindEvent(...)` / `bindSimpleEvent(...)` 绑定 RML data event
- `document_controller_.load(DOCUMENT_PATH)` 加载文档
- 通过 `document_controller_.markDirty(...)` / `markAllDirty()` 刷新绑定
- RML 中通过 `data-for="entry : list_entries"` 循环渲染

不再使用的旧路径：

- 不在 `BattleScene` 中直接持有 `RmlDataBridge`
- 不直接调用 `Scene::loadRmlDocument()` / `Scene::unloadAllRmlDocuments()`
- 不通过旧的 `data-command` 或自定义 event bridge 做生产 UI 事件

视觉选中态的第一版建议：

- 先依赖 `:focus` 伪类与程序化设置的导航焦点
- 不把 `selected` 作为 Stage 1 的强制字段
- 只有当后续确认需要“焦点”和“已选值”分离时，再补 `selected`

### 4. 菜单输入由 BattleScene 自主管理

这一点要在 Stage 1 就锁定。

推荐方案：

- 鼠标点击继续走 RML 的 `data-event-click`
- 键盘 / 手柄方向移动由 `BattleScene` 监听 `menu_up / menu_down / menu_left / menu_right`
- 键盘 / 手柄确认由 `BattleScene` 监听 `menu_confirm`
- `Cancel / Back` 统一走 `InputManager` 的 `menu_cancel`
- `BattleScene` 自己维护当前菜单光标，并程序化调用当前条目的 `Focus()`
- 不在 RML 里塞隐藏按钮，也不依赖 RmlUi 原生方向键导航作为 Stage 1 的主路径

原因：

- `Direction` / `Confirm` / `Cancel` 本质是场景输入动作，不是具体 UI 元素点击
- 当前 `InputContext::Battle` 已存在 `menu_up/down/left/right/confirm/cancel`
- `InputManager` 会在 `Battle` / `Menu` / `Dialogue` 这类菜单上下文中抑制 `menu_up/down/left/right/confirm/cancel` 对应的键盘事件转发给 RmlUi；即使 RML 元素有 `tab-index: auto` 与 `nav-*`，键盘方向导航也不会自然到达 RmlUi
- 这条路径与 `PauseMenuScene`、`InventoryMenuScene` 等场景更一致

本阶段锁定的实现路径是“自主管理光标”，而不是查询 RmlUi 当前焦点：

- `BattleScene` 持有每层菜单的光标索引，例如 `main_action_cursor_ / list_entry_cursor_ / target_entry_cursor_`
- 进入某个 `MenuState` 时，把对应光标 clamp 到第一个可用条目；空列表使用 `-1` 或 `std::optional<int>` 表示无可确认条目
- `menu_up/down/left/right` 根据当前 `MenuState` 移动对应光标，并跳过 disabled 条目
- 移动后通过稳定元素 id 程序化调用 `element->Focus()`，例如主菜单、列表、目标项都用 `entry_index` 或 `it_index` 生成可预测 id
- `menu_confirm` 根据当前 `MenuState` 和光标索引从 `main_actions_ / list_entries_ / target_entries_` 中读取条目并提交，不通过 `document()->GetFocusLeafNode()` 反推业务选择
- 鼠标点击条目的 `data-event-click` 也应进入同一套选择辅助函数，并同步更新对应光标，避免鼠标与键盘状态分叉
- `tf-nav-auto` / `nav.rcss` 仍需保留，用于让元素可聚焦、复用焦点样式，并给后续可能的原生导航路径留空间；但 Stage 1 不把 RmlUi 原生 `nav-*` 作为键盘导航依赖

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

导航基础也应在 Stage 1 一起补齐，而不是留到 Stage 5：

- `battle.rml` 引入 `../theme/nav.rcss`
- `body` 使用 `tf-screen-root tf-nav-root`
- 可聚焦条目使用 `tf-nav-auto`，或继续使用包含 `tab-index: auto` / `nav-*` 的共享按钮类
- 三层面板切换后，需要确保隐藏面板不会参与焦点
- 由于 `Battle` 上下文会抑制键盘导航事件转发给 RmlUi，`nav-*` 在 Stage 1 主要作为可聚焦与样式骨架；实际方向移动由 `BattleScene` 的 `menu_up/down/left/right` 光标逻辑驱动

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
- 若退出面板时需要替换 `list_entries_` / `target_entries_`，先隐藏 `data-if` 子树并 mark dirty，不要同帧清空数组

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

同时新增 `Rml::DataTypeRegister type_register_` 与 `data_types_registered_`，把当前 `BattleScene::initUI()` 的 `createModel(MODEL_NAME)` 改成 `createModel(MODEL_NAME, &type_register_)`，这是 Stage 1 新增列表绑定的必要改动。

### Step 3: 建立列表绑定骨架与输入捕获路径

为 `main_actions_ / list_entries_ / target_entries_` 建立 `RegisterStruct<T>()`、`RegisterArray<>()` 与 `Bind()` 接线；同时把 `menu_up / menu_down / menu_left / menu_right / menu_confirm / menu_cancel` 的场景输入处理接入 `BattleScene`。

本阶段选用自主管理光标方案：

- 不通过 `document()->GetFocusLeafNode()` 反推当前业务项
- `BattleScene` 维护每层菜单光标索引
- 方向动作移动索引并调用当前 RML 元素 `Focus()`
- 确认动作根据索引读取 ViewModel，再调用统一的选择/提交辅助函数
- 场景 `clean()` / 析构前断开所有 `menu_*` action listener，避免场景弹出后仍触发回调

### Step 4: 重构输入期菜单流转

将现有 `queueAttackAction / queueSkillAction / queueItemAction` 从“直接构造待提交动作”改为“先驱动菜单子状态流转”。

第一版允许：

- `Skill` / `Item` 必须进入各自空列表面板
- `Guard / Escape / EndTurn` 暂时保留直接提交
- `Attack` 可保留现有直达路径，或半步接入目标选择骨架，但不要求在本阶段完成真实目标候选生成

### Step 5: 调整 battle.rml/rcss 为三层骨架

把当前固定按钮区升级为主菜单 / 列表 / 目标三层容器；面板使用 `data-if` 互斥显示，并为空列表预留占位文案区域。

同时补齐 RmlUi 导航基础：

- 引入 `../theme/nav.rcss`
- `body` 增加 `tf-nav-root`
- 新增列表/目标条目使用 `tf-nav-auto` 或共享按钮类
- 验证隐藏面板不参与焦点
- 为 `data-for` 生成的条目设置稳定 id，方便 `BattleScene` 根据当前光标调用 `Focus()`

### Step 6: 补充结构性测试

至少补两类验证：

- `BattleScene` 仍保留现有顶层 `FlowState`
- `Skill` / `Item` 可进入空列表面板，并能通过 `menu_cancel` 返回 `MainMenu`
- `battle.rml` 已引入 `nav.rcss` 并使用 `tf-nav-root`
- `BattleScene` 使用 `RmlDocumentController + Rml::DataTypeRegister`，不回退到直接 `RmlDataBridge` 或 `loadRmlDocument()`
- `menu_up/down/left/right/confirm/cancel` 有明确的场景输入监听与断开路径
- `menu_confirm` 不依赖 `GetFocusLeafNode()` 反推选择，而是通过 `BattleScene` 自己维护的光标索引提交

## ToDo

- [x] 定义 `MenuState`，并明确其只在 `FlowState::WaitingForInput` 内流转
- [x] 定义动作草稿结构，并补上 `requires_target_selection`
- [x] 定义 `MainActionViewModel / ListEntryViewModel / TargetEntryViewModel`
- [x] 按当前 RmlUi 集成方式完成 `RmlDocumentController + Rml::DataTypeRegister + RegisterStruct<T>() + RegisterArray<>() + Bind()` 设计
- [x] 将当前 `BattleScene::initUI()` 的 `createModel(MODEL_NAME)` 改为 `createModel(MODEL_NAME, &type_register_)`
- [x] 为 `BattleScene` 增加 `menu_title / menu_hint / back_hint / main_actions / list_entries / target_entries` 等绑定字段
- [x] 为 `BattleScene` 增加主菜单 / 列表 / 目标选择的光标索引
- [x] 将 `menu_up/down/left/right/confirm/cancel` 明确接入 `BattleScene`，作为键盘/手柄移动、确认与返回路径
- [x] 鼠标 `data-event-click` 与键盘/手柄确认复用同一套选择辅助函数，并同步光标索引
- [x] 把 `Skill` / `Item` 从直接提交改成进入空列表占位态
- [x] 重构 `battle.rml/rcss` 为主菜单 / 列表 / 目标三层骨架，并用 `data-if` 控制显隐
- [x] 为 `battle.rml` 补齐 `nav.rcss` / `tf-nav-root` / 可聚焦条目的导航类和稳定元素 id
- [x] 避免隐藏 `data-if + data-for` 面板时同帧清空 backing vector
- [x] 补充场景级 smoke / 接线测试，验证进入列表与返回主菜单的闭环

## 备注

Stage 1 的完成标准是：

- `BattleScene` 顶层 `FlowState` 保持不变
- 新增 `MenuState`，且只在 `WaitingForInput` 内工作
- `BattleScene` data model 可以绑定至少一组列表型 ViewModel
- `Skill` / `Item` 进入各自空列表面板时，UI 可显示占位文案
- 鼠标点击走 `data-event-click`，键盘/手柄方向与确认走 `menu_up/down/left/right/confirm`
- `menu_cancel` 可以把 `SkillList / ItemList / TargetSelect` 正确退回上一层
- `battle.rml` 已具备共享导航主题与 `tf-nav-root`
- `BattleScene` 通过自维护光标索引确定当前选中项，并程序化同步 RmlUi 焦点
- 三个菜单面板互斥显示，隐藏面板不参与焦点
- 本阶段仍不要求真实技能、物品、目标数据接线
