# JRPG 回合制战斗系统

## 概述

回合制战斗系统将游戏从"实时探索"切换到"策略回合"模式，玩家与敌方单位按速度顺序交替行动，直至一方全灭。本系统的设计参考经典 JRPG（如 RPG Maker 系列、最终幻想早期作品）的回合驱动模型。

核心设计原则：
- **领域逻辑与表现分离** — `TurnCore` / `BattleSession` 不依赖 ECS、dispatcher、UI，可纯逻辑单测。
- **场景栈切换** — 战斗以 push/pop 叠加在探索场景之上，返回时探索状态零成本恢复。
- **状态机驱动** — `BattleScene` 内部以有限状态机编排回合流转，逻辑清晰且易扩展。

## 架构分层

```mermaid
graph TD
    subgraph "表现层 — BattleScene"
        UI["UI 面板<br/><i>血量 / 回合信息 / 按钮</i>"]
        FSM["FlowState 状态机<br/><i>WaitingForInput → ExecutingAction<br/>→ AnimatingResult → CheckVictory</i>"]
        POP["requestPopScene()<br/><i>战斗结束 → 弹出场景</i>"]
    end

    subgraph "应用层 — BattleSession"
        SUB["submitAction()<br/><i>校验 + 执行 + 产出快照</i>"]
    end

    subgraph "领域核心 — TurnCore"
        ORDER["回合顺序<br/><i>speed 降序稳定排序</i>"]
        ADV["advanceTurn()<br/><i>跳过死亡单位</i>"]
        JUDGE["evaluateOutcome()<br/><i>Victory / Defeat / Ongoing</i>"]
    end

    subgraph "场景管理"
        SM["SceneManager<br/><i>push / pop 场景栈</i>"]
        GS["GameScene<br/><i>探索场景</i>"]
    end

    UI -->|玩家输入| FSM
    FSM -->|提交动作| SUB
    SUB -->|操作| ORDER
    SUB -->|操作| ADV
    SUB -->|查询| JUDGE
    SUB -->|返回| FSM
    FSM -->|结束| POP
    POP --> SM
    SM -->|pop 后恢复| GS
```

**关键边界**：`BattleSession` 是表现层与领域核心之间的唯一桥梁。`BattleScene` 通过 `submitAction()` 与领域交互，从不直接操作 `TurnCore`；`TurnCore` 对 UI 和事件系统完全无感。

## 回合驱动原理

### 为什么用回合制而非实时

实时战斗需要逐帧处理所有单位的 AI 决策、碰撞检测与动画同步，复杂度高且难以调试。回合制将"谁行动、做什么、结果如何"拆解为离散步骤，每一步都可以暂停等待输入，适合策略型 RPG 且天然可测试。

### 回合顺序：Speed 降序稳定排序

每场战斗开始时，`TurnCore` 根据所有存活单位的 `speed` 属性进行**降序稳定排序**，生成行动队列 `turn_order_`。速度高的单位先行动；速度相同时，保持初始加入顺序（稳定排序保证）。

```
单位:   勇者(spd=12)  史莱姆A(spd=8)  法师(spd=15)  史莱姆B(spd=8)
排序后: 法师(15) → 勇者(12) → 史莱姆A(8) → 史莱姆B(8)
                                ↑ 同速保持原序
```

### 行动推进与死亡跳过

`advanceTurn()` 沿 `turn_order_` 循环推进，遇到已死亡单位（`hp <= 0`）自动跳过。若完整遍历一圈都没有存活单位，说明战斗已结束。

```mermaid
flowchart TD
    A["advanceTurn()"] --> B["current_turn_index_ + 1<br/>（模 turn_order_ 长度）"]
    B --> C{"当前单位存活？"}
    C -->|Yes| D["设为当前行动者<br/>返回 true"]
    C -->|No| E{"已遍历完整一圈？"}
    E -->|No| B
    E -->|Yes| F["无存活单位<br/>调用 refresh()<br/>返回 false"]
```

### 胜负判定

每次动作执行后，`evaluateOutcome()` 扫描全部单位：

| 存活玩家 | 存活敌人 | 判定 |
|---|---|---|
| > 0 | > 0 | `Ongoing` — 战斗继续 |
| > 0 | = 0 | `Victory` — 玩家胜利 |
| = 0 | >= 0 | `Defeat` — 玩家失败 |

