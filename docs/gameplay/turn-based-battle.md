# JRPG 回合制战斗系统

## 概述

回合制战斗系统会把游戏从“实时探索”切换到“策略回合”模式，玩家与敌方单位按速度顺序交替行动，直到一方全灭或玩家成功逃跑。当前实现已经不再是最初的 Attack 原型，而是具备完整战斗菜单闭环的最小 JRPG 战斗骨架：

- `BattleScene` 负责 RmlUi 菜单、输入和表现层状态机
- `BattleAiPlanner` 负责敌方回合的最小自动行动规划
- `BattleSession` 负责接收行动并返回全量结果快照
- `BattleActionResolver` 负责技能、物品、防御、逃跑等具体结算
- `BattleRewardResolver` 负责胜利后的金币、掉落与经验汇总
- `TurnCore` 负责行动顺序推进与胜负判定
- `GameScene` 负责战斗入口、场景 push/pop、战斗库存写回与胜利奖励落地

核心设计原则：

- **领域逻辑与表现分离**：`TurnCore` / `BattleSession` / `BattleActionResolver` 不依赖 ECS UI。
- **场景栈切换**：战斗通过 push/pop 叠加在探索场景之上，结束后直接恢复探索。
- **子状态机驱动菜单**：`BattleScene` 在 `FlowState::WaitingForInput` 内部再维护 `MenuState`。
- **目录驱动动作**：技能从 `RpgCatalog` 读取，战斗物品从 `ItemCatalog::battle_use` 读取。

## 架构分层

```mermaid
graph TD
    subgraph "表现层 — BattleScene"
        UI["RmlUi 菜单与结果文本"]
        FSM["FlowState + MenuState"]
        INPUT["menu_up/down/confirm/cancel"]
        AI["BattleAiPlanner"]
    end

    subgraph "应用层 — BattleSession"
        SESSION["submitAction()"]
        STOCKS["battle item_stocks 运行时副本"]
    end

    subgraph "执行层 — BattleActionResolver"
        RESOLVE["Attack / Skill / Item / Guard / Escape / EndTurn"]
        CATALOG["RpgCatalog / ItemCatalog"]
    end

    subgraph "奖励层 — BattleRewardResolver"
        REWARD["gold / drops / exp summary"]
    end

    subgraph "领域核心 — TurnCore"
        ORDER["速度排序"]
        ADV["advanceTurn()"]
        OUTCOME["evaluateOutcome()"]
    end

    subgraph "探索侧 — GameScene"
        PUSH["requestPushScene(BattleScene)"]
        WRITEBACK["BattleEndedEvent + 库存/奖励写回"]
        WALLET["PlayerWalletComponent"]
    end

    INPUT --> FSM
    UI --> FSM
    AI --> FSM
    FSM --> SESSION
    SESSION --> STOCKS
    SESSION --> RESOLVE
    RESOLVE --> CATALOG
    RESOLVE --> ORDER
    RESOLVE --> ADV
    RESOLVE --> OUTCOME
    WRITEBACK --> REWARD
    WRITEBACK --> WALLET
    PUSH --> WRITEBACK
```

关键边界：

- `BattleScene` 不直接操作 `TurnCore`
- `BattleSession` 是表现层进入战斗逻辑的唯一入口
- `BattleActionResolver` 是动作规则真相来源，UI 只做前置筛选
- `BattleAiPlanner` 只生成敌方行动，不直接修改回合状态
- `BattleRewardResolver` 只生成奖励摘要，不直接写探索态真相
- `GameScene` 保持对真实背包、金币和探索流程的所有权

## 回合驱动原理

### 速度排序

`TurnCore` 在战斗开始时按 `speed` 降序稳定排序，速度高者先行动；同速时保持原始加入顺序。

### 行动推进与死亡跳过

`advanceTurn()` 会在行动队列中循环推进，并自动跳过 `hp <= 0` 的单位。若一整圈都没有存活单位，战斗会进入终局判定。

### 胜负结果

当前结果枚举为：

