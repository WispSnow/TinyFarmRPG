# FND-003C Move 语义下的 Hotbar 单点收敛

## 元信息
- 任务ID：`FND-003C`
- 任务标题：`Move 语义下的 Hotbar 单点收敛`
- 优先级：`P1`
- 状态：`Done`
- 负责人：`TBD`
- 计划时间：`2026-02-18` ～ `2026-02-19`（1d）
- 依赖任务：`FND-003B`（已完成）
- 对应上层计划：`./2026-02-16-foundation-backlog.md`

## 目标
- 消除 `InventoryMoveCommand` 路径下 `InventorySystem` 对 `HotbarComponent` 的直接写入。
- 将 Hotbar 映射跟随规则（move/swap/merge）统一收敛到 `HotbarSystem`。
- 在保持当前行为不回退的前提下，提升“库存变更语义 -> Hotbar 收敛”的一致性与可维护性。

## 范围
- 扩展 `InventoryChanged`，补充 move 语义元信息（`move_kind/from_slot/to_slot`）。
- `InventorySystem::onMoveItem` 仅负责库存数据变更与事件发布，不再改 hotbar 映射。
- `HotbarSystem::onInventoryChanged` 接管 move/swap/merge 下的 hotbar 映射跟随规则。
- 增加 move 语义相关测试，覆盖关键场景并执行全量回归。

## 非目标
- 不改 `InventoryDomainService` 的 add/remove 语义与接口。
- 不调整 Hotbar 自动绑定策略（继续仅在 `from_add == true` 时触发）。
- 不引入新的事件总线、阶段调度或 trait 分类机制。

## 当前问题（基线）
- `FND-003B` 后，绝大多数 Hotbar 一致性已由 `HotbarSystem` 收敛，但 `InventorySystem::onMoveItem` 仍在部分分支直接修改 `HotbarComponent`。
- 当前实现虽然行为正确且测试通过，但存在“双写入口”（InventorySystem 与 HotbarSystem 均可能改同一组件），长期会增加推导与回归成本。

## 实现思路（最优方案）
1. **事件契约增强（Move 语义显式化）**
- 在 `InventoryChanged` 中新增字段：
  - `InventoryMoveKind move_kind{None}`
  - `int move_from_slot{-1}`
  - `int move_to_slot{-1}`
- 不新增 `has_move`，以 `move_kind != None` 表示“这是 move 语义事件”。
- `move_kind` 建议覆盖集合：`None`、`MoveToEmpty`、`Swap`、`Merge`。
- 说明：`move_kind` 主要用于“是否需要做映射跟随”；空槽位解绑仍由 HotbarSystem 现有收敛逻辑处理。

2. **InventorySystem 去除 Hotbar 写入**
- `onMoveItem` 维持现有库存槽位变更逻辑与 diff 计算。
- 删除 `onMoveItem` 中所有 hotbar 映射写入（包含 swap 分支）。
- 发布带 move 元信息的 `InventoryChanged`，作为 Hotbar 收敛唯一输入。
- 额外清理：移除 `InventorySystem` 内对 `HotbarComponent` 的 include/辅助函数/局部变量依赖。

3. **HotbarSystem 统一执行映射跟随**
- 在 `onInventoryChanged` 内必须先处理 move 映射跟随，再执行已有“无效映射清理 + 槽位更新”流程。
- 顺序约束原因：`MoveToEmpty` 场景下 `from_slot` 已空，若先做收敛会先解绑引用 from 的 hotkey，后续再做 swap 会产生错误映射。
- 映射规则保持现状：
  - `MoveToEmpty`：from 的 hotkey 跟随到 to。
  - `Swap`：引用 from/to 的 hotkey 互换。
  - `Merge`：
    - 若 to 已有 hotkey：保留 to，清空 from。
    - 若 to 无 hotkey：from 跟随到 to。
- `Merge` 的部分合并（from 未清空）不做映射跟随，映射保持不变。
- 保持 `from_add` 自动绑定分支不变。