判定结果存储在 `outcome_` 中，`BattleSession` 读取后附加到 `BattleActionResult` 返回给表现层。

## 动作执行链路

`BattleSession::submitAction()` 是整个战斗的核心处理节点，负责校验、执行、状态更新、结果快照的完整链路。

```mermaid
sequenceDiagram
    participant BS as BattleScene
    participant SE as BattleSession
    participant TC as TurnCore

    BS->>SE: submitAction(BattleAction)

    Note over SE: 1. 前置校验
    SE->>SE: 战斗已结束？→ Rejected
    SE->>SE: actor 非当前行动者？→ Rejected
    SE->>SE: actor 未找到或已死亡？→ Rejected

    alt Attack
        Note over SE: 2a. 攻击执行
        SE->>SE: 校验 target 存在、存活、敌方
        SE->>SE: damage = actor.attack（最少 1）
        SE->>SE: target.hp -= damage（下限 0）
        SE->>TC: refresh()（重新判定胜负）
        alt Ongoing
            SE->>TC: advanceTurn()
        end
    else EndTurn
        Note over SE: 2b. 跳过回合
        SE->>TC: advanceTurn()
    end

    Note over SE: 3. 构造结果
    SE->>SE: 生成 BattleSnapshot（全量状态副本）
    SE-->>BS: BattleActionResult
```

### 为什么用全量快照

`BattleActionResult` 中嵌入完整的 `BattleSnapshot`（所有单位当前状态 + 当前行动者 + 战斗结果），而非增量 diff。原因：

1. **UI 刷新简单** — 表现层直接从快照重建界面，无需维护本地状态副本。
2. **可测试性** — 测试只需断言快照内容，不依赖 UI 状态。
3. **未来回放** — 序列化快照序列即可实现战斗回放。

## 场景切换协议

### Push/Pop 模型

战斗场景以 **push** 方式叠加在探索场景之上，而非 **replace**。这意味着 `GameScene` 在战斗期间仍保留在场景栈中，只是不接收 `update()` / `fixedUpdate()` 调用（`SceneManager` 只更新栈顶场景）。

```mermaid
sequenceDiagram
    participant GS as GameScene
    participant SM as SceneManager
    participant BS as BattleScene
    participant D as Dispatcher

    Note over GS: 探索中...
    D->>GS: EnterBattleCommand
    GS->>SM: requestPushScene(BattleScene)
    SM->>SM: 场景栈: [GameScene, BattleScene]
    Note over SM: 只更新栈顶 → BattleScene

    Note over BS: 战斗进行中...

    BS->>D: emit BattleEndedEvent
    BS->>SM: requestPopScene()
    SM->>SM: 场景栈: [GameScene]
    Note over SM: 栈顶恢复为 GameScene

    D->>GS: BattleEndedEvent
    Note over GS: 继续探索（状态完整保留）
```

### 为什么选 Push/Pop 而非 Replace

| 方案 | 优点 | 缺点 |
|---|---|---|
| **push/pop** | 返回时状态零成本恢复；与 PauseMenuScene 架构一致 | 战斗期间 GameScene 占用内存 |
| **replace** | 释放探索场景内存 | 返回需重建 GameScene（地图、NPC、玩家位置等），成本高且易出 bug |

对于 JRPG 来说，战斗频繁且短暂，push/pop 是最优选择。

## BattleScene 状态机

`BattleScene` 内部以有限状态机（FSM）驱动回合流转，每帧在 `update()` 中调用 `runStateMachine()`。

```mermaid
stateDiagram-v2
    [*] --> WaitingForInput

    WaitingForInput --> ExecutingAction : 玩家点击 Attack / EndTurn<br/>或 AI 提交动作
    ExecutingAction --> AnimatingResult : submitAction() 返回结果
    AnimatingResult --> CheckVictory : 动画计时结束（0.2s）
    CheckVictory --> NextTurn : outcome == Ongoing
    CheckVictory --> BattleEnd : outcome == Victory / Defeat
    NextTurn --> WaitingForInput : 推进到下一行动者
    BattleEnd --> [*] : emit BattleEndedEvent<br/>requestPopScene()
```

### 各状态职责