| 结果 | 含义 |
|---|---|
| `Ongoing` | 战斗继续 |
| `Victory` | 玩家方胜利 |
| `Defeat` | 玩家方失败 |
| `Escaped` | 玩家方成功逃跑 |

## BattleScene 菜单与输入

当前 `BattleScene` 不再是“两个按钮直接提交”的原型，而是如下结构：

### 顶层状态机

```mermaid
stateDiagram-v2
    [*] --> NextTurn
    NextTurn --> WaitingForInput : 当前行动者是玩家
    NextTurn --> ExecutingAction : 当前行动者是敌人
    WaitingForInput --> ExecutingAction : 提交 BattleAction
    ExecutingAction --> AnimatingResult : session_.submitAction()
    AnimatingResult --> CheckVictory : 0.2s 占位计时结束
    CheckVictory --> NextTurn : outcome == Ongoing
    CheckVictory --> BattleEnd : Victory / Defeat / Escaped
    BattleEnd --> [*]
```

### 输入期子菜单状态

`FlowState::WaitingForInput` 内再维护 `MenuState`：

- `MainMenu`
- `SkillList`
- `ItemList`
- `TargetSelect`

说明：

- `WaitingForInput` 的语义已经收紧为“等待玩家输入”
- 敌方回合不会进入玩家菜单，而是在 `NextTurn` 阶段直接通过 `BattleAiPlanner` 生成并提交动作

### 输入路径

当前 Battle 菜单采用“场景级输入 + RML 点击”的双路径：

- 鼠标点击：走 RML `data-event-click`
- 键盘 / 手柄：走 `menu_up/down/left/right/confirm/cancel`
- `BattleScene` 自己维护菜单光标，并程序化 `Focus(true)` 同步 RmlUi 焦点

这点很重要：Battle 输入上下文会抑制原生菜单方向键事件转发给 RmlUi，所以当前实现**不依赖** RmlUi 原生方向键导航。

### 主菜单动作集

当前主菜单包含：

- `Attack`
- `Skill`
- `Item`
- `Guard`
- `Escape`
- `End Turn`

### 技能 / 物品 / 目标流

```mermaid
flowchart TD
    A["MainMenu"] --> B["Attack"]
    A --> C["SkillList"]
    A --> D["ItemList"]
    A --> E["Guard / Escape / End Turn"]

    B --> T["TargetSelect (OneEnemy)"]
    C -->|"OneEnemy / OneAlly"| T
    D -->|"OneEnemy / OneAlly"| T
    C -->|"Self / AllEnemies / AllAllies"| S["submitDraftAction()"]
    D -->|"Self / AllEnemies / AllAllies"| S
    E --> S
    T --> S
```

### 候选来源

| 菜单 | 当前来源 |
|---|---|
| `SkillList` | `BattleUnit::skill_ids` + `RpgCatalog` |
| `ItemList` | `BattleSession::itemStocks()` + `ItemCatalog::battle_use_` |
| `TargetSelect` | 当前 actor side + `session_.units()` + action scope |

### Scope 规则

| 动作 | scope 处理 |
|---|---|
| `Attack` | 固定为 `OneEnemy`，必须显式选目标 |
| `Skill` / `Item` `OneEnemy` / `OneAlly` | 进入 `TargetSelect` |
| `Skill` / `Item` `Self` / `AllEnemies` / `AllAllies` | 不弹目标菜单，直接提交 |
| `Scope::None` | 在列表里 disabled，不应进入提交路径 |

### Cancel / Back 规则

| 当前菜单 | `menu_cancel` 行为 |
|---|---|
| `TargetSelect` | 回到动作来源菜单，并保留技能/物品列表 |
| `SkillList` / `ItemList` | 回到 `MainMenu` |
| `MainMenu` | 吃掉输入，不退出战斗 |

## 敌方 AI 行动规划

敌方回合由 `BattleAiPlanner` 完成，`BattleScene` 在 `FlowState::NextTurn` 阶段根据当前行动者阵营分支：玩家方进入 `WaitingForInput`，敌方直接调用 planner 并将结果送入提交流程。

### 选技算法

`planEnemyAction()` 遍历 `EnemyData::actions_` 选出最高 `rating_` 且当前可执行的技能：

