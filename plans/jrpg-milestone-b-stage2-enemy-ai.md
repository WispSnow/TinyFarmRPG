# Milestone B / Stage 2: 敌方 AI 最小行动规划细化计划

## Context

Stage 1 已经补齐了 Milestone B 的两个前置基础：

- `BattleUnit` 现在具备 `source_enemy_id / source_actor_id`
- 玩家金币真相已经落到 player entity 上

因此，Stage 2 可以正式开始处理“敌方自动行动”这条主线。

当前运行时代码里，敌方 AI 的阻塞点已经很明确：

### 1. `BattleScene` 仍把所有回合都当成“等待输入”

当前 `BattleScene` 的流程是：

- `init()` 在战斗仍为 `Ongoing` 时直接 `enterInputMenu()`
- `runStateMachine()` 的 `FlowState::NextTurn` 也固定走：
  - `state_ = FlowState::WaitingForInput`
  - `enterInputMenu()`

这意味着：

- 若首个行动者是敌人，战斗一开始也会进入玩家菜单
- 若轮到敌人行动，仍会先进入 `WaitingForInput`
- `WaitingForInput` 的语义当前并不纯粹，它实际变成了“等待任何阵营行动”

这与 Milestone B 的目标不一致。Stage 2 必须把 `WaitingForInput` 收敛为“只等待玩家输入”。

### 2. 当前没有独立的敌方行动规划器

`BattleSession` / `BattleActionResolver` 已经能稳定执行：

- `Attack`
- `Skill`
- `Guard`
- `Escape`
- `EndTurn`

所以 Stage 2 缺的不是“行动执行能力”，而是“在敌方回合自动构造一个合法 `BattleAction`”。

这部分如果直接塞进 `BattleScene` 的 UI 分支，会带来两个问题：

- 规则散落在场景层，不利于测试
- 后续若要增强 AI，会把表现层和规则层重新绑死

因此 Stage 2 应该新增独立 `BattleAiPlanner` helper，由它负责：

- 根据当前敌方单位与 `EnemyData::actions_` 选动作
- 根据 skill scope 选目标
- 在无有效技能时退回 `Attack / EndTurn`

### 3. planner 的数据边界需要显式化

Stage 1 已经明确：

- `BattleUnit.source_enemy_id` 可以作为调用侧查表入口
- 但 planner 本身不应该负责 `enemy id -> EnemyData` 查表

当前 `BattleScene` 已经持有：

- `rpg_catalog_`
- `session_`

因此更自然的边界是：

- `BattleScene` 先根据 `current actor + source_enemy_id` 解析 `const EnemyData*`
- 再把 `BattleUnit + EnemyData + 当前 units + RpgCatalog` 显式传给 planner

这样 planner 仍然是纯逻辑 helper，不需要依赖 `BattleScene`、`BattleSession` 或 catalog 生命周期管理。

### 4. `OneAlly` 目标选择不能依赖 resolver 默认行为

当前 `BattleActionResolver::collectTargets()` 的行为是：

- `Scope::OneEnemy` 未提供 target 时，会自动选第一个存活敌方
- `Scope::OneAlly` 未提供 target 时，会默认选 actor 自己

这对 resolver 是合理的，但对 AI 不够：

- 敌方治疗技能若不显式选目标，就会退化成“永远给自己放”
- Stage 2 需要明确 ally target 的最小优先级规则，而不是把选择交给 resolver 默认值

因此，planner 需要为 `OneAlly` 显式给出 `target_id`。

## 范围

### 本阶段包含

- 新增独立 `BattleAiPlanner`
- 明确 planner 输入边界与降级策略
- `BattleScene` 在首回合与 `NextTurn` 阶段区分玩家/敌方回合
- 敌方回合跳过 `WaitingForInput`，直接提交 AI 行动
- 明确各类 scope 的最小目标选择规则
- 明确 `Attack / EndTurn` fallback
- 对应的 planner 单元测试与 BattleScene smoke test

### 本阶段不包含

- 随机权重抽选
- 行为树、条件树、仇恨系统
- 敌方状态机脚本化
- 基于元素克制、状态收益、未来回合价值的复杂估值
- 战斗奖励、经验值、掉落写回
- 独立敌方思考动画或额外 battle log UI

## 实现思路

### 1. `BattleAiPlanner` 保持纯逻辑 helper

推荐新增：

- `src/game/battle/battle_ai_planner.h`
- `src/game/battle/battle_ai_planner.cpp`

推荐职责：