| 状态 | 职责 |
|---|---|
| `WaitingForInput` | 等待当前行动者的输入。玩家方启用按钮；敌方（未来）由 AI 自动提交。 |
| `ExecutingAction` | 调用 `session_.submitAction()`，获取 `BattleActionResult`。同步执行，无帧延迟。 |
| `AnimatingResult` | 展示动作结果（伤害数值、击败提示）。首版以 0.2s 延时占位，未来替换为动画系统。 |
| `CheckVictory` | 读取 `outcome`，决定继续还是结束。 |
| `NextTurn` | 刷新 UI 显示新的当前行动者，回到 `WaitingForInput`。 |
| `BattleEnd` | 发送 `BattleEndedEvent`，调用 `requestPopScene()` 退出战斗。 |

### 状态机为什么是同步循环

`runStateMachine()` 在单帧内**持续循环**直到遇到需要等待的状态（`WaitingForInput` 或 `BattleEnd`）。这意味着 `ExecutingAction → AnimatingResult` 的过渡不需要跨帧，减少了状态管理的复杂度。只有 `AnimatingResult` 的计时需要跨帧等待。

## 数据结构

### 核心类型

```mermaid
classDiagram
    class BattleUnit {
        +BattleUnitId id
        +string name
        +BattleSide side
        +int hp
        +int max_hp
        +int attack
        +int speed
        +isAlive() bool
    }
    note for BattleUnit "战斗单位：双方共用同一结构\n扩展点：defense / magic / status_effects"

    class BattleAction {
        +BattleActionType type
        +BattleUnitId actor_id
        +optional~BattleUnitId~ target_id
    }
    note for BattleAction "行动指令：当前支持 Attack / EndTurn\n扩展点：Skill / Item / Defend / Flee"

    class BattleActionResult {
        +BattleActionStatus status
        +BattleActionType action_type
        +BattleUnitId actor_id
        +BattleUnitId target_id
        +int damage
        +bool target_defeated
        +BattleOutcome outcome_after
        +BattleSnapshot snapshot
    }
    note for BattleActionResult "动作结果：包含全量快照\n表现层唯一数据源"

    class BattleSnapshot {
        +vector~BattleUnit~ units
        +optional~BattleUnitId~ current_actor_id
        +BattleOutcome outcome
    }

    BattleActionResult *-- BattleSnapshot
```

### 命令与事件

```mermaid
classDiagram
    class EnterBattleCommand {
        +vector~BattleUnit~ player_units
        +vector~BattleUnit~ enemy_units
    }
    note for EnterBattleCommand "探索层 → 战斗层的入口契约\n由 debug 面板或遭遇系统发出"

    class SubmitBattleActionCommand {
        +BattleAction action
    }

    class BattleEndedEvent {
        +BattleOutcome outcome
        +vector~BattleUnit~ final_units
    }
    note for BattleEndedEvent "战斗层 → 探索层的结算契约\n未来承载经验值、掉落等数据"
```

## 完整战斗流程

以 2v2 战斗为例（勇者 + 法师 vs 史莱姆A + 史莱姆B），展示从触发到结算的完整流程：

```mermaid
sequenceDiagram
    participant DBG as Debug 面板
    participant D as Dispatcher
    participant GS as GameScene
    participant SM as SceneManager
    participant BS as BattleScene
    participant SE as BattleSession
    participant TC as TurnCore

    Note over DBG: 1. 触发战斗
    DBG->>D: EnterBattleCommand{<br/>  player: [勇者, 法师]<br/>  enemy: [史莱姆A, 史莱姆B]}
    D->>GS: onEnterBattleCommand()
    GS->>SM: requestPushScene(BattleScene)

    Note over BS: 2. 初始化
    BS->>SE: 构造 BattleSession(units)
    SE->>TC: 构造 TurnCore(units)
    TC->>TC: 按 speed 降序排序<br/>→ [法师, 勇者, 史莱姆A, 史莱姆B]
    BS->>BS: buildLayout() + refreshView()

    Note over BS: 3. 回合循环
    loop 每个行动者的回合
        BS->>BS: WaitingForInput
        alt 玩家方
            Note over BS: 启用 Attack / EndTurn 按钮
            BS->>BS: 玩家点击 Attack
            BS->>BS: selectDefaultTarget() → 首个存活敌方
        else 敌方（未来 AI）
            Note over BS: AI 自动选择动作
        end
        BS->>SE: submitAction(Attack, target)
        SE->>SE: damage = actor.attack
        SE->>SE: target.hp -= damage
        SE->>TC: refresh() → evaluateOutcome()
        SE->>TC: advanceTurn()
        SE-->>BS: BattleActionResult + Snapshot
        BS->>BS: AnimatingResult（0.2s）
        BS->>BS: CheckVictory
    end

    Note over BS: 4. 战斗结束
    BS->>D: emit BattleEndedEvent{Victory, final_units}
    BS->>SM: requestPopScene()
    D->>GS: onBattleEnded()
    Note over GS: 继续探索
```

