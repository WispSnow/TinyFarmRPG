# FND-003C 计划审阅

## 整体评价

方向正确——这是 FND-003B code review 中 P1 的精确解决方案。计划精简、范围收敛，没有膨胀。以下逐项分析。

## 1. move_kind 枚举设计 — 合理但需注意一个边界

计划提出 `MoveToEmpty / Swap / Merge` 三种 `move_kind`，覆盖了 `onMoveItem` 的三个分支。正确。

**但有一个被遗漏的 Merge 子情况**：当前代码中 merge 并不总是清空 from_slot。如果 from 的数量超过 to 的剩余空间，合并后 from 仍有残留（部分合并）。此时：
- from_slot 不为空，不需要解绑 hotbar
- from 和 to 的 hotbar 映射都应保持不变
- 唯一的变化是两个 slot 的 count 值

当前 `onMoveItem` 代码（`inventory_system.cpp:149`）用 `if (from.empty())` 守卫了 hotbar 修改，所以部分合并时不触发任何 hotbar 操作——这是正确的。

**建议**：`move_kind` 的 `Merge` 应区分"完全合并（from 清空）"和"部分合并（from 仍有残留）"。或者更简单的方案：HotbarSystem 不需要区分，因为它已经有"空槽位清理"逻辑——收敛循环（:254-268）检查被引用的 inventory slot 是否为空，如果空就解绑。所以 HotbarSystem 只需要在 move 场景做 swap 映射跟随，空槽位清理由现有逻辑自动完成。

**结论**：`move_kind` 的实际作用仅是告诉 HotbarSystem "需要做 swap 映射跟随"，而不是"需要做空槽位清理"。空槽位清理已经由现有收敛循环覆盖。因此 `move_kind` 可以进一步简化为两种：
- `SwapMapping`：MoveToEmpty + Swap + Merge（from 清空且 to 无 hotkey）→ 执行 swap
- `None`（默认）：Merge（to 有 hotkey 或 from 未清空）→ 不做映射跟随，交给收敛循环

甚至更极端：只需要一个 `std::optional<std::pair<int,int>> swap_hotbar_mapping` 字段就够了——告诉 HotbarSystem "请把引用 from 的 hotbar 槽位改为引用 to"。不需要枚举。

**这不是阻塞问题**。计划的三元枚举方案可以工作，只是有简化空间。

## 2. InventorySystem 去除 Hotbar 写入 — 核心价值

正确识别了三处需要移除的写入点：
1. `swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot)`（MoveToEmpty 分支，line 166）
2. `swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot)`（Swap 分支，line 173）
3. `swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot)`（Merge+无target hotkey 分支，line 157）

移除后，`onMoveItem` 只负责库存数据变更 + diff 计算 + 发布 `InventoryChanged`。`hotbar` 变量和 `hotbarReferencesInventorySlot` 函数可以完全从 `InventorySystem` 中移除。

**附加清理建议**：移除后，`inventory_system.cpp` 中以下代码也可以删除：
- `#include "game/component/hotbar_component.h"`
- `hotbarReferencesInventorySlot()` 函数
- `swapHotbarInventorySlotMappings()` 函数
- `auto* hotbar = registry_.try_get<...>(evt.target);`

这会让 InventorySystem 完全不再引用 HotbarComponent，实现真正的职责分离。

## 3. HotbarSystem 接管映射跟随 — 执行顺序重要

计划说"先处理 move 元信息，再执行已有收敛流程"。**顺序必须反过来**。

**原因**：
- 收敛循环（:254-268）检查 hotbar 引用的 inventory slot 是否为空，如果空就解绑
- Swap 映射跟随会改变 hotbar 槽位的 `inventory_slot_index_`
- 如果先 swap 再收敛：swap 后某个 hotbar 槽位指向了 from_slot（现在有新物品），收敛不会错误清理它——**正确**
- 如果先收敛再 swap：收敛看到 from_slot 为空（MoveToEmpty 场景），会把引用 from 的 hotbar 解绑为 -1，然后 swap 试图把 -1 换到 to——**错误**

