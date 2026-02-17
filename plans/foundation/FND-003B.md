# FND-003B 消除跨系统 Command 链耦合

## 元信息
- 任务ID：`FND-003B`
- 任务标题：`消除跨系统 Command 链耦合`
- 优先级：`P0`
- 状态：`Done`
- 负责人：`TBD`
- 计划时间：`2026-02-17` ～ `2026-02-19`（2d）
- 依赖任务：`FND-003`（已完成）
- 对应上层计划：`plans/2026-02-16-foundation-backlog.md`

## 目标
- 解决 FND-003 之后仍存在的“系统间嵌套 Command 转发”问题，降低运行时耦合与回归风险。
- 建立库存写入的统一入口（领域服务），让库存变更语义集中、可测试。
- 将 Hotbar 与 Inventory 的一致性维护收敛到 DomainEvent（`InventoryChanged`）驱动，移除 InventorySystem 内部的 Hotbar 命令转发。

## 范围
- 新增 `InventoryDomainService` 作为库存写入统一入口（加物品/减物品）。
- 改造 `ItemUse/Farm/Chest/Pickup`，不再通过 `trigger(Add/Remove*Command)` 跨系统转发库存变更。
- 简化 `InventorySystem`：移除内部 `HotbarBind/HotbarUnbind` 转发逻辑。
- 增强 `HotbarSystem::onInventoryChanged`，负责映射清理与自动绑定收敛。
- 更新相关测试并全量回归。

## 非目标
- 不引入 `GameEventBus` 门面层。
- 不引入 `event_traits.h` 或模板分类约束。
- 不改 `SystemScheduler` 为 `CommandPhase/DomainEventPhase` 双阶段模型。
- 不改 UI 交互手感（不把现有关键 `trigger()` 统一改为 `enqueue()`）。

## 当前问题（基线）
- 仍存在多条跨系统嵌套 Command 链，典型链路：
  - `ItemUse -> Remove/Add -> Inventory -> HotbarBind/Unbind -> Hotbar`
  - `Farm(收获) -> AddItem -> Inventory -> HotbarBind -> Hotbar`
  - `Chest/Pickup -> AddItem -> Inventory -> HotbarBind -> Hotbar`
- 这类链路的问题：
  - 嵌套触发深，行为难推导，失败点分散。
  - InventorySystem 需要“替 HotbarSystem 做决定”，职责边界不清晰。
  - 回归时很难定位是哪一层命令转发导致不一致。

## 实现思路（最优方案）
1. **InventoryDomainService（核心）**
- 新增库存领域服务，提供统一 API（如 `addItem/removeItem`），内部完成：
  - 组件写入（堆叠、空槽、slot 限制）
  - 结果返回（成功数量、拒绝数量、变更槽位）
  - 事件发布（`InventoryChanged` / `InventoryFullEvent`）
- 由系统显式调用 service，而非通过命令总线间接转发。

2. **调用路径收敛**
- `ItemUseSystem/FarmSystem/ChestSystem/PickupSystem` 改为直接调用 `InventoryDomainService`。
- `InventorySystem` 保留 UI/调试入口命令消费（`Add/Remove/Move/Sync/SetPage`），但其加减物品实现改为委托 service。

3. **Hotbar 一致性收敛到 DomainEvent**
- 移除 `InventorySystem` 内部的 `HotbarBind/HotbarUnbind` 命令转发。
- 增强 `HotbarSystem::onInventoryChanged`：
  - 清理无效映射（越界、空槽、无效引用）
  - 按既定规则执行自动绑定（仅对新增可绑定槽位生效，避免误绑定）
  - 发出 `HotbarChanged`

4. **测试策略**
- 重点验证“行为一致性与耦合下降”，不做字符串扫描。
- 新增 service 单测覆盖库存写入边界；更新链路测试覆盖 Hotbar 收敛行为。

## 需要新增的文件
- `src/game/domain/inventory_domain_service.h`
- `src/game/domain/inventory_domain_service.cpp`
- `tests/game/inventory_domain_service_test.cpp`

