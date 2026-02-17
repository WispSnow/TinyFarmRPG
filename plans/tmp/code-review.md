# FND-003C 代码审阅

## 构建与测试

| 指标 | 结果 |
|------|------|
| 编译 | 通过（零警告） |
| 测试 | 146/146 通过（较 FND-003B 的 140 增加 6 个） |
| 新增文件 | 1（`inventory_move_hotbar_follow_test.cpp`） |
| 新增测试用例 | 5（move 专项）+ 1（consistency 补充）= 6 个 |
| 改动文件 | 7（含 plan 更新） |
| 净增减 | +151 / -57 = +94 行 |

## 整体评价

**干净利落**。完全实现了 FND-003B review 中 P1 的目标：InventorySystem 对 HotbarComponent 的零依赖。采纳了 review 建议（`move_kind != None` 替代 `has_move`、增加部分合并用例、执行顺序先 move 后收敛）。代码精简无冗余。

## 逐项审阅

### 1. InventoryChanged 事件扩展 — 完全符合建议

```cpp
enum class InventoryMoveKind : std::uint8_t { None, MoveToEmpty, Swap, Merge };
// + move_kind, move_from_slot, move_to_slot 三个字段
```

- 没有引入多余的 `has_move` 字段，`move_kind != None` 即为判断条件
- 默认值 `None/-1/-1` 保证非 move 路径零影响
- 枚举用 `std::uint8_t` 底层类型，紧凑

### 2. InventorySystem 清理 — 彻底

验证结果：`inventory_system.h` 和 `inventory_system.cpp` 中**零 hotbar 相关引用**。

具体移除：
- `#include "game/component/hotbar_component.h"` — 已删
- `hotbarReferencesInventorySlot()` — 已删
- `swapHotbarInventorySlotMappings()` — 已删
- `auto* hotbar = registry_.try_get<...>()` — 已删
- 三个分支中的 hotbar 写入逻辑 — 已删

`onMoveItem` 现在纯粹做库存数据变更 + diff + 发布带 move 元信息的 `InventoryChanged`。职责单一。

`emitChanged` 签名扩展为带 `move_kind/move_from_slot/move_to_slot` 默认参数，非 move 调用方（`onSync`）无需改动。设计合理。

### 3. HotbarSystem move 映射跟随 — 正确

三阶段执行顺序：**move 跟随 → 收敛循环 → from_add 自动绑定**。

对 review 建议的执行顺序分析：
- 先 move 跟随：MoveToEmpty 场景中，hotbar 引用从 from_slot 转移到 to_slot
- 再收敛：收敛循环看到 to_slot 非空，保留映射；from_slot 无映射，跳过
- 结果正确

逐分支验证：

**MoveToEmpty**（:291-295）：仅在 from_slot 有 hotbar 引用时 swap。正确——如果 from 无 hotbar 引用，不需要操作。

**Swap**（:297-299）：无条件 swap 映射。正确——swap 后两个 hotbar 槽位跟随各自的原物品。

**Merge**（:301-312）：
1. `from_slot` 不为空（部分合并）→ 直接 break，不做映射操作 → 正确
2. `from_slot` 为空，from 无 hotbar 引用 → break → 正确
3. `from_slot` 为空，from 有 hotbar 引用，to 也有 → 不 swap → 收敛循环会清理 from 的映射 → 正确
4. `from_slot` 为空，from 有 hotbar 引用，to 无 → swap（from 的 hotkey 跟随到 to）→ 正确

**辅助函数改进**：`swapHotbarInventorySlotMappings` 增加了 `changed_hotbar_slots` 输出参数，跟踪哪些 hotbar 槽位被修改，避免后续重复遍历。这比原版更精确。

### 4. 测试覆盖 — 完整

新增 `inventory_move_hotbar_follow_test.cpp`，5 个用例：

| 用例 | 覆盖场景 | 验证内容 |
|------|----------|----------|
| `MoveToEmpty_SourceHotkeyFollowsToDestination` | MoveToEmpty | hotbar 跟随 + 事件 move_kind |
| `Swap_SwapsHotbarMappings` | Swap | hotbar 互换 + 事件 move_kind |
| `MergeIntoReferencedSlot_KeepsTargetHotkeyAndClearsSource` | Merge（to 有 hotkey） | to 保留、from 清理 |
| `MergeIntoUnreferencedSlot_SourceHotkeyFollowsToTarget` | Merge（to 无 hotkey） | from 的 hotkey 跟随到 to |
| `MergePartial_SourceNotEmpty_KeepsMappingsUnchanged` | 部分合并 | 两侧映射不变 |

`inventory_hotbar_consistency_test.cpp` 新增 1 个：
- `MergePartial_KeepsHotbarMappingsUnchanged` — 与 move 测试中的部分合并对应，在 consistency 测试中也锁定了行为

测试使用 `MoveTestContext` 封装初始化，减少重复代码。`InventoryChangedCapture` 同时验证了事件中的 `move_kind/from_slot/to_slot` 字段。

### 5. 代码风格

- 符合 code-guide：现代 C++、精简、无过度防御
- `[[nodiscard]]` 正确标注在新增的 `hotbarReferencesInventorySlot`
- switch 包含 `default` 分支（编译器友好）
- 无多余注释

## 发现的问题

### 无阻塞问题

本次改动干净，没有发现需要修复的问题。

### 低优先级观察

**O1 — Merge 分支的 `!inventory.slot(evt.move_from_slot).empty()` 检查**

`hotbar_system.cpp:302`：Merge 分支第一步检查 from_slot 是否为空。这依赖于 InventorySystem 已经修改了库存数据后 trigger 的 InventoryChanged——事件到达时 from_slot 可能为空也可能不空（部分合并）。当前实现正确读取了最终状态来判断。

唯一的隐患是：如果未来有其他系统在 InventoryChanged 回调中修改了 from_slot 的数据，且订阅顺序在 HotbarSystem 之前，会导致 HotbarSystem 看到被二次修改的状态。但这属于"远期架构风险"，当前无此场景。不需要处理。

## DoD 验证

| 验收标准 | 状态 |
|----------|------|
| InventorySystem 不再在任何 move 分支写入 HotbarComponent | 通过 — 零 hotbar 引用 |
| Hotbar 映射在 move/swap/merge 全部由 HotbarSystem 收敛 | 通过 — 三种 move_kind 全覆盖 |
| 既有 Hotbar 行为不回退 | 通过 — 原 `MergeIntoReferencedSlot` 测试仍通过 |
| 全量回归通过 | 通过 — 146/146 |

## 总结

| 维度 | 评价 |
|------|------|
| 目标达成 | InventorySystem 对 HotbarComponent 完全零依赖 |
| 事件契约 | `InventoryMoveKind` 枚举 + 三字段，最小够用 |
| 执行顺序 | move 跟随 → 收敛 → from_add，正确 |
| 测试 | 6 个新用例，覆盖全部 5 种场景（含部分合并） |
| 代码质量 | 精简、符合 code-guide |
| 遗留问题 | 无 |

**结论**：代码质量高，建议合入。FND-003 系列（003/003B/003C）至此完成了从"命名约定"到"真正的架构解耦"的完整改造。
