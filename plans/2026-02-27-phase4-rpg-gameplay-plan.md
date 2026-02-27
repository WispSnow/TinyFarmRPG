# Phase 4 开发计划（RPG玩法扩展）

## 元信息
- 日期：`2026-02-27`
- 状态：`In Progress`
- 范围：`战斗动作扩展 + 8参数/公式接入 + 任务/商店状态落盘`
- 约束：`采用最优方案，可重构，不考虑向后兼容`

## 1. 已确认决策

1. `Item` 动作一期只支持“消耗并触发 `on_use.effects`”。
2. `Escape` 一期固定为“单次概率判定，失败后继续回合”。
3. 公式执行采用 **Lua 解释执行**（项目已集成 Sol2），不改为模板化硬编码公式。
4. `Item` 动作数据源沿用现有 `ItemCatalog(item_config.json)`；Phase 4 不引入 `RpgCatalog::loadItems/Weapons/Armors`。
5. 目标选择 UI 一期不做复杂化，保持自动目标兜底策略。

## 2. 实现思路

### 2.1 总体策略
1. 先打通战斗 domain（动作输入 -> 结算 -> 结果），再接入 UI。
2. 明确入口构建链路：`RpgCatalog(Actor/Class/Troop)` -> `BattleUnit`。
3. 将回合推进、状态时效、动作结算统一收敛到 battle domain，不把规则散在 Scene。
4. 保存层补齐 `quest_state/skill_state/combat_state` 的首版可落盘结构。

### 2.2 类型分层原则
1. `battle_types.h`：跨模块可见的接口层类型（命令、快照、结果、事件负载）。
2. `battle_runtime_types.h`：仅 battle 结算内部使用的临时态（防御姿态、状态剩余回合、逃跑计数等）。

### 2.3 本阶段完成标准
1. `BattleActionType` 支持 `Attack/Skill/Item/Guard/Escape/EndTurn`。
2. `BattleAction` 可携带 `skill_id/item_id` 等必要参数。
3. 伤害/恢复由 Lua 公式驱动（基于 8 参数 + 状态修正）。
4. `TurnCore` 支持回合计数与回合钩子，能驱动状态持续回合衰减。
5. `BattleActionResult` 能表达技能/物品/逃跑等扩展结果。
6. `SaveData` 可稳定读写 `quest_state/skill_state/combat_state`。

## 3. 需要新增的文件

1. `src/game/battle/battle_unit_factory.h`
2. `src/game/battle/battle_unit_factory.cpp`
3. `src/game/battle/battle_runtime_types.h`
4. `src/game/battle/battle_formula_evaluator.h`
5. `src/game/battle/battle_formula_evaluator.cpp`
6. `src/game/battle/battle_action_resolver.h`
7. `src/game/battle/battle_action_resolver.cpp`
8. `tests/game/battle/battle_unit_factory_test.cpp`
9. `tests/game/battle/battle_formula_evaluator_test.cpp`
10. `tests/game/battle/battle_action_resolver_test.cpp`
11. `tests/game/save/save_data_phase4_test.cpp`

## 4. 实现步骤

### 步骤 1：战斗入口与动作结构重构
1. 扩展 `BattleActionType` 为 `Attack/Skill/Item/Guard/Escape/EndTurn`。
2. 扩展 `BattleAction` 字段，显式支持 `skill_id`、`item_id`、目标参数。
3. 新增 `battle_unit_factory`：将 `ActorData + ClassData.base_params + TroopData` 映射为运行时 `BattleUnit`。
4. `GameScene` 改为优先通过 factory 构建战斗单位（沿用现有 `actor_ids/troop_id`）。

### 步骤 2：BattleUnit 与运行时状态建模
1. `BattleUnit` 补齐 8 参数（`mhp/mmp/atk/def/mat/mdf/agi/luk`）与资源值（`hp/mp`）。
2. `battle_runtime_types.h` 定义临时态容器（防御、状态剩余回合、逃跑相关状态）。
3. 明确 battle_types 与 runtime_types 的边界，避免接口层类型膨胀。