1. 跳过 `skill_id` 为空或 `RpgCatalog` 中不存在的条目
2. 跳过 MP 不足的技能
3. 针对该技能的 scope 生成目标，若没有合法目标则跳过
4. 在所有可生成动作的候选中取 `rating_` 最高者
5. 若无任何可用技能，调用 `planFallbackAction()`：Attack 指向最低 HP% 存活对手，若无存活对手则 EndTurn

### 目标选择规则（按 scope）

| scope | 目标策略 |
|---|---|
| `OneEnemy` | 对手中 HP 百分比最低的存活单位；同比例时优先绝对值更低者，再按 id 稳定 |
| `AllEnemies` | 无需选单体，要求对手侧至少有存活单位 |
| `OneAlly` | 恢复类技能选"资源缺口最大的友军"；其他技能选 HP% 最低的友军 |
| `AllAllies` | 恢复类技能额外要求友军侧至少有资源缺口，避免对满血/满 MP 全体浪费 |
| `Self` | 恢复类技能要求行动者自身存在资源缺口，否则跳过 |

### 恢复意图检测

AI 在选 `OneAlly` / `AllAllies` / `Self` 技能时会通过 `detectRecoveryIntent()` 检查技能是否具有 HP/MP 恢复效果（`DamageType::HpRecover` / `MpRecover`，或 `effects_` 中含 `RecoverHp` / `RecoverMp`），确保恢复技能不浪费在满血/满 MP 目标上。

## 动作执行链路

当前动作提交流程如下：

```mermaid
sequenceDiagram
    participant BS as BattleScene
    participant SE as BattleSession
    participant AR as BattleActionResolver
    participant TC as TurnCore

    BS->>BS: queue / select action
    BS->>BS: submitDraftAction()
    BS->>SE: submitAction(BattleAction)
    SE->>AR: resolve(action, turn_core, runtime_state)
    AR->>AR: 校验 actor / target / skill / item / stock / mp
    AR->>TC: 应用伤害 / 恢复 / 状态 / 防御 / 逃跑
    AR->>TC: refresh() / advanceTurn()
    SE->>SE: fillSnapshot(result)
    SE-->>BS: BattleActionResult + BattleSnapshot
    BS->>BS: refreshView() / result_text
```

当前支持的行动类型：

- `Attack`
- `Skill`
- `Item`
- `Guard`
- `Escape`
- `EndTurn`

### UI 与执行层的分工

`BattleScene` 会尽量在 UI 层阻止无效选择，但 `BattleActionResolver` 仍保留最终保护：

- `OneEnemy` / `OneAlly` 在脚本或测试直接提交时，resolver 仍会校验 target
- `Skill` / `Item` 在 `OneEnemy` / `OneAlly` scope 下若缺少 target，resolver 仍保留 fallback 语义
- `Attack` 仍要求显式 target；缺少 target 会直接 rejected
- 若 MP、库存、目标状态在提交瞬间失效，resolver 会拒绝并返回失败原因

### 公式与效果来源

`BattleActionResolver` 当前已经是数据驱动结算，而不是纯硬编码分支：

- 普通攻击也会走 `BattleFormulaEvaluator`，当前默认公式是 `a.atk`；若 Lua 公式求值失败，则回退到 `actor.attack`
- Skill 主效果由 `SkillData::damage_` 驱动，支持 `HpDamage / MpDamage / HpRecover / MpRecover / HpDrain / MpDrain / None`
- Skill 附加效果当前支持 `RecoverHp / RecoverMp / AddState / RemoveState`
- Battle item 目前只支持 `battle_use.effects` 中的 `RecoverHp / RecoverMp`；没有 `battle_use` 或 `scope == None` 的物品会在列表中被禁用或在 resolver 中被拒绝

### 结果反馈

当前 `BattleScene::refreshView()` 会根据 `BattleActionResult` 生成最小可读文案：

- Attack：伤害与 KO
- Skill：miss / damage / HP 恢复 / MP 恢复 / 首个 added state
- Item：HP 恢复 / MP 恢复，或 `"Item used"`
- Escape：成功 / 失败
- Guard / EndTurn：短文本反馈