## 扩展指南

当前实现是最小骨架，以下是各方向的扩展点：

| 扩展方向 | 当前状态 | 扩展方式 |
|---|---|---|
| **技能系统** | 仅 Attack / EndTurn | 在 `BattleActionType` 增加 `Skill`，`BattleAction` 增加 `skill_id`，`BattleSession` 增加技能查表与效果计算 |
| **状态异常** | 无 | `BattleUnit` 增加 `status_effects` 列表，`TurnCore` 在回合开始/结束时触发效果（中毒扣血、眩晕跳过等） |
| **AI 系统** | 敌方无 AI | `BattleScene` 在敌方回合调用 `AIStrategy::decide(snapshot)` 生成 `BattleAction`，无需修改领域层 |
| **多目标技能** | 单目标 | `BattleAction` 的 `target_id` 改为 `vector<BattleUnitId>`，`BattleSession` 循环处理 |
| **速度变动** | 排序仅在初始化时 | 每轮开始调用 `TurnCore::rebuildTurnOrder()`，支持减速/加速 buff |
| **战斗动画** | 0.2s 占位延时 | `AnimatingResult` 状态接入动画系统，播放攻击/受击/倒地序列帧 |
| **经验与掉落** | 仅 log | `BattleEndedEvent` 扩展 `exp_gained` / `loot_items`，`GameScene` 在 `onBattleEnded()` 中处理 |
| **逃跑/防御** | 无 | `BattleActionType` 增加 `Flee` / `Defend`，对应逻辑在 `BattleSession` 中实现 |

## 测试策略

分层测试确保各层独立可验证：

| 测试文件 | 测试目标 | 测试方式 |
|---|---|---|
| `turn_core_test.cpp` | 速度排序、死亡跳过、胜负判定 | 纯逻辑单测，无外部依赖 |
| `battle_session_test.cpp` | 动作校验、伤害计算、回合推进 | 纯逻辑单测，通过快照断言 |
| `battle_scene_smoke_test.cpp` | 状态机完整性、pop 退出路径 | 接线级验证，不 mock Context |
| `game_scene_battle_entry_test.cpp` | EnterBattleCommand 监听、push 调用 | 接线级验证 |

领域核心（`TurnCore` + `BattleSession`）的测试是最高优先级：它们运行快、无副作用、覆盖了战斗的全部规则。表现层测试以 smoke 形式验证接线正确性，不追求 UI 细节覆盖。

## 涉及文件

| 文件 | 层 | 职责 |
|---|---|---|
| `src/game/battle/battle_types.h` | 领域 | 战斗数据类型：BattleUnit / BattleAction / BattleSnapshot / BattleActionResult |
| `src/game/battle/turn_core.h/.cpp` | 领域 | 回合顺序管理、行动推进、胜负判定 |
| `src/game/battle/battle_session.h/.cpp` | 应用 | 动作校验与执行、状态快照生成 |
| `src/game/scene/battle_scene.h/.cpp` | 表现 | UI 构建、状态机驱动、玩家输入处理 |
| `src/game/scene/game_scene.h/.cpp` | 表现 | 战斗入口（sink EnterBattleCommand）与结算处理 |
| `src/game/defs/commands.h` | 契约 | `EnterBattleCommand` / `SubmitBattleActionCommand` |
| `src/game/defs/events.h` | 契约 | `BattleEndedEvent` |
| `src/game/debug/player_debug_panel.cpp` | 调试 | `Start Test Battle (2v2)` 按钮 |
