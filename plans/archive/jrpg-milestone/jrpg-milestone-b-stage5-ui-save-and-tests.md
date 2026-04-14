# Milestone B / Stage 5: UI、存档与测试补强细化计划

## 实现思路

- Stage 5 不再扩展新的战斗能力，只做 Milestone B 的收尾闭环。
- 核心目标是把“敌方 AI + 战斗奖励”变成真正可见、可存、可回归的功能，而不是只停留在运行时代码已接通。
- 结合当前代码状态，Stage 5 的前半段以审核确认为主，不预期再发生大规模运行时代码改动。
- 本阶段按三条线收尾：
  1. UI：确认金币展示统一读取 `PlayerWalletComponent`，不再残留占位或第二真相。
  2. 存档：确认 `SaveService::capture()/apply()` 与 `SaveData::player.gold` 的 roundtrip 护栏足够稳定。
  3. 测试：把 Milestone B 关键路径测试整理完整，避免后续重构把 battle provenance / AI / reward writeback / wallet UI 悄悄打回去。
- 若 Stage 5 需要补测试，优先补行为测试或纯函数测试；不再新增源码字符串匹配测试。
- 若 Stage 5 需要补文档，优先更新现有 battle/gameplay 文档，不新增重复设计稿。

## 需要新增的文件

- 必需：
  - `plans/jrpg-milestone-b-stage5-ui-save-and-tests.md`
- 代码层不强制新增运行时文件。
- 若现有测试覆盖仍有缺口，优先在已有测试文件中补 case：
  - `tests/game/save_service_async_test.cpp`
  - `tests/game/inventory_menu_scene_slot_grid_registration_test.cpp`
  - `tests/game/battle/battle_ai_planner_test.cpp`
  - `tests/game/battle/battle_reward_resolver_test.cpp`
  - `tests/game/game_scene_battle_reward_writeback_test.cpp`
- 若 battle 玩法文档已明显落后于实现，可补：
  - `docs/gameplay/turn-based-battle.md`

## 实现步骤

### Step 1. 收口金币 UI 真相

- 审核 `InventoryMenuScene` 的金币显示路径。
- 确认金币只从 `PlayerWalletComponent` 读取。
- 若仍有 `"Gold: --"` 或其他占位/重复缓存路径，统一移除。
- 保持缺失 wallet 时的降级显示与日志策略一致。
- 当前实现已符合预期：`syncCharacterPanel()` 已读取 `PlayerWalletComponent`，缺失时回退 `"Gold: 0"` 并 `warn`。
- 因此 Step 1 预计只做审核确认，除非发现新的第二真相路径，否则不预期代码改动。

### Step 2. 收口金币存档闭环

- 审核 `SaveService::capture()/apply()` 的金币读写路径。
- 明确 `SaveData::player.gold` 是钱包组件的唯一存档映射字段。
- 补强金币 roundtrip 测试，确保“运行时钱包 -> SaveData -> 恢复后钱包”稳定成立。
- 不在 Stage 5 引入新的钱包领域服务或余额上限逻辑。
- 当前实现已基本到位：`capture()` 已写出金币，`apply()` 已恢复钱包组件，`save_service_async_test.cpp` 也已覆盖写出与 roundtrip。
- 因此 Step 2 的默认策略是先审核现有覆盖；若未发现新的缺口，则不额外补测。

### Step 3. 补齐 Milestone B 回归护栏

- 整理并补强以下关键路径测试：
  - `BattleUnit` 来源信息不会回退为空字符串语义。
  - `BattleAiPlanner` 的最小敌方回合策略稳定。
  - `BattleRewardResolver` 只在 Victory 汇总奖励，且按 defeated unit 逐个结算。
  - `GameScene` 奖励写回会正确处理 `Victory / Defeat / Escaped`。
  - `InventoryMenuScene` 会显示真实金币。
- 优先补行为测试，其次纯函数测试。
- 不再新增新的源码扫描 smoke test。
- 当前 `tests/game/game_scene_battle_reward_writeback_test.cpp` 已经是行为测试，不再需要执行“源码扫描 -> 行为测试”的替换动作。
- 已把金币标签源码扫描断言移除，并改为：
  - `tests/game/inventory_menu_character_panel_test.cpp`：稳定覆盖金币文案纯逻辑
  - `tests/game/ui_layout_integration_test.cpp`：在有 RmlUi runtime 的环境里补真实布局显示护栏

### Step 4. 更新最小玩法文档

- 若 `docs/gameplay/turn-based-battle.md` 仍停留在 Milestone A 视角，补一版最小说明。
- 文档只需覆盖：
  - 敌方回合自动行动
  - Victory 奖励写回规则
  - Escape / Defeat 不发标准奖励但保留战斗物品消耗
  - 金币真相位于 player entity 的 `PlayerWalletComponent`
- 不把成长、商店、任务等 Milestone C 以后内容提前写进 Stage 5 文档。

## ToDo

- [x] 审核并收口金币 UI 读取路径
- [x] 确认 `InventoryMenuScene` 不再残留占位金币文案
- [x] 审核并收口金币存档 roundtrip 路径
- [x] 确认现有 `SaveService` 金币 roundtrip 测试已覆盖主路径
- [x] 复核 Milestone B 关键路径测试矩阵
- [x] 若有缺口，优先补行为测试而不是源码字符串测试
- [x] 视需要更新 `docs/gameplay/turn-based-battle.md`

## 疑问

- 当前没有必须阻塞 Stage 5 细化的问题。