- 输入：
  - 当前行动敌人 `const BattleUnit& actor`
  - 已解析的 `const EnemyData& enemy`
  - 当前战斗单位视图 `const std::vector<BattleUnit>& units`
  - 显式技能查询依赖
    - Stage 2 首版可直接传 `const RpgCatalog&`
    - 若后续想进一步收窄依赖，也可以改成 `findSkill(skill_id)` 这类回调或轻量 resolver
- 输出：
  - 一个可直接提交给 `BattleSession::submitAction()` 的 `BattleAction`

推荐边界：

- planner 不持有状态
- planner 不做 `source_enemy_id -> EnemyData` 查表
- planner 不直接访问 `BattleSession`
- planner 不记录日志，日志由调用侧在降级分支输出
- planner 的外部依赖必须显式传入，不隐藏 catalog ownership 或全局单例

实现形式不必强行限制成 class：

- 可以是 `BattleAiPlanner` 类型上的静态函数
- 也可以是 `battle_ai_planner.*` 中的自由函数

只要满足“纯逻辑、好测试、无隐藏查表”即可。

### 2. `BattleScene` 需要抽出“开始当前回合” helper

当前 `init()` 和 `FlowState::NextTurn` 都各自手写了“进入输入菜单”的逻辑。

Stage 2 推荐新增一个共享 helper，语义类似：

- `beginCurrentTurnFlow()`
- 或 `enterCurrentTurnState()`

它的职责是：

1. 读取 `session_.currentActorId()`
2. 查到当前 `BattleUnit`
3. 若当前 actor 是玩家：
   - `state_ = FlowState::WaitingForInput`
   - `enterInputMenu()`
4. 若当前 actor 是敌人：
   - 规划 AI 行动
   - 直接进入 `ExecutingAction`

这样可以同时修掉两个入口点：

- 战斗初始化时，若首个行动者就是敌人，不会再进入玩家菜单
- `NextTurn` 也不会先进入 `WaitingForInput` 再绕过

这是 Stage 2 最关键的场景层改动。

这个 helper 还应承担一个额外职责：

- 当 `FlowState::ExecutingAction` 发现 `pending_action_` 意外为空时，不直接写死回到 `WaitingForInput`
- 而是重新走同一个“开始当前回合” helper，让当前 actor 按阵营重新分流

原因：

- Stage 2 之后，“回到当前回合入口”已经不再等价于“进入玩家菜单”
- 若边缘情况下敌方回合丢失了 `pending_action_`，直接 `WaitingForInput + enterInputMenu()` 会把敌人错误暴露给玩家菜单

### 3. `WaitingForInput` 语义正式收敛为“等待玩家输入”

Stage 2 之后，建议把状态语义固定为：

- `WaitingForInput`：只服务玩家可操作回合
- `ExecutingAction`：无论玩家还是敌方，真正提交行动都走这里
- `AnimatingResult / CheckVictory / NextTurn / BattleEnd`：继续保持现有职责

不推荐的实现方式：

- 让敌方先进入 `WaitingForInput`
- 再在 `refreshView()`、`prepareUi()` 或输入回调里发现“这是敌方”然后绕开菜单

原因：

- 会让状态语义变脏
- 更容易造成菜单焦点、`actions_enabled_`、Rml 显示状态的额外边缘问题
- 也不利于 smoke test 锁定行为

### 4. 调用侧负责 enemy 查表，planner 只接收显式参数

推荐在 `BattleScene` 的敌方回合 helper 中按以下顺序处理：

1. 获取当前 actor
2. 若 `actor.side != BattleSide::Enemy`，直接走玩家输入分支
3. 若 `rpg_catalog_ == nullptr`：
   - 记录 `warn`
   - 走 fallback
4. 若 `actor.source_enemy_id` 为空：
   - 记录 `warn`
   - 走 fallback
5. 若 `rpg_catalog_->findEnemy(*actor.source_enemy_id)` 失败：
   - 记录 `warn`
   - 走 fallback
6. 查表成功时，调用 planner 生成 `BattleAction`

这里建议继续保持“调用侧显式传依赖”的原则：

- `BattleScene` 负责 `EnemyData` 查表
- planner 只消费显式传入的 `EnemyData` 与 skill lookup 依赖
- Stage 2 首版若实现简单优先，直接传 `const RpgCatalog&` 也是可接受的

fallback 规则建议统一为：

- 优先尝试 `Attack`
- 若找不到合法攻击目标，再退回 `EndTurn`

原因：

- 这能覆盖预构建 battle units 的“无来源”合法输入
- 即使 catalog 缺失，也不会让战斗卡死
- `Attack` 是当前最接近“无脑敌人 AI”的保底方案