## 预计改动文件
- `src/game/runtime/system_bundle.h`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/game/system/inventory_system.h`
- `src/game/system/inventory_system.cpp`
- `src/game/system/hotbar_system.h`
- `src/game/system/hotbar_system.cpp`
- `src/game/system/item_use_system.h`
- `src/game/system/item_use_system.cpp`
- `src/game/system/farm_system.h`
- `src/game/system/farm_system.cpp`
- `src/game/system/chest_system.h`
- `src/game/system/chest_system.cpp`
- `src/game/system/pickup_system.h`
- `src/game/system/pickup_system.cpp`
- `tests/game/command_event_flow_test.cpp`
- `tests/game/item_use_system_test.cpp`
- `tests/game/inventory_hotbar_consistency_test.cpp`
- `tests/CMakeLists.txt`

## 实现步骤
1. 新增 `InventoryDomainService`。  
   说明：实现库存加减核心逻辑与变更结果结构，补充对应单测。
2. 改造库存写入调用方。  
   说明：`ItemUse/Farm/Chest/Pickup` 改为调用 service，不再跨系统转发库存命令。
3. 精简 `InventorySystem`。  
   说明：保留命令入口与同步职责，移除内部 `Hotbar*Command` 转发逻辑。
4. 增强 `HotbarSystem::onInventoryChanged`。  
   说明：在库存变更事件中完成映射收敛与自动绑定策略。
5. 更新测试并回归。  
   说明：新增 service 测试，更新链路回归，执行全量 `ctest`。

## 待办清单（用于追踪）
- [x] T1 新增 `inventory_domain_service.h/.cpp`
- [x] T2 新增 `inventory_domain_service_test.cpp`
- [x] T3 `ItemUseSystem` 改为调用 `InventoryDomainService`
- [x] T4 `Farm/Chest/Pickup` 改为调用 `InventoryDomainService`
- [x] T5 `InventorySystem` 移除内部 `HotbarBind/Unbind` 命令转发
- [x] T6 `HotbarSystem::onInventoryChanged` 完成映射收敛与自动绑定
- [x] T7 更新 `command_event_flow_test.cpp`
- [x] T8 更新 `item_use_system_test.cpp` 与 `inventory_hotbar_consistency_test.cpp`
- [x] T9 执行 `ctest --test-dir build --output-on-failure -j4`

## 测试计划
- 必跑：
  - `ctest --test-dir build --output-on-failure -j4`
- 新增测试：
  - `InventoryDomainServiceTest`：覆盖满包、堆叠上限、指定槽位、部分失败等边界。
- 更新测试：
  - `tests/game/command_event_flow_test.cpp`
  - `tests/game/item_use_system_test.cpp`
  - `tests/game/inventory_hotbar_consistency_test.cpp`

## 验收标准（DoD）
- `ItemUse/Farm/Chest/Pickup` 到库存的变更路径不再依赖跨系统命令转发。
- `InventorySystem` 不再内部触发 `HotbarBind/Unbind*Command`。
- Hotbar 映射一致性由 `InventoryChanged` 收敛，核心场景无行为回退。
- 新增与更新测试通过，且全量回归通过：`ctest --test-dir build --output-on-failure -j4`。

## 风险与回滚
- 风险：
  - Hotbar 自动绑定规则重写后可能与现有习惯不一致。
  - 库存写入统一后，边界条件（满包、多奖励）可能出现行为差异。
- 缓解：
  - 先锁定现有行为测试，再改造实现，逐步比对。
  - 对 service 进行细粒度单测，覆盖部分成功/部分失败场景。
- 回滚策略：
  - 按步骤小提交；若回归，优先回退 Hotbar 收敛改造，再回退调用方切换。

## 进度日志
- `2026-02-17` 根据审阅意见精简方案，聚焦 InventoryDomainService + Hotbar 收敛。
- `2026-02-17` 实现完成并通过全量回归：`ctest --test-dir build --output-on-failure -j4`。
- `2026-02-17` 已按约束保持 Hotbar 自动绑定策略：仅在“新增物品”语义路径触发。

## 后续跟进（非阻塞）
- `FND-003C` 候选：将 `InventoryMoveCommand` 的 hotbar 跟随语义（swap/merge/move）从 `InventorySystem` 迁移到 `HotbarSystem` 统一收敛。  
  当前实现在 move 场景仍有少量 `InventorySystem` 直接改 hotbar 映射的逻辑；虽已通过测试且行为正确，但与“单点收敛”目标仍有距离。  
  预计需要在 `InventoryChanged` 中补充 move 语义元信息（如 `from_slot/to_slot/move_kind`）后再安全迁移。
