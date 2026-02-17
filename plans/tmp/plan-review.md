# FND-003B 计划审阅

## 整体评价

方向正确——FND-003 只做了命名，FND-003B 要做真正的架构改造。但**方案过重**，存在过度设计风险。以下逐项分析。

## 1. 核心问题识别准确

计划正确识别了 5 条跨系统同步 Command 链：

| 链路 | 嵌套深度 | 风险 |
|------|----------|------|
| ItemUse → Remove + Add → Inventory → HotbarBind → Hotbar | 3 | 最深，且无回滚 |
| Farm(harvest) → AddItem → Inventory → HotbarBind → Hotbar | 3 | 收获后加物品失败无反馈 |
| Farm(seed) → RemoveItem → Inventory | 2 | 种子已种但扣除失败导致不一致 |
| Chest → AddItem(循环) → Inventory → HotbarBind → Hotbar | 3 | 部分奖励溢出静默丢失 |
| Pickup → AddItem → Inventory → HotbarBind → Hotbar | 3 | 拾取物已删除但背包满 |

这些确实是实际问题。尤其是 InventorySystem 的注释自己承认了 Hotbar 依赖的脆弱性：
```cpp
// fallback：若 HotbarSystem 不存在，至少保证组件状态正确
```

## 2. 过度设计问题（重要）

### 2a. GameEventBus 门面层不必要

计划提出用 `GameEventBus` 封装 `entt::dispatcher`，提供 `emitCommand<T>()` / `emitDomainEvent<T>()` + 阶段 guard。

**问题**：
- 这要求**所有**使用 dispatcher 的代码（30+ 个系统、UI、Scene）都改为经过 EventBus。改动面巨大。
- entt::dispatcher 的 `trigger/enqueue` 语义已经足够清晰。用一层封装替换它，收益只是"阶段检查"——一个 debug-only 的断言。
- 阶段 guard 用 `#ifndef NDEBUG` 保护的断言就够了，不需要一个门面类。

**建议**：不引入 GameEventBus。改为：
- 在 `SystemScheduler::tick()` 中用一个简单的 `phase_` 标记（Command/DomainEvent），传给系统。
- 系统在 debug 模式下可以 `assert(phase == CommandPhase)` 来检查。
- 这只改 scheduler + 几个系统的断言，不改 dispatcher 用法。

### 2b. event_traits.h 的实际约束力有限

C++ 模板 traits 可以在编译期区分 Command 和 DomainEvent 类型，但 `entt::dispatcher` 不认识这些 traits——它接受任意类型。除非你**替换整个 dispatcher 接口**（即 GameEventBus），traits 只能用于文档和静态分析，无法阻止"在错误阶段发错误类型的事件"。

如果不引入 GameEventBus，traits 的价值降到接近零。直接用命名约定（`*Command` vs `*Event`/`*Changed`）就够了。

**建议**：不引入 event_traits.h。用命名约定 + 代码审查替代。

### 2c. CommandPhase / DomainEventPhase 与当前 scheduler 不兼容

当前 scheduler 的 stage 是**具体系统更新步骤**（Chest、ItemUse、Pickup 等）。每个 stage 内部既处理 Command 又产出 DomainEvent——这是 ECS 系统的自然行为。

计划提出在 scheduler 中插入 `CommandPhase` 和 `DomainEventPhase`，但当前的事件处理是**同步 trigger 在 stage 执行期间发生的**，不存在一个可以"drain"的队列。要实现真正的阶段化，需要：
1. 所有 Command 改为 `enqueue`（延迟），而非 `trigger`（即时）
2. 在 CommandPhase 统一 `dispatcher.update()` 消费 Command 队列
3. Command 处理产出的 DomainEvent 再 enqueue
4. 在 DomainEventPhase 统一 `dispatcher.update()` 消费 DomainEvent 队列

这要求**改变所有 trigger 调用为 enqueue**——UI 点击的即时反馈全部延迟到下一帧。这是一个**手感破坏性变更**。

**建议**：不做全面的阶段化。只解决实际问题（跨系统 Command 链），不追求"理想的事件流模型"。

## 3. InventoryDomainService 是真正有价值的部分

在整个计划中，**只有 InventoryDomainService 解决了实际问题**：

当前：
```
ItemUseSystem → trigger(RemoveItemCommand) → InventorySystem → trigger(HotbarBindCommand) → HotbarSystem
```

改造后：
```
ItemUseSystem → inventoryService.removeItem() → 直接修改组件 → emit InventoryChanged
HotbarSystem.onInventoryChanged() → 自行更新映射
```

