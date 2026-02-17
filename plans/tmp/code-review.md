# FND-003 代码审阅

## 审阅范围

### 新增文件
- `src/game/defs/commands.h` — 11 个 Command 结构体定义
- `tests/game/command_event_flow_test.cpp` — UseItem 端到端事件流测试
- `tests/game/interaction_command_pipeline_test.cpp` — InteractCommand 扇出测试

### 改动文件（27 个）
- `src/game/defs/events.h` — 移除 11 个 `*Request` 定义（-67 行）
- `src/game/system/inventory_system.{h,cpp}` — 订阅/回调改为 `*Command`
- `src/game/system/hotbar_system.{h,cpp}` — 同上
- `src/game/system/item_use_system.{h,cpp}` — 消费 `UseItemCommand`，发出 `RemoveItemCommand`/`AddItemCommand`
- `src/game/system/interaction_system.cpp` — 发布 `InteractCommand`
- `src/game/system/dialogue_system.{h,cpp}` — 订阅 `InteractCommand`
- `src/game/system/chest_system.{h,cpp}` — 订阅 `InteractCommand`，发布 `AddItemCommand`
- `src/game/system/rest_system.{h,cpp}` — 订阅 `InteractCommand`
- `src/game/system/farm_system.cpp` — 发布 `AddItemCommand`/`RemoveItemCommand`
- `src/game/system/pickup_system.cpp` — 发布 `AddItemCommand`
- `src/game/system/player_control_system.{h,cpp}` — 订阅 `HotbarActivateCommand`
- `src/game/ui/inventory_ui.cpp` — 发布 Command
- `src/game/ui/hotbar_ui.cpp` — 发布 Command
- `src/game/scene/game_scene.cpp` — enqueue Command
- `src/game/debug/inventory_debug_panel.cpp` — trigger Command
- `src/game/save/save_service.cpp` — trigger Command
- `tests/CMakeLists.txt` — 新增 2 个测试文件
- `tests/game/inventory_hotbar_consistency_test.cpp` — 改为 `*Command`
- `tests/game/item_use_system_test.cpp` — 改为 `*Command`
- `tests/game/rest_area_interaction_test.cpp` — 改为 `*Command`

## 构建与测试

- 构建：通过（增量编译无错误）
- 测试：**137/137 通过**（+3 新增测试），3 个 Audio 测试正常 Skipped
- 新增测试：
  - `CommandEventFlowTest.UseItemCommand_EmitsDomainEventsAndUpdatesHotbarView` ✓
  - `InteractionCommandPipelineTest.InteractCommand_FansOutToComponentDrivenSubscribers` ✓
- 旧名称残留检查：`src/` 和 `tests/` 中 **零残留**（11 个旧 `*Request` 名称全部清除）

## DoD 逐项核对

| 验收标准 | 状态 | 备注 |
|----------|------|------|
| Inventory/Hotbar/Interact/UseItem 四条链路符合 Command→DomainEvent 边界 | **通过** | 全部改为 `*Command`，DomainEvent 保持原名 |
| Farm/Pickup/Chest 跨系统加物品链路完成命令化 | **通过** | 三处 `AddItemRequest` 均改为 `AddItemCommand` |
| UI/Scene/Debug 层不再触发 `*Request` | **通过** | grep 验证零残留 |
| 结果事件语义清晰，既有 DomainEvent 名称稳定 | **通过** | `InventoryChanged`/`HotbarChanged` 等保持原名 |
| 全量测试通过 | **通过** | 137/137 |

## 编码规范合规性

对照 `for_agent/code-guide.md` 的两条要求：

### 现代 C++ 语法风格
- `commands.h` 使用 C++17 聚合初始化默认值，与 `events.h` 风格一致 ✓
- 无裸指针、无 `new`/`delete`、无 `using namespace` 在头文件 ✓

### 保持代码精简，不做过度防御；仅考虑最优方案，不考虑向后兼容
- 采用"直接切换"策略，无兼容桥接层 ✓
- 旧 `*Request` 从 `events.h` 中直接删除，不保留 typedef/alias ✓
- 交互链路只改名，不引入多余的中间解析事件层 ✓

## 设计质量评价

### 优点

1. **改动纯净**：本次是纯机械化重命名 + 文件拆分。没有逻辑变更、没有行为变更、没有新的执行路径。这是最安全的重构策略。

2. **`commands.h` 结构干净**：只包含 Command 定义，不依赖 `events.h`，头文件依赖链清晰（只需 `entt/core/fwd.hpp` 和 `entt/entity/entity.hpp`）。

3. **InteractCommand 保留扇出模式**：按审阅建议，没有引入 `InteractionResolvedEvent`。Dialogue/Chest/Rest 继续直接订阅 `InteractCommand`，设计简洁。