### 5. AI 动作选择采用“有效技能中按 rating 取最高”策略

Stage 2 推荐保持 deterministic，而不是一开始引入随机权重抽选。

推荐最小规则：

1. 遍历 `enemy.actions_`
2. 对每个 action：
   - 查 skill
   - 做可执行性过滤
   - 若可执行，生成具体 `BattleAction`
3. 在所有可执行 skill candidate 中：
   - 选择 `rating_` 最高者
   - 若 `rating_` 相同，保留 `enemy.actions_` 原始顺序中的更早项
4. 若没有任何有效 skill candidate：
   - fallback 到 `Attack`
   - 若 `Attack` 也没有合法目标，则 `EndTurn`

这里不建议在 Stage 2 引入：

- attack 与 skill 的复杂评分统一框架
- 随机 roll 选 action
- `rating_` 阈值窗口

原因：

- 当前目标是形成稳定、好测、可解释的最小 AI
- deterministic 规则更适合先把 BattleScene 接线做稳
- 后续若要贴近 RPG Maker 的权重抽选，再单独增强 planner 即可

### 6. skill 可执行性过滤要先于 rating 比较

推荐判定顺序：

1. `skill_id_` 非空
2. `findSkill(skill_id_)` 成功
3. `skill.scope_ != Scope::None`
4. `actor.mp >= skill.mp_cost_`
5. 当前 scope 存在可执行目标

只有同时满足以上条件，才把该 skill 纳入候选集。

这意味着以下 skill 在当前回合应被直接跳过：

- catalog 中不存在的技能
- `None` scope 的技能
- MP 不足的技能
- 目标集合为空的技能

### 7. 目标选择规则必须 deterministic

推荐统一目标优先级规则，避免 planner 在同一输入上产生不稳定结果。

#### `Scope::OneEnemy`

目标集合：

- 所有存活的对方单位

推荐选择：

1. `hp / max_hp` 比例最低
2. 若比例相同，当前 `hp` 更低
3. 若仍相同，`BattleUnitId` 更小

原因：

- 这是最小且可解释的“集火残血”策略
- 不依赖额外 threat 表或历史伤害记录

#### `Scope::AllEnemies`

只要至少存在一个存活对方单位，就视为可执行：

- `target_id = std::nullopt`

#### `Scope::OneAlly`

目标集合：

- 所有存活的同阵营单位

推荐选择规则分两档：

1. 若 skill 明显属于 HP/MP 恢复型：
   - 优先从“确实受损”的 ally 中选择
   - 仍按“最低资源比例 -> 更低绝对值 -> 更小 id”决胜
   - 若没有任何 ally 受损，则该 skill 本回合视为无效
2. 若 skill 不是明显恢复型：
   - 直接在所有存活 ally 中按“最低 HP 比例 -> 更低 HP -> 更小 id”选目标

这里的“明显恢复型”需要明确按 OR 逻辑识别，而不是要求 `damage_` 与 `effects_` 同时存在。

建议区分两个恢复维度：

- HP 恢复意图：
  - `skill.damage_.type == DamageType::HpRecover`
  - 或 `effects_` 中存在 `RecoverHp`
- MP 恢复意图：
  - `skill.damage_.type == DamageType::MpRecover`
  - 或 `effects_` 中存在 `RecoverMp`

这意味着：

- 纯公式恢复（`damage_.type == HpRecover/MpRecover`，`effects_` 为空）仍然算恢复型
- 纯 flat 恢复（`damage_.type == None`，但 `effects_` 有 `RecoverHp/RecoverMp`）也算恢复型

对恢复型 `OneAlly`，目标是否“确实受损”也必须按资源维度判断：

- HP 恢复只看 `hp < max_hp`
- MP 恢复只看 `mp < max_mp`
- 若同一个 skill 同时具备 HP 和 MP 恢复意图，只要任一资源存在缺口即可进入候选

排序时也应优先比较该 skill 真正会恢复的资源缺口比例，而不是一律看 HP。

Stage 2 不要求做更复杂的“状态收益”判断。

#### `Scope::AllAllies`

只要至少存在一个存活 ally，就可认为有候选。

但对明显恢复型技能建议增加一个最小 gating：

- HP 恢复型只看是否存在 `hp < max_hp` 的 ally
- MP 恢复型只看是否存在 `mp < max_mp` 的 ally
- 若 skill 同时恢复 HP 和 MP，则只要任一维度存在缺口，就可视为有效
- 只有相关恢复维度都不存在缺口时，才视为当前回合无效