好处：
- 消除了 3 层嵌套 trigger
- InventorySystem 不再需要"替 HotbarSystem 操心"
- Farm/Chest/Pickup 调用同一个 service，统一写入口
- 可以做原子性检查（先验证再执行）

**建议**：FND-003B 的核心就应该是这一件事——引入 InventoryDomainService，消除跨系统 Command 链。其他的（EventBus、traits、阶段化）砍掉。

## 4. Hotbar 一致性收敛方案合理但需细化

计划说"Inventory 只发 DomainEvent，Hotbar 在 DomainEvent 阶段做映射收敛"。这个思路对，但要注意：

当前 HotbarSystem 被 InventorySystem 调用的场景有两个：
1. **AddItem 后自动绑定热键**：新物品加入背包时，如果热键栏有空位，自动绑定。
2. **MoveItem 合并后清理映射**：源格清空时，解绑对应热键。

改为 DomainEvent 驱动后，HotbarSystem 需要订阅 `InventoryChanged`（已经在订阅了！），然后在回调中：
1. 检查变更的 slot 是否涉及热键映射
2. 如果 slot 清空了 → 自动解绑
3. 如果新物品加入且热键有空位 → 自动绑定

**关键**：HotbarSystem 已经订阅了 `InventoryChanged`，只是当前的 `onInventoryChanged` 回调逻辑不够——它只做了 UI 同步，没做映射收敛。需要增强这个回调。

## 5. 精简后的实施建议

### 砍掉
- GameEventBus（不引入）
- event_traits.h（不引入）
- CommandPhase / DomainEventPhase 阶段化（不改 scheduler）
- 对应的 3 个测试（command_domain_event_phase_test、event_bus_guard_test）

### 保留
- InventoryDomainService（核心价值）
- HotbarSystem 增强 InventoryChanged 回调（收敛映射）
- 消除 InventorySystem 内部的 HotbarBind/UnbindCommand 转发
- 消除 ItemUseSystem/Farm/Chest/Pickup 到 InventorySystem 的 Command 链

### 精简后的文件变更

**新增：**
- `src/game/domain/inventory_domain_service.h`
- `src/game/domain/inventory_domain_service.cpp`
- `tests/game/inventory_domain_service_test.cpp`

**改动：**
- `src/game/system/inventory_system.{h,cpp}` — 移除内部 HotbarCommand 转发
- `src/game/system/hotbar_system.{h,cpp}` — 增强 onInventoryChanged 做映射收敛
- `src/game/system/item_use_system.cpp` — 改为调用 InventoryDomainService
- `src/game/system/farm_system.cpp` — 改为调用 InventoryDomainService
- `src/game/system/chest_system.cpp` — 改为调用 InventoryDomainService
- `src/game/system/pickup_system.cpp` — 改为调用 InventoryDomainService
- `src/game/runtime/system_bundle.h` — 新增 InventoryDomainService
- `src/game/runtime/game_runtime_assembler.cpp` — 装配 InventoryDomainService
- `tests/game/command_event_flow_test.cpp` — 更新
- `tests/game/inventory_hotbar_consistency_test.cpp` — 更新
- `tests/game/item_use_system_test.cpp` — 更新

### 精简后的步骤
1. 新增 InventoryDomainService（add/remove/useItem 统一入口）
2. 改造 ItemUse/Farm/Chest/Pickup 改为调用 service 而非 trigger Command
3. 移除 InventorySystem 内部的 HotbarCommand 转发
4. 增强 HotbarSystem.onInventoryChanged() 做映射收敛
5. 更新测试并回归

## 6. 预估工作量对比

| 方案 | 新增文件 | 改动文件 | 复杂度 | 价值 |
|------|----------|----------|--------|------|
| 原计划（全套） | 7 | 18 | 高 | 过度设计 |
| 精简方案 | 3 | 11 | 中 | 解决实际问题 |

## 总结

| 维度 | 评价 |
|------|------|
| 问题识别 | 准确，5 条跨系统链分析到位 |
| GameEventBus | 过度设计，不建议引入 |
| event_traits | 无实际约束力，不建议引入 |
| 阶段化 | 与当前 trigger 语义冲突，会破坏手感 |
| InventoryDomainService | **核心价值，建议保留并作为本次改造重点** |
| Hotbar 收敛 | 方向正确，需细化 onInventoryChanged 增强逻辑 |

**结论**：建议大幅精简计划，砍掉 EventBus/traits/阶段化，聚焦 InventoryDomainService + Hotbar 收敛。这样既解决了实际的跨系统耦合问题，又避免了架构膨胀。