### 战斗结算通知反馈

战斗结束后，`game_scene_reward_feedback` 模块负责将写回结果格式化为可见文本：

- `formatRewardFeedback()`：将金币与掉落写回结果转成单段文本（逐条列出掉落物、金币；如有 `rejected` 量则追加提示）
- `formatBattleSettlementFeedback()`：将奖励写回结果与任务推进摘要（`QuestBattleProgressSummary`）合并为一条完整的战斗结算通知文本，通过现有 `DialogueShowEvent` 渠道显示给玩家
- 若本场 Victory 没有金币、掉落或任务推进，反馈会回退为单行 `战斗胜利`

## 战斗库存与结算协议

### 战斗物品库存

战斗物品不是直接读写真实背包，而是“进入战斗复制，战斗结束写回”：

```mermaid
sequenceDiagram
    participant GS as GameScene
    participant BS as BattleScene
    participant SE as BattleSession
    participant EVT as BattleEndedEvent

    GS->>GS: collectPlayerItemStocks()
    GS->>BS: BattleSessionOptions.item_stocks
    BS->>SE: 运行时消耗 item_stocks
    SE-->>BS: itemStocks() 剩余库存
    BS->>EVT: remaining_item_stocks
    EVT->>GS: onBattleEnded()
    GS->>GS: 写回 battle item delta
```

这保证了：

- 战斗层不直接依赖探索库存组件
- 物品消耗不会在战斗结束后丢失
- `GameScene` 仍保有真实背包同步的最终控制权

### Victory 奖励写回

标准奖励只在 `Victory` 时生效：

```mermaid
sequenceDiagram
    participant BS as BattleScene
    participant EVT as BattleEndedEvent
    participant GS as GameScene
    participant RR as BattleRewardResolver
    participant INV as InventoryDomainService
    participant WALLET as PlayerWalletComponent

    BS->>EVT: BattleEndedEvent{outcome, final_units, remaining_item_stocks}
    EVT->>GS: onBattleEnded()
    GS->>GS: 先写回 battle item delta
    alt outcome == Victory
        GS->>RR: resolve(final_units, rpg_catalog)
        RR-->>GS: gold_total / item_drops / exp_total
        GS->>WALLET: gold += gold_total
        GS->>INV: addItem(item_drops)
        GS->>GS: 通过 DialogueShowEvent 显示最小奖励反馈
    else outcome == Defeat / Escaped
        GS->>GS: 不发标准奖励
    end
```

当前规则：

- `Victory`：写回金币与掉落，并显示最小奖励反馈
- `Defeat`：不发金币/掉落/经验，但保留战斗中已发生的物品消耗
- `Escaped`：不发金币/掉落/经验，但同样保留战斗中已发生的物品消耗
- 金币真相位于 player entity 的 `PlayerWalletComponent`

## 数据结构

### 核心类型

| 类型 | 当前关键字段 |
|---|---|
| `BattleUnit` | `id / name / side / hp / max_hp / mp / max_mp / attack / defense / magic_attack / magic_defense / speed / luck / skill_ids / source_actor_id / source_enemy_id` |
| `BattleAction` | `type / actor_id / target_id / skill_id / item_id` |
| `BattleActionResult` | `status / action_type / damage / hp_recovered / mp_recovered / mp_spent / missed / critical / target_guarded / target_defeated / escape_succeeded / states_added / states_removed / failure_reason / outcome_after / snapshot` |
| `BattleSnapshot` | `units / current_actor_id / round_index / outcome` |
| `BattleSessionOptions` | `rpg_catalog / item_catalog / item_stocks` |

### 关键辅助类型