否则：

- `target_id = std::nullopt`

#### `Scope::Self`

推荐规则：

- actor 存活时即可视为有目标
- `target_id = std::nullopt`

但若 skill 明显属于 HP/MP 恢复型，建议增加最小 gating：

- HP 恢复型只在 actor `hp < max_hp` 时有效
- MP 恢复型只在 actor `mp < max_mp` 时有效
- 若 skill 同时恢复 HP 和 MP，只要任一维度有缺口即可

这样可以避免敌人在满血满蓝时重复空放自疗。

### 8. `Attack` fallback 也要用同一套目标规则

当没有有效 skill candidate 时，enemy fallback 到 `Attack`。

这里推荐：

- 直接复用 `OneEnemy` 的目标选择规则
- 不依赖 resolver 的“第一个合法目标”默认值

原因：

- 这样 planner 的行为更一致
- 测试也更容易断言

若当前根本不存在合法攻击目标，则返回：

- `BattleActionType::EndTurn`

虽然在 `BattleOutcome::Ongoing` 下理论上不常见，但这是 planner 的最后兜底。

### 9. Stage 2 不修改 `BattleSession` 与 resolver 核心语义

当前 `BattleSession::submitAction()` 与 `BattleActionResolver` 已经稳定。

因此 Stage 2 不推荐修改：

- `BattleSession` 的公开接口
- `BattleActionResolver` 的校验/公式规则
- `TurnCore` 的回合推进语义

推荐做法是：

- 让 planner 生成“当前规则下已经尽量合法”的 `BattleAction`
- 仍由 resolver 作为最终校验者

这样可以保持 battle core 的边界稳定。

### 10. `prepareActionActor()` 不建议直接扩成“敌我通用回合入口”

当前 `prepareActionActor()` 明确要求：

- `state_ == FlowState::WaitingForInput`

它本质上是“给玩家输入菜单准备 actor”的 helper。

Stage 2 更推荐：

- 保留它继续服务玩家输入路径
- 另外新增一个不依赖 `WaitingForInput` 的 `currentActor()` 或等价 helper，供敌方自动回合使用

原因：

- 这样函数语义更清晰
- 避免 Stage 2 为了接 AI，把玩家输入 helper 改成含混的“什么状态都能拿 actor”

## 测试策略

Stage 2 至少建议补以下两层测试。

### 1. `tests/game/battle/battle_ai_planner_test.cpp`

建议新增 planner 专项测试，覆盖最关键决策：

- 最高 `rating_` 的可执行 skill 会被选中
- MP 不足时会跳过 skill，转而选择次优 skill 或 fallback attack
- `OneEnemy` 会选最低 HP 比例的玩家目标
- `OneAlly` 的治疗 skill 会选最受伤的敌方友军，而不是默认 self
- 纯 `damage_.type` 恢复 和 纯 `effects_` 恢复 都会被识别为恢复型
- `AllAllies` 或 `Self` 的恢复 skill 在全员满状态时会被判定为无效
- `HpRecover` 与 `MpRecover` 的 gating 会按对应资源维度分别判断，不混用 HP/MP 满状态
- 缺少有效 skill 时会 fallback 到 `Attack`
- 连 `Attack` 都没有目标时会 fallback 到 `EndTurn`

这组测试应尽量只测 planner，不依赖 `BattleScene`。

### 2. `tests/game/battle/battle_scene_smoke_test.cpp`

建议补 source-level smoke，锁定场景接线语义：

- `init()` 不再无条件 `enterInputMenu()`
- `FlowState::NextTurn` 不再固定 `WaitingForInput -> enterInputMenu()`
- `FlowState::ExecutingAction` 在 `pending_action_` 为空时不再硬编码回到玩家菜单
- 当前 actor 为敌方时会走 AI 规划并提交行动
- `WaitingForInput` 只服务玩家输入路径

原因：

- `BattleScene` 完整行为测试目前装配成本高
- 但 Stage 2 最容易回退的就是状态机分支语义，source smoke 足够当护栏
- 这里应尽量锁定“存在阵营分支”和“不会把敌方送进玩家菜单”这些语义，不要把测试绑死到具体 helper 名称

### 3. 现有 battle session / resolver 测试继续作为执行护栏

Stage 2 不需要重写这些测试：

- `tests/game/battle/battle_session_test.cpp`
- `tests/game/battle/battle_action_resolver_test.cpp`

它们已经覆盖：

- action 提交后的推进
- scope 校验
- MP 校验
- guard / escape / item 等基础规则