4. **`command_event_flow_test` 质量好**：不是字符串扫描，而是真正的端到端事件流测试——构造 InventorySystem + HotbarSystem + ItemUseSystem，发 UseItemCommand，验证 InventoryChanged 和 HotbarChanged 都正确产出。

5. **`interaction_command_pipeline_test` 验证了核心扇出语义**：用轻量 mock subscriber 验证 InteractCommand 按组件类型正确分发到 Dialogue/Chest/Rest。

6. **`save_service.cpp` 也被正确迁移**：这个文件不在原始计划的"预计改动文件"列表中，但实际代码中它 trigger 了 `InventorySyncRequest`/`HotbarSyncRequest`/`HotbarActivateRequest`。Codex 正确识别并迁移了它。

### 需要关注的问题

#### 问题 1（小）：`events.h` 中剩余的 `AdvanceTimeRequest` 和 `ToggleLightRequest`

```cpp
struct AdvanceTimeRequest {
    int hours{0};
};

struct ToggleLightRequest {
    entt::id_type light_type_id{entt::null};
};
```

这两个仍使用 `*Request` 命名，但在 FND-003 计划中被显式排除（"非目标：不重构非本次链路的事件"）。这是正确的——它们不在本次范围内。但后续需要在某个阶段迁移为 `*Command`，保持整体一致性。

**建议**：无需当前处理，但建议在 backlog 中记录。

#### 问题 2（小）：`item_use_system.h` 的 include 变化

```diff
-#include "game/defs/events.h"
+#include "game/defs/commands.h"
```

`item_use_system.h` 原本 include `events.h`，现在改为 `commands.h`。但 `item_use_system.cpp` 中仍然需要 `events.h`（用到 `DialogueShowEvent` 等），它在 `.cpp` 中单独 include 了。这没问题，但如果 `.h` 中有其他地方间接依赖 `events.h` 中的类型，可能会在其他编译单元引发问题。

验证：编译通过，说明当前不存在此问题。

#### 问题 3（小）：trigger/enqueue 语义未在代码中显式标注

审阅建议中提到应在 `commands.h` 头部用注释约定 trigger/enqueue 策略。当前 `commands.h` 没有此注释。不影响行为，但会降低后续维护者的理解成本。

**建议**：考虑在 `commands.h` 顶部加一行注释说明 Command 的推荐发送方式。

## 改动完整性验证

通过 grep 验证所有 11 个旧 `*Request` 名称在 `src/` 和 `tests/` 中彻底清除：

| 旧名称 | src/ 残留 | tests/ 残留 |
|--------|-----------|-------------|
| AddItemRequest | 0 | 0 |
| RemoveItemRequest | 0 | 0 |
| UseItemRequest | 0 | 0 |
| InventorySyncRequest | 0 | 0 |
| InventoryMoveRequest | 0 | 0 |
| InventorySetActivePageRequest | 0 | 0 |
| HotbarBindRequest | 0 | 0 |
| HotbarUnbindRequest | 0 | 0 |
| HotbarActivateRequest | 0 | 0 |
| HotbarSyncRequest | 0 | 0 |
| InteractRequest | 0 | 0 |

## 计划合规性

| 计划待办项 | 状态 | 对应实现 |
|------------|------|----------|
| T1 新增 commands.h | **完成** | 11 个 Command 定义 |
| T2 整理 events.h | **完成** | 移除 11 个 Request，保留 DomainEvent |
| T3 UI 改为发送 Command | **完成** | inventory_ui.cpp + hotbar_ui.cpp |
| T4 GameScene/DebugPanel 改为发送 Command | **完成** | game_scene.cpp + inventory_debug_panel.cpp |
| T5 InventorySystem/HotbarSystem 消费 Command | **完成** | 订阅/回调全部改名 |
| T6 ItemUseSystem 消费 UseItemCommand | **完成** | 含链式 RemoveItemCommand/AddItemCommand |
| T7 Interact 链路命令化 | **完成** | 保留扇出，仅改名 |
| T8 command_event_flow_test | **完成** | 端到端事件流验证 |
| T9 interaction_command_pipeline_test | **完成** | 扇出分发验证 |
| T10 更新回归测试 | **完成** | 3 个测试文件 + save_service |
| T11 全量测试 | **通过** | 137/137 |

## 总结

| 维度 | 评价 |
|------|------|
| 改动完整性 | 好，11 个旧名称零残留，含计划外的 save_service.cpp |
| 行为等价性 | 好，纯重命名+文件拆分，无逻辑变更 |
| 编码规范 | 合规，代码精简无过度防御 |
| 测试覆盖 | 好，2 新增 + 3 更新，端到端事件流覆盖 |
| 计划合规 | 完全合规，所有 T1-T11 均已完成 |

**结论**：代码质量好，可以提交。问题均为小项，不阻塞。