| 类型 | 当前职责 |
|---|---|
| `BattleRuntimeState` | 单次 `BattleSession` 拥有的运行时可变状态：`item_stocks`、单位防御/状态剩余回合、逃跑尝试计数。刻意与快照类型分离，不出现在命令/事件负载中 |
| `BattleRewardItemDrop` | 奖励掉落聚合条目：`item_id`（原始字符串 id）、`item_id_hash`（库存写回用稳定 hash）、`count`（聚合数量） |
| `BattleRewardSummary` | 胜利后的 `gold_total / exp_total / item_drops` 聚合结果；`empty()` 可快速判断是否有奖励 |
| `BattleRewardWritebackItemResult` | 单个掉落条目的实际写回结果：原始 `drop`、`accepted`（成功入包数量）、`rejected`（背包满等原因拒绝数量） |
| `BattleRewardWritebackResult` | 完整写回摘要：`gold_written_back` + `item_results` 列表；`empty()` 可判断是否有任何写回 |
| `PlayerWalletComponent` | 探索态金币真相 |

### 命令与事件

| 契约 | 当前形态 |
|---|---|
| `EnterBattleCommand` | 可携带 `actor_ids / troop_id`，也可直接携带预构建 `player_units / enemy_units` |
| `BattleEndedEvent` | `outcome / final_units / remaining_item_stocks` |
| `SubmitBattleActionCommand` | 目前保留为通用契约类型；当前 `BattleScene` 自己直接调 `BattleSession::submitAction()`，不经 dispatcher |

## 完整战斗流程

以当前测试战斗入口为例：

```mermaid
sequenceDiagram
    participant DBG as Debug 面板
    participant D as Dispatcher
    participant GS as GameScene
    participant BUF as BattleUnitFactory
    participant BS as BattleScene
    participant SE as BattleSession

    DBG->>D: EnterBattleCommand{}
    D->>GS: onEnterBattleCommand()
    GS->>GS: collectPlayerItemStocks()
    GS->>BUF: buildBattleUnitsFromCatalog(...)
    GS->>GS: requestPushScene(BattleScene)

    loop 每个行动者回合
        alt 当前行动者是玩家
            BS->>BS: MainMenu / SkillList / ItemList / TargetSelect
            BS->>SE: submitAction(BattleAction)
        else 当前行动者是敌人
            BS->>BS: BattleAiPlanner 生成动作
            BS->>SE: submitAction(BattleAction)
        end
        SE-->>BS: BattleActionResult + Snapshot
        BS->>BS: AnimatingResult(0.2s)
    end

    BS->>D: BattleEndedEvent{outcome, final_units, remaining_item_stocks}
    BS->>GS: requestPopScene()
    D->>GS: onBattleEnded()
    GS->>GS: 写回 battle item delta
    GS->>GS: Victory 时写回金币/掉落
```

## 扩展指南

| 方向 | 当前状态 | 后续扩展方式 |
|---|---|---|
| 技能系统 | 已接入目录与 scope | 扩展学习/成长/技能详情 UI |
| 战斗物品 | 已接入 `battle_use` 与库存写回 | 扩展状态类、伤害类 battle item 效果 |
| 状态异常 | 已有 runtime state 与 added state 基础 | 扩展回合开始/结束 hook 与更多状态表现 |
| AI | 已有最小敌方自动行动 | 继续扩展评分规则、目标偏好、条件分支 |
| 全体 / 自身动作 | 已通过 scope 直接提交支持 | 后续可补更丰富的结果展示 |
| 目标 UI | 已支持单体敌/友选择 | 后续可补头像、弱点、预览、复活目标规则 |
| 战斗日志 | 当前只有简短 `result_text` | 后续可拆独立 log/popup 系统 |
| 奖励结算 | 已完成 Victory 金币/掉落写回 | 后续可扩经验消费方、任务推进、独立结算界面 |

## 测试策略

当前测试已经覆盖到“领域规则 + 场景接线 + RML/RCSS 静态约束”三层：