Stage 2 只需要保证 planner 生成的 action 与这些既有规则兼容。

## 需要新增或修改的文件

### 计划文档

- `plans/jrpg-milestone-b-stage2-enemy-ai.md`

### 推荐新增的代码文件

- `src/game/battle/battle_ai_planner.h`
- `src/game/battle/battle_ai_planner.cpp`

### 预计修改的代码文件

- `src/game/scene/battle_scene.h`
- `src/game/scene/battle_scene.cpp`

### 实现时需要注意的 include 边界

- `EnemyData` 与 `findEnemy()` 的使用尽量留在 `src/game/scene/battle_scene.cpp`
- `battle_scene.h` 不需要为了敌方回合 helper 额外暴露 `rpg_data.h`
- `src/game/scene/battle_scene.cpp` 实现 Stage 2 时大概率还需要新增 `battle_ai_planner.h`
- 若未来把 `EnemyData` 参与的 helper 提升到头文件签名，再单独评估前置声明或 include；Stage 2 首版不建议这样做

### 预计新增或补强的测试文件

- `tests/game/battle/battle_ai_planner_test.cpp`
- `tests/game/battle/battle_scene_smoke_test.cpp`

若当前测试目标不是自动 glob source，还需要同步更新对应的 CMake/测试清单。

## 实现步骤

### Step 1: 锁定 planner API 与 helper 粒度

先确定：

- planner 接收显式 `EnemyData`，不隐藏 catalog 查表
- planner 的技能查询依赖保持显式；首版可直接传 `const RpgCatalog&`，后续若需要再收窄为 skill lookup 回调
- 目标选择 helper 放在 planner 内部
- planner 输出直接可提交的 `BattleAction`

### Step 2: 实现 skill 候选过滤与 deterministic 选取

实现：

- rating 优先级
- MP gating
- scope gating
- 缺少 skill / target 时跳过

### Step 3: 实现最小目标选择规则

补齐：

- `OneEnemy`
- `OneAlly`
- `AllEnemies`
- `AllAllies`
- `Self`

并为恢复型 skill 增加最小“是否值得释放”判断。

### Step 4: 接入 `BattleScene` 的敌方回合分支

新增共享 turn-entry helper，并在：

- `init()`
- `FlowState::NextTurn`
- `FlowState::ExecutingAction` 的空 `pending_action_` 回退

两个入口统一使用。

玩家进入 `WaitingForInput`；
敌方直接规划并提交行动。

### Step 5: 补齐降级路径与日志

明确处理：

- 缺少 `rpg_catalog_`
- `source_enemy_id` 为空
- `findEnemy()` 失败

以上情况都不让战斗卡死，而是记录 `warn` 并走 `Attack / EndTurn`。

### Step 6: 补强测试

补 planner 单测；
补 BattleScene smoke；
确保 Stage 2 的状态机分支不会回退。

## 完成标准

- [ ] 敌方首回合不再进入玩家菜单
- [ ] `NextTurn` 时能正确按阵营切分玩家/敌方分支
- [ ] `ExecutingAction` 在 `pending_action_` 丢失时不会把敌方错误送回玩家菜单
- [ ] `WaitingForInput` 的语义收敛为“等待玩家输入”
- [ ] `BattleAiPlanner` 不负责 `enemy id -> catalog` 查表
- [ ] planner 能按 `rating_` 选择当前最优可执行 skill
- [ ] `OneEnemy / OneAlly / Self / AllEnemies / AllAllies` 都有明确且 deterministic 的目标规则
- [ ] 缺少来源或 catalog 时，敌方仍能通过 `Attack / EndTurn` 继续战斗
- [ ] 关键测试覆盖 planner 行为与 BattleScene 状态机接线

## 备注

当前额外设计结论：

- Stage 2 的关键不是“让敌人会放技能”，而是先把“敌方回合不再经过玩家菜单”这条状态机边界锁死
- planner 必须保持纯逻辑，避免把 `BattleScene`、`BattleSession` 或 catalog 生命周期管理耦合进去
- `OneAlly` 不应依赖 resolver 默认 self-target 语义，AI 必须显式选 ally
- 先做 deterministic 规则，比一开始做随机权重抽选更适合建立稳定回归
- 预构建 `BattleUnit` 缺少 `source_enemy_id` 仍然是合法输入，Stage 2 必须显式走降级分支

这样可以保证 Milestone B 在 Stage 2 结束后形成一个稳定的“敌方自动行动闭环”，并且为后续奖励结算继续保留清晰边界。