4. **测试收敛**
- 新增 move 语义专项测试，覆盖：
  - MoveToEmpty 跟随
  - Swap 互换
  - Merge（to 有 hotkey / to 无 hotkey）
  - Merge（部分合并，from 未清空）
- 保留并更新既有 `inventory_hotbar_consistency_test` 与事件流测试，确保行为不回退。

## 需要新增的文件
- `tests/game/inventory_move_hotbar_follow_test.cpp`

## 预计改动文件
- `src/game/defs/events.h`
- `src/game/system/inventory_system.h`
- `src/game/system/inventory_system.cpp`
- `src/game/system/hotbar_system.cpp`
- `tests/game/inventory_hotbar_consistency_test.cpp`
- `tests/game/command_event_flow_test.cpp`（如需补充事件断言）
- `tests/CMakeLists.txt`

## 实现步骤
1. 扩展 `InventoryChanged` 的 move 元信息。  
   说明：定义 `move_kind/from_slot/to_slot`，默认值保证非 move 场景零影响。
2. 改造 `InventorySystem::onMoveItem`。  
   说明：移除 hotbar 写入，仅产出库存 diff 与 move 元信息；清理 Hotbar 相关辅助函数。
3. 改造 `HotbarSystem::onInventoryChanged`。  
   说明：先按 move 元信息执行映射跟随，再走现有统一收敛流程。
4. 增补/更新测试。  
   说明：新增 move 专项测试（含部分合并）并更新已有一致性测试。
5. 执行全量回归。  
   说明：`ctest --test-dir build --output-on-failure -j4`。

## 待办清单（用于追踪）
- [x] T1 扩展 `InventoryChanged` move 元信息（`move_kind/from_slot/to_slot`）
- [x] T2 `InventorySystem::onMoveItem` 移除 Hotbar 直接写入
- [x] T3 `InventorySystem` 发布带 move 语义的 `InventoryChanged`
- [x] T4 `HotbarSystem` 接入 move/swap/merge 跟随规则
- [x] T5 保持 `from_add` 自动绑定策略不变并回归验证
- [x] T6 新增 `inventory_move_hotbar_follow_test.cpp`（含部分合并场景）
- [x] T7 更新 `inventory_hotbar_consistency_test.cpp`
- [x] T8 执行 `ctest --test-dir build --output-on-failure -j4`

## 验收标准（DoD）
- `InventorySystem` 不再在任何 move 分支写入 `HotbarComponent`。
- Hotbar 映射在 move/swap/merge 场景全部由 `HotbarSystem` 基于 `InventoryChanged` 收敛。
- 既有 Hotbar 行为（特别是 merge 场景）不回退。
- 全量回归通过：`ctest --test-dir build --output-on-failure -j4`。

## 风险与回滚
- 风险：
  - move 元信息设计不完整会导致 Hotbar 跟随歧义。
  - 映射跟随与“空槽清理”执行顺序处理不当可能引发边界回归。
  - 迁移后时序变化（Hotbar 改动从 InventorySystem 前置写入，改为 HotbarSystem 事件内写入）可能触发隐性回归。
- 缓解：
  - 先定义最小 `move_kind`，避免过度泛化。
  - 用专项测试锁定 5 类关键 move 行为后再重构。
- 回滚策略：
  - 若回归，优先回退 `HotbarSystem` 的 move 收敛改动，保留事件契约扩展。

## 疑问与待澄清
- 暂无。

## 进度日志
- `2026-02-17` 完成 FND-003C 实施：InventoryMove 路径下 Hotbar 映射写入已从 InventorySystem 迁移到 HotbarSystem。
- `2026-02-17` 新增 `InventoryMoveHotbarFollowTest`（5 个用例，含部分合并场景）。
- `2026-02-17` 全量回归通过：`ctest --test-dir build --output-on-failure -j4`（146/146 通过，含既有 skip）。
