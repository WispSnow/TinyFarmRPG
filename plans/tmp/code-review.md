# FND-003B 代码审阅

## 构建与测试

| 指标 | 结果 |
|------|------|
| 编译 | 通过（零警告） |
| 测试 | 140/140 通过（较 FND-003 的 137 增加 3 个） |
| 新增文件 | 2（`inventory_domain_service.h/.cpp`） |
| 新增测试 | 1（`inventory_domain_service_test.cpp`，3 个用例） |
| 改动文件 | 21（含 6 个测试文件） |
| 净增减 | +357 / -267 = +90 行 |

## 整体评价

实现**完全符合精简后的计划**：聚焦 InventoryDomainService + Hotbar 收敛，没有引入 GameEventBus / event_traits / 阶段化等过度设计。代码质量高，职责拆分清晰。

## 逐项审阅

### 1. InventoryDomainService（核心）— 优秀

- 接口设计干净：`addItem(target, item_id, count, preferred_slot)` / `removeItem(target, item_id, count, slot_index)`
- 返回值 `InventoryMutationResult` 包含 `accepted/rejected/changed_slots`，调用方可以根据结果做后续判断
- 正确复用 `inventory_helpers.h` 的 `tryMerge/tryFillEmpty`，没有重复实现
- `addItem` 的 stack_limit 使用 `std::max(1, item->stack_limit_)` 避免零上限，优于原 InventorySystem 实现
- 事件发射与结果返回顺序正确：先 `emitChanged`，再 `InventoryFullEvent`，最后 `return result`

### 2. InventorySystem 瘦身 — 优秀

- `onAddItem/onRemoveItem` 变为单行委托，消除了 InventorySystem 内 ~120 行的 add/remove 逻辑
- 完全移除了 `HotbarBindCommand/HotbarUnbindCommand` 的转发：**零残留**
- `onMoveItem` 中合并分支的注释清楚地解释了为什么不再直接修改 hotbar 映射
- 保留了 `onSync/onMoveItem/onSetActivePage` 这些不涉及跨系统 Command 链的逻辑，拆分粒度合理
- 移除了 `findHotbarSlotToFill`、`isItemOnHotbar`、`collectHotbarSlotIndicesReferencingInventorySlot`、`selectInventorySlotForItem` 共 4 个内部辅助函数

### 3. HotbarSystem 增强 — 优秀

- `onInventoryChanged` 增强为两阶段：
  1. 先收敛已有映射（空槽位 → 解绑）
  2. 仅在 `from_add` 时才做自动绑定新物品
- 引入了 `inventorySlotAffected()` 优化：只处理变更涉及的 hotbar 槽位
- `upsertUpdate/pushSlotUpdate` 确保同一 hotbar_index 不会重复推送（先解绑再绑定的场景）
- 自动绑定逻辑完整迁移：`isItemOnHotbar` / `findHotbarSlotToFill` / `selectInventorySlotForItem` 三个函数从 InventorySystem 原样搬入

### 4. 调用方改造 — 正确

| 系统 | 改造方式 | 验证 |
|------|----------|------|
| ItemUseSystem | `removeItem` + 循环 `addItem` 替代 trigger | 返回值 `(void)` cast 明确表示不关心结果 |
| FarmSystem | `removeItem`（种子扣减）+ `addItem`（收获）替代 trigger | 正确 |
| ChestSystem | 循环 `addItem` 替代循环 trigger | 正确 |
| PickupSystem | `addItem` 替代 trigger | 正确，且移除了不再需要的 `commands.h` 和 `events.h` include |

### 5. `from_add` 标志位 — 设计合理

- 在 `InventoryChanged` 事件中新增 `from_add` 字段，仅 `InventoryDomainService::addItem` 路径设置为 `true`
- `InventorySystem::emitChanged` 硬编码 `from_add = false`（用于 Sync/Move 路径）
- HotbarSystem 只在 `from_add == true` 时执行自动绑定，避免 Move 操作误触发自动绑定

### 6. 装配层 — 正确

- `GameRuntimeServices` 正确添加了 `inventory_domain_service` 成员
- `assembleSystems` 中在所有系统创建之前构造 `InventoryDomainService`
- 所有 5 个消费方（InventorySystem/ItemUseSystem/FarmSystem/ChestSystem/PickupSystem）正确接收引用

### 7. 测试覆盖 — 充分

- 新增 `InventoryDomainServiceTest`：3 个用例覆盖部分添加+满包、指定槽位移除、自动创建背包
- 既有测试全部适配新构造参数，无逻辑改动
- `inventory_hotbar_consistency_test` 的 merge 场景仍然通过——验证了 HotbarSystem 收敛路径正确