等等，让我重新分析。当 `InventoryChanged` 到达 HotbarSystem 时，库存数据已经是 move 后的最终状态。所以对于 MoveToEmpty：
- from_slot 已经是空的
- to_slot 有物品
- 如果先收敛：hotbar 引用 from 的槽位会被清理为 -1（因为 from 为空）→ 随后 swap 把 -1 和 to 交换 → 结果：引用 to 的变成 -1，引用 from 的变成 to_slot → **完全错误**
- 如果先 swap：hotbar 引用 from 的改为引用 to → 收敛检查 to_slot 不为空，保留 → **正确**

**结论**：必须**先执行 move 映射跟随，再执行收敛循环**。计划中说的"先处理 move 元信息"是正确的，我重新确认了。但建议计划中明确标注这个顺序约束及其原因，避免实现时误调。

## 4. `InventoryChanged` 事件扩展方式

计划建议 `has_move/from_slot/to_slot/move_kind`。考虑到 `InventoryChanged` 的其他消费者（GameScene 是 no-op，InventoryUI 只读 slots 和 active_page），新增字段对它们零影响——默认值使非 move 场景完全透明。

**具体建议**：
```cpp
enum class InventoryMoveKind : uint8_t { None, MoveToEmpty, Swap, Merge };

struct InventoryChanged {
    entt::entity target{entt::null};
    std::vector<InventorySlotUpdate> slots{};
    bool full_sync{false};
    int active_page{0};
    bool from_add{false};
    // move 语义元信息（仅 onMoveItem 路径填充）
    InventoryMoveKind move_kind{InventoryMoveKind::None};
    int move_from_slot{-1};
    int move_to_slot{-1};
};
```

不需要单独的 `has_move` 字段——`move_kind != None` 就是语义等价。

## 5. 测试设计 — 需要 5 个而非 4 个用例

计划列出 4 个：MoveToEmpty 跟随、Swap 互换、Merge（to 有 hotkey）、Merge（to 无 hotkey）。

建议增加第 5 个：**部分合并（from 未清空）**。这个场景下 from 的 hotbar 映射应保持不变。当前代码中 `if (from.empty())` 守卫了这个行为，迁移到 HotbarSystem 后需要确保收敛循环不会误清理。

## 6. 改动文件清单验证

| 计划列出 | 实际需要 | 备注 |
|----------|----------|------|
| `src/game/defs/events.h` | 是 | 新增 move_kind 枚举和字段 |
| `src/game/system/inventory_system.h` | 可能 | 如果移除 hotbar include 的话 |
| `src/game/system/inventory_system.cpp` | 是 | 移除 hotbar 写入 + hotbar 辅助函数 |
| `src/game/system/hotbar_system.cpp` | 是 | 新增 move 映射跟随 |
| `tests/game/inventory_hotbar_consistency_test.cpp` | 是 | 更新 |
| `tests/game/command_event_flow_test.cpp` | 视需要 | |
| `tests/CMakeLists.txt` | 是 | 新增测试文件 |
| `tests/game/inventory_move_hotbar_follow_test.cpp` | 是 | 新增 |

清单完整，无遗漏。

## 7. 风险评估

计划的风险识别准确。补充一点：

**额外风险**：当前 `onMoveItem` 中 hotbar swap 是在 `emitChanged` 之前执行的，所以 HotbarSystem 收到 `InventoryChanged` 时 hotbar 组件已经是 swap 后状态。迁移后，HotbarSystem 自己做 swap，时序变化：hotbar 组件在收到事件时还是 move 前的状态。这个行为变化是正确的（因为 HotbarSystem 现在是唯一写入者），但需要确保测试覆盖。

## 总结

| 维度 | 评价 |
|------|------|
| 范围 | 精准，仅解决 P1 |
| move_kind 设计 | 可行，有简化空间但不阻塞 |
| hotbar 写入移除 | 正确识别了所有 3 处 |
| 执行顺序 | 计划中"先 move 再收敛"是正确的，建议明确标注原因 |
| 测试 | 建议增加"部分合并"用例（5 个而非 4 个） |
| 文件清单 | 完整 |
| 工作量 | ~1d 合理 |

**结论**：计划可执行，建议关注 3 点：
1. 明确标注 move 映射跟随必须在收敛循环之前执行
2. 考虑 `has_move` 字段可用 `move_kind != None` 替代
3. 测试增加"部分合并"场景
