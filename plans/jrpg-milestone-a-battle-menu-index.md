# Milestone A: 战斗菜单可玩化索引计划

## Context

当前战斗系统已经具备这些基础：

- `BattleScene` 已接入场景栈与 `InputContext::Battle`
- `BattleSession` / `BattleActionResolver` / `TurnCore` 已形成稳定的领域闭环
- `RpgCatalog` 已支持技能数据加载与查表
- 背包 / 快捷栏 / 存档系统已可提供后续物品接线基础
- RmlUi 生产界面已统一为 `RmlDocumentController` 管理文档、data model、事件绑定和脏标记

当前真正缺失的是战斗菜单这一层的“可玩化接线”：

- `Skill` / `Item` 按钮仍走硬编码默认值
- 玩家无法在战斗中选择具体技能
- 玩家无法在战斗中选择具体可用物品
- 目标选择仍是默认推断，不是显式交互

因此，Milestone A 只解决一件事：

- 把战斗菜单从“原型按钮”升级为“真实的 `Skill / Item / Target` 选择闭环”

## 范围

### 本阶段包含

- 战斗内主动作菜单状态机整理
- 技能列表的最小数据来源接线
- 物品列表的最小数据来源接线
- 目标选择 UI 与提交路径
- 键盘 / 手柄 / 鼠标下的战斗菜单交互闭环
- 与现有 `BattleSession` / `BattleActionResolver` 的接线
- 对应的回归测试

### 本阶段不包含

- 敌方 AI
- 战斗奖励、掉落、金币结算
- 任务推进
- 商店
- 完整角色养成、职业、装备系统
- 完整技能树 / 技能学习界面

## 实现思路

采用“按系统边界拆阶段”的方式推进，而不是按纯 UI 页面拆分。

原因：

- `BattleScene` 当前是主要编排点，先收敛状态机比先堆 UI 更稳
- 技能与物品的数据来源属于不同边界，后续需要分别细化
- 目标选择与动作提交天然是独立阶段，便于单独验证
- 这种拆法更适合后续为每个阶段单独写细化计划

整体策略：

1. 先重构 `BattleScene` 的菜单状态与 ViewModel
2. 再分别接入技能候选与物品候选
3. 再补目标选择与最终提交
4. 最后做 UI 收尾和测试补强

同时需要明确一个边界：

- 现有 `BattleScene` 的 6 段式战斗流程状态机保持不变
- 菜单层级状态只作为 `FlowState::WaitingForInput` 内部的子状态机
- 不把“主菜单 / 技能列表 / 物品列表 / 目标选择”提升为顶层战斗流程状态
- RmlUi 接线遵循当前 `RmlDocumentController + Rml::DataTypeRegister` 路径，不回退到直接持有 `RmlDataBridge` 或调用 `loadRmlDocument()` / `unloadAllRmlDocuments()`
- 键盘 / 手柄的 `menu_up/down/left/right/confirm/cancel` 作为场景输入动作处理；鼠标点击仍走 RML `data-event-click`
- 由于 `Battle` 上下文会抑制菜单导航键盘事件转发给 RmlUi，Stage 1 采用 `BattleScene` 自主管理光标并程序化 `Focus()` 的方案，不依赖 RmlUi 原生方向键导航

## 阶段索引

### Stage 1: BattleScene 菜单状态与 ViewModel 基础

目标：

- 让 `BattleScene` 从“固定六个按钮”扩展为“主菜单 + 子菜单 + 目标选择”的可编排状态

本阶段聚焦：

- 明确 `FlowState` 与 `MenuState` 的父子关系
- 定义菜单层级状态
- 定义技能/物品/目标候选的 ViewModel 结构与 RmlUi 列表绑定约束
- 定义当前动作选择上下文
- 明确 `Cancel / Back / Confirm` 的状态流
- 明确 `RmlDocumentController + Rml::DataTypeRegister` 的 data model 注册路径
- 明确 `battle.rml` 需要引入 `nav.rcss` 与 `tf-nav-root`
- 明确 `data-if + data-for` 面板隐藏时不能同帧清空 backing vector
- 明确 `menu_up/down/left/right/confirm` 的场景输入路径，避免只依赖被输入层抑制的键盘事件转发
- 明确当前选中项来源：`BattleScene` 维护菜单光标索引，方向动作移动索引并程序化 `Focus()`，确认动作根据索引读取 ViewModel
- 锁定 `menu_cancel` 作为 `Cancel / Back` 的主输入路径
- 定义空列表占位态，确保 Stage 1 结束时就有完整流转闭环