## 发现的问题

### P1 — InventorySystem 对 MoveItem 合并分支的 Hotbar 处理存在不一致

**场景**：合并（merge）导致 from_slot 清空，from_slot 和 to_slot 都有 hotbar 映射（`target_has_hotkey == true`）。

**当前行为**（`inventory_system.cpp:155-158`）：
```cpp
if (!target_has_hotkey) {
    swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot);
}
// target_has_hotkey == true 时什么都不做
```

InventorySystem 在 `target_has_hotkey` 时不修改 hotbar 组件，而是依赖后续 `emitChanged` → `HotbarSystem::onInventoryChanged` 收敛。

但 `emitChanged` 发出的 `InventoryChanged` 带 `from_add = false`，HotbarSystem 在收敛阶段（:254-268）会发现 from_slot 为空，将对应 hotbar 槽位的 `inventory_slot_index_` 置为 -1。**行为正确**。

**但有一个微妙的问题**：在 `!target_has_hotkey` 分支中，InventorySystem **直接修改了 hotbar 组件**（`swapHotbarInventorySlotMappings`），然后才发 `InventoryChanged`。当 HotbarSystem 收到事件时，hotbar 组件已被改过——HotbarSystem 看到的映射状态是 swap 后的，不是原始的。这在当前逻辑中不会出错（因为 swap 后 from_slot 的映射已经转移到 to_slot 了），但这种"两个系统都可以修改同一个组件"的模式正是 FND-003B 要消除的。

**建议**：此问题不阻塞合入，但应在后续 backlog 中记录：将 `onMoveItem` 的 hotbar swap 逻辑也迁移到 HotbarSystem 中。方法是在 `InventoryChanged` 事件中增加 move 语义信息（`from_slot/to_slot`），让 HotbarSystem 统一做映射跟随。

### P2 — DebugPanel 仍直接发 AddItem/RemoveItemCommand

`inventory_debug_panel.cpp:83,88` 仍通过 `dispatcher_.trigger(AddItemCommand/RemoveItemCommand)` 操作库存。这不影响正确性（InventorySystem 仍订阅这些 Command 并委托给 service），但 DebugPanel 作为"输入层"直接发 Command 是正确的 — 与 UI 层发 Command 的模式一致。

**结论**：不是问题，保持现状即可。这是计划中"保留 UI 触发 Command"的正确体现。

### P3 — addItem 的 preferred_slot 在合并阶段可能产生重复 diff 条目

`inventory_domain_service.cpp:65-74` 中 preferred_slot 优先处理后，后续的"合并同类"循环（:76-83）不排除 preferred_slot，理论上可能对同一 slot 写入两次 diff。

分析实际影响：preferred_slot 已经被 tryMerge 填满后，后续循环的 tryMerge 不会再改这个 slot（因为 space = 0），所以**不会实际重复**。但如果 preferred_slot 被 tryMerge 部分填充（还有空间），后续循环会再次修改它并再 push 一次 diff。

**影响**：diff 数组中同一 slot_index 会出现两次。HotbarSystem 和 UI 消费 diff 时用 `any_of` 查找，不会出错，但语义不精确。

**建议**：低优先级，可在后续优化 diff 去重。不阻塞合入。

## DoD 验证

| 验收标准 | 状态 |
|----------|------|
| 库存写入有统一入口（InventoryDomainService） | 通过 — ItemUse/Farm/Chest/Pickup 全部改为直接调用 service |
| Hotbar 一致性由 DomainEvent 收敛 | 通过 — HotbarSystem.onInventoryChanged 完成映射收敛与自动绑定 |
| 系统间"命令转发式耦合"消除 | 通过 — InventorySystem 内零 HotbarCommand 残留，4 个调用方零 AddItem/RemoveItemCommand trigger |
| 全量回归通过 | 通过 — 140/140 |

## 总结

| 维度 | 评价 |
|------|------|
| 架构改造 | 精准聚焦，无过度设计 |
| InventoryDomainService | 接口清晰，职责单一，返回值设计良好 |
| HotbarSystem 收敛 | 逻辑完整，`from_add` 策略正确区分 add 与 move 语义 |
| 代码质量 | 符合 code-guide 要求：现代 C++、精简、无过度防御 |
| 测试覆盖 | 充分，新增 3 个用例，既有 7 个测试正确适配 |
| 遗留问题 | P1 onMoveItem hotbar swap 未统一到 HotbarSystem（不阻塞），P3 diff 可能微重复（无害） |

**结论**：代码质量高，实现与精简计划完全吻合。建议合入，P1 记入后续 backlog。
