# FND-003 计划审阅

## 整体评价

计划结构清晰，目标合理，基线分析准确。Command/DomainEvent 边界的建立对后续 Battle/Cutscene 模式接入至关重要。以下是具体审阅意见。

## 1. 基线分析验证

逐项核对计划中的基线声明：

| 声明 | 验证结果 |
|------|----------|
| events.h 混合 Request 与结果事件 | **准确** — 13 个 Request + 10 个 Event/Changed 混在同一文件 |
| inventory_ui.cpp:334 trigger(UseItemRequest) | **准确** |
| inventory_ui.cpp:463 trigger(InventoryMoveRequest) | **准确** |
| hotbar_ui.cpp:268/303 触发快捷栏请求 | **准确** — 268 HotbarBindRequest, 303 HotbarActivateRequest |
| interaction_system.cpp:76/88 InteractRequest | **准确** — 实际在 88 和 93 行 |
| inventory_system.cpp:114 订阅 | **准确** — 订阅 5 个 Request |
| hotbar_system.cpp:17 订阅 | **准确** — 订阅 3 个 Request + 1 个 InventoryChanged |
| item_use_system.cpp:54 订阅 | **准确** — 订阅 UseItemRequest |
| dialogue_system.cpp:27 订阅 InteractRequest | **准确** |
| chest_system.cpp:113 订阅 InteractRequest | **准确**（实际在构造函数 67-68 行） |
| rest_system.cpp:25 订阅 InteractRequest | **准确** |
| 测试基线 135/135 | **需确认** — FND-002 之后已变为 134/134（部分测试可能被合并/重写） |

基线声明整体准确，个别行号有偏差但不影响理解。

## 2. 遗漏的跨系统链路（重要）

计划的范围列出了 `Inventory/Hotbar/Interact/UseItem` 四条链路，但**遗漏了三个同样发 AddItemRequest 的系统**：

| 系统 | 代码位置 | 行为 |
|------|----------|------|
| **FarmSystem** | `farm_system.cpp:395` | 收获时 `trigger(AddItemRequest)` |
| **ChestSystem** | `chest_system.cpp:142` | 开箱时 `trigger(AddItemRequest)` |
| **PickupSystem** | `pickup_system.cpp:133` | 拾取时 `trigger(AddItemRequest)` |

这些系统向 InventorySystem 发送 `AddItemRequest`，属于"系统→系统"的请求链。如果 FND-003 将 `AddItemRequest` 改名为 `AddItemCommand`，这三个系统也必须同步修改。

**建议**：
- 方案 A（推荐）：将 Farm/Chest/Pickup 的 `AddItemRequest` 一并纳入改名范围，保持一致性。改动量小（每个文件改 1 行）。
- 方案 B：本次只改名，不改语义——但需要在计划的"预计改动文件"中补充这三个文件。

同理，`InventorySystem` 内部触发 `HotbarBindRequest`（line 201）和 `HotbarUnbindRequest`（line 312）也是跨系统链路。计划中已提到 ItemUseSystem 的链路，但没有显式提及 InventorySystem 自身触发的 Hotbar 请求。

## 3. InteractRequest 的改造方案需要更细致的设计（重要）

计划提出将 InteractRequest 改为两阶段：
```
InteractCommand → InteractionResolvedEvent → Dialogue/Chest/Rest
```

但当前 InteractionSystem 的设计是**有意的扇出模式**（注释明确写了"扩展点"）：
```cpp
// - InteractionSystem 只负责：选目标 + 发请求（不写具体交互逻辑）
// - 具体交互由订阅者系统各自处理（Dialogue/Chest/Rest...）
// - 想加新交互对象时：优先新增订阅者，而不是改 InteractionSystem
```

引入 `InteractionResolvedEvent` 的问题是：**谁来"解析"并发布这个事件？**

- 如果由 InteractionSystem 自己解析目标类型并发 `InteractionResolvedEvent`，那 InteractionSystem 就需要知道"这是 NPC/Chest/Rest"——这违背了当前"InteractionSystem 不含具体交互逻辑"的设计原则。
- 如果仍然由各订阅系统自己过滤（检查 entity 上有无 DialogueComponent/ChestComponent），那 `InteractCommand` → `InteractionResolvedEvent` 的转换是多余的——只是把 Request 改名为 Command，结果事件其实没有增加新的语义。

**建议**：对 Interact 链路，最简方案是**只改名**（`InteractRequest` → `InteractCommand`），保持现有扇出语义不变。Dialogue/Chest/Rest 继续订阅 `InteractCommand`。引入 `InteractionResolvedEvent` 的时机应推迟到真正需要"统一解析后再分发"的场景（如战斗中的交互与对话共存）。

## 4. trigger() vs enqueue() 策略需要显式约定

当前混用情况：

| 调用方 | 方法 | 原因 |
|--------|------|------|
| UI 点击 | `trigger()` | 立即同步处理，UI 状态需要在当帧更新 |
| GameScene init/toggle | `enqueue()` | 延迟到下一轮 dispatcher 处理 |
| InventorySystem 内部 | `trigger()` | 同步链式处理（AddItem → HotbarBind） |
| ChestSystem 开箱 | `trigger(AddItemRequest)` + `enqueue(PlayAnimation)` | 混用 |

计划中提到了 `trigger/enqueue` 风险，但没有给出**统一策略**。这在 FND-003 的改造中是关键问题：