阶段交付物：

- 可扩展的战斗菜单状态机
- 遵循当前 RmlUi 集成方式的可绑定数据模型骨架
- 统一的鼠标点击与键盘/手柄光标输入流转规则
- `Skill / Item -> 空列表 -> Cancel 返回 MainMenu` 的最小可验证闭环

建议后续细化文档：

- `plans/jrpg-milestone-a-stage1-battle-menu-state.md`

### Stage 2: 技能候选数据接线

目标：

- 让战斗中的 `Skill` 菜单展示“当前行动者可用技能”，而不是硬编码默认技能

本阶段聚焦：

- 锁定“可用技能列表挂载点”
- 技能来源约定
- 技能候选过滤
- 技能可用性判定
- 技能菜单数据绑定

推荐最小方案：

- 在当前代码结构下，优先把技能列表直接挂到战斗单位模型或其构建输入上
- 不通过 `ActorData -> ClassData` 做深层间接查找
- 先为 `BattleUnit` 或 battle unit 构建输入补一个最小 `skill_ids` / learned-skill 列表桥接字段
- 玩家单位从“当前战斗单位的已学技能 / 预设技能列表”读取
- 敌人单位从 `EnemyData::actions_` 提取可用技能 id，先不实现 AI 权重选择
- 不在本阶段扩展完整场外技能管理界面

原因：

- 当前 `BattleUnit` 只有数值属性，没有技能列表桥接字段
- 敌人和玩家都需要统一的战斗内技能来源
- 直接挂到 battle unit 侧，能避免战斗层深度耦合 `RpgCatalog` 内部关系

阶段交付物：

- 战斗内真实技能列表
- 技能禁用/可用状态
- 选择技能后进入下一步选择流程

建议后续细化文档：

- `plans/jrpg-milestone-a-stage2-skill-source.md`

### Stage 3: 物品候选数据接线

目标：

- 让战斗中的 `Item` 菜单展示当前库存中“可在战斗中使用”的物品

本阶段聚焦：

- Inventory -> battle item list 的映射
- 可战斗使用物品的筛选规则
- 数量显示与空列表处理
- 物品选择后的动作上下文保存
- 锁定战斗物品目标规则与效果闭环

推荐最小方案：

- 只读取当前库存
- 只展示 `on_use_.has_value()` 的可用物品
- 物品候选需要额外给出“解析后的 battle scope”，而不是假定当前 `ItemData` 已自带 `scope`
- 不做“战斗外专用背包分页”或额外库存模型
- 明确战斗内 `item_stocks` 快照与真实背包之间的同步策略：若 Stage 3 允许消耗物品，就必须同时补剩余库存 / 物品变更的写回路径

本阶段必须同时覆盖：

- 让战斗内物品选择与实际效果执行形成闭环
- 明确消耗后的库存真相：可以选择战斗结束时通过 `BattleEndedEvent` 携带剩余 `item_stocks` / delta 让 `GameScene` 写回，或在战斗内通过库存领域服务即时写回；但不能只更新 `BattleRuntimeState` 后丢弃
- 若本阶段引入恢复类战斗物品，则需要同时扩展物品 use schema 与 resolver，至少补齐 `RecoverHp / RecoverMp`
- 若仍只支持当前 `AddItem` 语义，则战斗菜单必须先限制到现有可闭环的物品类型，避免出现“可选但无效果”的假入口

原因：

- 当前代码中的 `ItemData` 还没有独立 battle scope 字段，也没有完整的恢复类战斗效果执行
- 当前 `GameScene` 进入战斗时只把玩家背包聚合成 `BattleSessionOptions::item_stocks` 快照，resolver 消耗的是 `BattleRuntimeState::item_stocks`
- 当前 `BattleEndedEvent` 还没有携带物品库存结果，若不扩展事件或即时写回，战斗内消耗不会反映到真实背包
- 仅做物品列表展示而不补目标规则和效果执行，战斗内物品选择将无法形成闭环

阶段交付物：

- 战斗内真实物品列表
- 物品数量同步
- 空物品/不可用物品的 UI 反馈
- 战斗内物品使用效果正确生效

建议后续细化文档：