| 测试文件 | 覆盖内容 |
|---|---|
| `tests/game/battle/turn_core_test.cpp` | 速度排序、死亡跳过、胜负判定 |
| `tests/game/battle/battle_action_resolver_test.cpp` | Attack / Skill / Item / Guard / Escape 的规则与 scope |
| `tests/game/battle/battle_unit_factory_test.cpp` | `BattleUnit` 来源信息与 catalog 构建路径 |
| `tests/game/battle/battle_ai_planner_test.cpp` | 敌方最小 AI 选技与目标选择 |
| `tests/game/battle/battle_reward_resolver_test.cpp` | Victory 奖励汇总、掉落合并、非 Victory 空摘要 |
| `tests/game/battle/battle_session_test.cpp` | 会话级提交、快照、回合推进 |
| `tests/game/battle/battle_scene_smoke_test.cpp` | `BattleScene` 状态机、菜单接线、RML/RCSS 关键绑定 |
| `tests/game/game_scene_battle_entry_test.cpp` | `EnterBattleCommand` 入口、push、catalog fallback |
| `tests/game/game_scene_battle_reward_writeback_test.cpp` | `Victory / Defeat / Escaped` 的库存与奖励写回 |
| `tests/game/save_service_async_test.cpp` | 钱包金币写出与 roundtrip 恢复 |
| `tests/game/ui_layout_integration_test.cpp` | InventoryMenuScene 的真实金币展示 |
| `tests/game/rml_menu_navigation_style_test.cpp` | 共享导航样式与 focus/hover 规范 |

当前没有为 BattleScene 建完整 RmlUi runtime 交互测试；该层仍以 smoke 方式验证接线和静态约束为主。

## 涉及文件

| 文件 | 层 | 职责 |
|---|---|---|
| `src/game/battle/battle_types.h` | 领域 | 战斗数据类型定义（`BattleUnit` / `BattleAction` / `BattleActionResult` / `BattleSnapshot` 等） |
| `src/game/battle/battle_runtime_types.h` | 领域 | 单次会话运行时可变状态（`BattleRuntimeState`，含 `item_stocks` / 单位防御标记 / 状态回合计数） |
| `src/game/battle/battle_formula_evaluator.h/.cpp` | 执行 | Lua 战斗公式求值器，将 `a`/`b` 绑定为施法者/目标属性表并返回整型计算结果 |
| `src/game/battle/turn_core.h/.cpp` | 领域 | 回合顺序与胜负判定 |
| `src/game/battle/battle_ai_planner.h/.cpp` | 领域 | 敌方最小自动行动规划 |
| `src/game/battle/battle_action_resolver.h/.cpp` | 执行 | 技能、物品、防御、逃跑等动作结算 |
| `src/game/battle/battle_reward_resolver.h/.cpp` | 应用 | Victory 奖励汇总 |
| `src/game/battle/battle_session.h/.cpp` | 应用 | 组织 resolver、runtime_state 与 snapshot |
| `src/game/battle/battle_unit_factory.cpp` | 应用 | 由 actor/troop/catalog 构建战斗单位 |
| `src/game/scene/battle_scene.h/.cpp` | 表现 | RmlUi 菜单、输入、FlowState / MenuState 编排 |
| `ui/rmlui/scenes/battle.rml` | UI | 战斗菜单 RML 结构 |
| `ui/rmlui/scenes/battle.rcss` | UI | 战斗菜单样式与 target/list 状态表现 |
| `src/game/scene/game_scene.h/.cpp` | 表现 | 战斗入口、push/pop 与战后结算入口 |
| `src/game/scene/game_scene_battle_settlement.h/.cpp` | 表现 | 战斗结束统一入口：物品库存写回 → Victory 奖励写回 → 任务推进 → 触发通知 |
| `src/game/scene/game_scene_reward_feedback.h/.cpp` | 表现 | 奖励写回结果格式化（`BattleRewardWritebackResult`）与战斗结算合并通知（含任务推进摘要） |
| `src/game/component/player_wallet_component.h` | 探索态 | 金币真相 |
| `src/game/data/rpg_catalog.*` | 数据 | 技能、状态、actor、enemy、troop 查表 |
| `src/game/data/item_catalog.*` | 数据 | `battle_use` 物品效果查表 |
| `src/game/defs/commands.h` | 契约 | `EnterBattleCommand` / `SubmitBattleActionCommand` |
| `src/game/defs/events.h` | 契约 | `BattleEndedEvent` |
| `src/game/debug/player_debug_panel.cpp` | 调试 | 测试战斗入口 |