- 如果 Command 统一用 `enqueue()`，则 UI 操作的即时反馈会延迟一帧。
- 如果 Command 统一用 `trigger()`，则可能在 dispatcher 处理中触发嵌套 trigger，导致重入问题。

**建议**：在实现步骤 T1 中明确约定：
- **Command → 使用 `trigger()`**（保持当前行为，UI 操作即时响应）
- **DomainEvent → 使用 `enqueue()`**（结果通知延迟到统一处理点）
- 在 `commands.h` / `domain_events.h` 文件头部用注释固定此策略

## 5. `domain_events.h` 拆分方案需要明确

计划提到"若采用拆分文件方案"，但没有做出决策。当前 `events.h` 中的 168 行结构体如果拆分为 `commands.h` + `domain_events.h`，需要决定：

- `UseToolEvent / SwitchToolEvent / SwitchSeedEvent / UseSeedEvent` 归哪一边？这些是 PlayerControlSystem 和 AnimationEventSystem 发出的，语义上更接近 DomainEvent。
- `DialogueShowEvent / DialogueMoveEvent / DialogueHideEvent` 是 UI 层事件还是 DomainEvent？
- `AdvanceTimeRequest / ToggleLightRequest` 不在本次范围，保留在 `events.h`？

**建议**：采用三文件方案：
- `commands.h` — 本次范围的 Command 定义
- `events.h` — 保留不动，继续承载范围外的 Request 和 DomainEvent
- 不引入 `domain_events.h`——将已有的 DomainEvent（`InventoryChanged` 等）留在 `events.h` 中

这样改动面最小，后续可以逐步迁移。

## 6. 测试设计疑问

### 6a. `CommandDomainBoundaryTest` 如何实现？

计划说"验证 UI/输入链路仅发 Command"——这是否是又一个源码字符串扫描测试？如果是，FND-002 刚消除了 `game_scene_light_toggle_hook_test` 的字符串扫描，这里又引入类似模式。

**建议**：如果测试目的是"确保 UI 层不直接引用旧 Request 结构体"，编译器会自然保证这一点（旧结构体改名后编译错误）。更有价值的测试是验证**事件流向**——例如模拟一次 UseItemCommand，断言 InventorySystem 发出了 InventoryChangedEvent。

### 6b. `InteractionCommandPipelineTest` 的端到端范围

如果 Interact 链路按建议只改名不改语义，这个测试的价值在于验证 `InteractCommand → DialogueSystem/ChestSystem/RestSystem` 的分发仍然正确。可以用 entt::registry + dispatcher 构造真实的 entity + component，然后 trigger InteractCommand 并检查各系统的响应。

## 7. 步骤粒度与风险

步骤 2（Inventory/Hotbar 命令化）和步骤 3（UseItem 链路）可以**串行但不可并行**——因为 ItemUseSystem 触发 RemoveItemRequest/AddItemRequest，如果步骤 2 先改名了这些 Request，步骤 3 必须同步更新。

**建议**：步骤 2 和 3 合并为一步，统一处理 Inventory/Hotbar/ItemUse 命令化。否则中间状态不可编译。

## 8. 预计改动文件补充

基于代码分析，以下文件也需要修改但未列入计划：

| 文件 | 原因 |
|------|------|
| `src/game/system/farm_system.cpp` | 触发 AddItemRequest（line 395） |
| `src/game/system/pickup_system.cpp` | 触发 AddItemRequest（line 133） |
| `src/game/system/player_control_system.cpp` | 触发 HotbarSlotChanged / ToggleLightRequest（不在范围内，但如果只改 Hotbar 相关的话需要确认） |

其中 `player_control_system.cpp` 中的 `HotbarSlotChanged` 是 DomainEvent（系统发出的结果通知），不需要改名。但 `farm_system.cpp` 和 `pickup_system.cpp` 的 `AddItemRequest` 如果改名为 `AddItemCommand`，则必须修改。

## 9. 命名规范的边界 case

`InventoryChanged` 和 `HotbarChanged` 当前命名不带 `Event` 后缀。计划说"结果通知统一为 `*Event`"。是否需要改为 `InventoryChangedEvent` / `HotbarChangedEvent`？

如果改，影响面较大（GameScene、HotbarSystem 等多处订阅）。如果不改，命名规范不完全统一。

**建议**：本次不改已有的 DomainEvent 命名（`InventoryChanged` / `HotbarChanged` 等），只确保新增的结构体遵循新规范。在"非目标"中注明"不重命名既有 DomainEvent"。

## 总结

| 维度 | 评价 |
|------|------|
| 目标清晰度 | 好 |
| 基线分析 | 准确，个别行号偏差 |
| 范围控制 | 需补充 Farm/Chest/Pickup 的 AddItemRequest |
| Interact 链路设计 | 过度设计，建议只改名不引入双阶段 |
| trigger/enqueue 策略 | 需显式约定 |
| 文件拆分方案 | 需明确决策，建议最小化拆分 |
| 测试设计 | CommandDomainBoundaryTest 价值存疑，建议改为事件流向测试 |
| 步骤粒度 | 步骤 2/3 建议合并 |
| 预计改动文件 | 需补充 farm_system.cpp、pickup_system.cpp |

**建议先解决第 2（遗漏链路）、3（Interact 改造方案）、4（trigger/enqueue 策略）点再开始实现。**