- `plans/jrpg-milestone-a-stage3-item-source.md`

### Stage 4: 目标选择与动作提交闭环

目标：

- 让 `Skill / Item / Attack` 最终都能通过显式目标选择进入统一提交路径

本阶段聚焦：

- 根据 scope 生成目标候选
- 单体 / 全体 / 自身 / 友方 / 敌方的 UI 行为
- 目标确认后的 `BattleAction` 构造
- `Back` 返回上一级菜单时的状态恢复

目标选择规则应明确为：

- `Scope::OneEnemy` / `Scope::OneAlly` 才进入显式目标选择
- `Scope::Self` / `Scope::AllEnemies` / `Scope::AllAllies` 直接确认提交
- `Attack` 按单体敌方动作处理
- 不为确定性 scope 强制弹出多余目标 UI

阶段交付物：

- 从技能/物品/攻击到目标确认的完整链路
- 与 `BattleSession::submitAction()` 的稳定接线
- 明确的失败反馈与取消流

建议后续细化文档：

- `plans/jrpg-milestone-a-stage4-target-selection.md`

### Stage 5: Battle RML/RCSS 收尾与测试补强

目标：

- 让整个战斗菜单在 UI 呈现、导航和测试层面达到“可持续迭代”的状态

本阶段聚焦：

- `battle.rml/rcss` 结构升级
- 键盘 / 手柄导航规则
- 选中态、禁用态、返回提示
- 领域测试与场景 smoke test 的补强

阶段交付物：

- 稳定的战斗菜单 UI 结构
- 菜单导航回归测试
- 技能 / 物品 / 目标选择关键路径的测试覆盖

建议后续细化文档：

- `plans/jrpg-milestone-a-stage5-ui-and-tests.md`

## 需要新增的文件

以下为推荐新增文件，是否最终拆分为独立文件，可在各阶段细化时再确认：

- `plans/jrpg-milestone-a-stage1-battle-menu-state.md`
- `plans/jrpg-milestone-a-stage2-skill-source.md`
- `plans/jrpg-milestone-a-stage3-item-source.md`
- `plans/jrpg-milestone-a-stage4-target-selection.md`
- `plans/jrpg-milestone-a-stage5-ui-and-tests.md`

若 `BattleScene` 内部状态继续膨胀，后续实现时可考虑新增：

- `src/game/scene/battle_menu_view_models.h`
- `src/game/scene/battle_menu_flow.h`
- `src/game/scene/battle_menu_flow.cpp`

但这不是当前索引文档必须立即锁定的结构。

## 实现步骤

### Step 1

完成 Stage 1 细化计划，先锁定 `BattleScene` 菜单状态机与数据模型边界。

### Step 2

完成 Stage 2 细化计划，确定技能候选来源与最小数据接线。

### Step 3

完成 Stage 3 细化计划，确定物品候选来源与库存映射规则。

### Step 4

完成 Stage 4 细化计划，锁定目标选择与统一动作提交链路。

### Step 5

完成 Stage 5 细化计划，统一战斗菜单 UI、导航与测试策略。

## ToDo

- [x] Stage 1: 细化 `BattleScene` 菜单状态与 ViewModel 方案 → `plans/jrpg-milestone-a-stage1-battle-menu-state.md`
- [x] Stage 2: 细化技能候选数据来源与过滤规则 → `plans/jrpg-milestone-a-stage2-skill-source.md`
- [x] Stage 3: 细化物品候选数据来源与库存映射规则 → `plans/jrpg-milestone-a-stage3-item-source.md`
- [x] Stage 4: 细化目标选择与动作提交流程 → `plans/jrpg-milestone-a-stage4-target-selection.md`
- [ ] Stage 5: 细化 Battle RML/RCSS 与测试补强方案

## 备注

本索引计划采用的推荐范围是：

- 只做 Milestone A 的“战斗菜单可玩化”
- 技能与物品只接最小可用数据源
- 不把 AI、奖励、任务、商店混入本阶段

当前额外设计结论：

- `End Turn` 暂保留，不在 Milestone A 主动移除
- 原因是本阶段目标是菜单可玩化闭环，不是战斗规则重构
- 后续若决定让 `Guard` 承担“防御并结束回合”的正式语义，再单独调整战斗动作集

这样可以保证 Milestone A 尽快形成一个可验证、可继续扩展的 JRPG 战斗菜单闭环。