### 步骤 3：回合系统升级（TurnCore）
1. 增加 round counter（至少 `round_index`）。
2. 提供回合开始/结束处理钩子（用于状态持续回合结算）。
3. 将状态过期判定纳入回合推进流程。

### 步骤 4：公式执行与动作结算核心
1. 新增 `battle_formula_evaluator`：通过 Lua 执行 `DamageFormulaData.formula`。
2. Lua 注入上下文至少包含 `a`（施法者参数）、`b`（目标参数）、必要辅助函数。
3. 新增 `battle_action_resolver`：按动作类型分发到 `Attack/Skill/Item/Guard/Escape` 处理器。
4. `BattleSession` 改为调用 resolver，移除现有硬编码 switch 结算。

### 步骤 5：BattleActionResult 扩展
1. 新增结果字段：状态施加/移除列表、MP 消耗、恢复量、miss/critical、escape 成功标记。
2. 保留兼容字段（damage/target_defeated/snapshot），作为 UI 过渡层输出。

### 步骤 6：规则接入与动作语义落地
1. Skill：读取 `RpgCatalog::SkillData` 与状态效果，执行伤害/附加状态。
2. Item：读取 `ItemCatalog`，执行“消耗 + on_use.effects”。
3. Guard：设置防御临时态并影响后续受击修正。
4. Escape：单次概率判定，失败后继续回合。

### 步骤 7：BattleScene 输入层最小升级
1. 增加 Skill/Item/Guard/Escape 的操作入口。
2. 目标选择继续采用自动兜底；Phase 4 不引入复杂目标选择 UI。
3. Scene 只组装命令和展示结果，不承载规则逻辑。

### 步骤 8：存档结构扩展
1. 扩展 `QuestStateSaveData`、`SkillStateSaveData`、`CombatStateSaveData` 为首版可用结构。
2. 补齐序列化/反序列化与默认值。
3. 增加 migration 回归测试，确保旧存档可加载。

### 步骤 9：测试与验收
1. 单测：公式、动作分发、状态持续回合、逃跑概率流程。
2. 集成：`GameScene -> BattleScene -> BattleEndedEvent`。
3. 存档：`save -> load` 后 quest/skill/combat 三类状态保真。

## 5. 可追踪待办（Checklist）

- [x] T1 扩展 `BattleActionType` 与 `BattleAction`（新增 `skill_id/item_id` 等字段）
- [x] T2 新增 `battle_unit_factory` 并接入 `RpgCatalog -> BattleUnit` 构建链路
- [x] T3 扩展 `BattleUnit`（8 参数 + hp/mp）与 `battle_runtime_types`
- [x] T4 升级 `TurnCore`：回合计数与回合钩子
- [x] T5 新增 `battle_formula_evaluator`（Lua 公式执行）并完成单测
- [x] T6 新增 `battle_action_resolver` 并完成动作分发单测
- [ ] T7 扩展 `BattleActionResult`（状态变化/消耗/命中信息/逃跑结果）
- [x] T8 `BattleSession` 完成 resolver 化改造，移除硬编码结算
- [ ] T9 Skill/Item/Guard/Escape 四类动作语义落地
- [ ] T10 BattleScene 增加对应输入入口（保留自动目标兜底）
- [ ] T11 扩展 `quest_state/skill_state/combat_state` 结构与读写
- [ ] T12 完成单测+集成+存档验收

## 6. 里程碑与交付顺序

1. M1（核心重构）：完成 T1-T4。
2. M2（可结算）：完成 T5-T8。
3. M3（可玩）：完成 T9-T10。
4. M4（可持久化）：完成 T11-T12。

## 7. 风险与缓解

1. 风险：Lua 公式执行存在脚本错误与数值异常。
2. 缓解：公式执行失败直接 fail-fast + 记录错误来源技能 ID；结果统一做数值钳制。
3. 风险：BattleActionResult 扩展后 UI 字段消费滞后。
4. 缓解：保留旧字段并逐步迁移 UI 展示逻辑。

## 8. 疑问与待确认

暂无。当前已按你确认的两项决策固化到本计划。
