# 2026-02-23 Phase 5.5 ECS 全面并行重构计划

## 元信息
- 阶段：`Phase 5.5`
- 主题：`ECS 全面并行重构（扩展版）`
- 优先级：`P1`
- 状态：`Planned`
- 上游依赖：`./phase5-ecs-task-parallel-deferred-plan.md`

## 1. 目标与定位
- `Phase 5` 已完成并行调度基础框架与测试基线，但真实并行系统仍偏少（主要是 `NPCWander ∥ AnimalBehavior`、`SpatialIndex ∥ CameraFollow`）。
- 本阶段目标：
  - 在不破坏 Gate 语义和主线程边界的前提下，显著提高探索模式 worker 覆盖率；
  - 把“并行安全靠约定”升级为“并行安全靠类型与资源声明”；
  - 建立可持续扩展的统一执行模型（任务声明、事件缓冲、deferred 结构写）。

## 2. 系统并行潜力分析（按当前实现）

| 系统 | 当前阶段 | 现状结论 | 并行化级别 | 主要阻塞点 | Phase 5.5 动作 |
|------|----------|----------|------------|------------|----------------|
| `TimeSystem` | 中段 | 主线程 | 保持主线程 | 写 `GameTime(ctx)` + `dispatcher.enqueue` | 保持主线程，不入 worker |
| `DayNightSystem` | 中段 | 未并行 | 高 | 依赖 `GameTime`，写 `GlobalLightingState(ctx)` | 纳入 worker 岛（`Time` 之后） |
| `PlayerControlSystem` | 中段 | 主线程 | 中 | `InputManager` + `dispatcher.trigger` + 多组件写 | 保持主线程；后续拆“计算/派发”再评估 |
| `NPCWanderSystem` | 中段 | 已并行 | 已完成 | - | 保持 worker |
| `AnimalBehaviorSystem` | 中段 | 已并行 | 已完成 | - | 保持 worker |
| `ChestSystem` | 中段 | 主线程 | 低 | 通知与事件派发为主 | 先保持主线程 |
| `ItemUseSystem` | 中段 | 主线程 | 低 | 通知与事件派发为主 | 先保持主线程 |
| `DialogueSystem` | 中段 | 主线程 | 中 | update 中直接发 UI 事件 + 写状态 | 拆 cooldown/逻辑 与事件派发 |
| `ActionSoundSystem` | 中段 | 主线程 | 高 | `dispatcher.enqueue` | 拆为“检测(并行) + flush(主线程)” |
| `AutoTileSystem` | 中段 | 主线程 | 高 | update 内直接结构写 tag | 改 deferred 后纳入 worker |
| `StateSystem` | 中段 | 主线程 | 高 | `dispatcher.enqueue` + 结构写 tag | 拆为“解析(并行) + 派发(主线程)” |
| `MovementSystem` | 中段 | 主线程 | 中/高 | 碰撞解析与 dynamic grid 同步耦合 | 先独占执行，后做域拆分并行 |
| `MapTransitionSystem` | Gate | 主线程 | 低 | 地图切换/场景状态强副作用 | 保持主线程 |
| `LightToggleSystem` | Gate | 主线程 | 中 | update 内结构写组件 | 改 deferred 后可 worker（仍在 Gate 区） |
| `SpatialIndexSystem` | 后段 | 已并行 | 已完成 | - | 保持 worker |
| `CameraFollowSystem` | 后段 | 已并行 | 已完成 | - | 保持 worker |
| `PickupSystem` | 后段 | 主线程 | 中 | 散落运动+收集+库存+音效耦合 | 拆 `ScatterMotion` 与 `Collect` 两段 |
| `InteractionSystem` | 后段 | 主线程 | 中 | `InputManager` + `dispatcher.trigger` | 保持主线程，后续做查询/派发拆分 |
| `AnimationSystem` | 后段 | 主线程 | 高 | `dispatcher.enqueue(AnimationEvent)` | 拆“动画推进(并行) + flush(主线程)” |

## 3. 架构升级（必做）

### 3.1 任务输出事件缓冲
- 引入 `TaskEventBuffer`（每任务局部写，wave barrier 后主线程统一 flush）。
- worker 任务不允许直接触碰 `dispatcher`（`enqueue/trigger/sink/update`）。

### 3.2 统一结构写路径
- worker 任务中的 `emplace/remove/create/destroy` 统一走 `DeferredCommands`。
- `AutoTile/LightToggle/Pickup/State/Dialogue` 分阶段迁移到 deferred。

### 3.3 调度器 mixed-policy wave
- 扩展 `ParallelWaveScheduler`：
  - 同一 wave 内允许 `MainThreadOnly + WorkerEligible` 混合；
  - 主线程任务串行执行，worker 任务并行执行；
  - wave barrier 后统一 `DeferredCommands + TaskEventBuffer` flush。

## 4. 实施路线

### Step 1（P0）基础设施增强
- 扩展调度器 mixed-policy wave。
- 新增 `TaskEventBuffer` 与统一 flush 机制。
- 为 `ctx`/外部资源（`camera/input/spatial_index/world_state/dispatcher`）建立显式资源 ID。

### Step 2（P0）低风险扩并
- `DayNightSystem` 接入中段并行岛（位于 `Time` 之后）。
- `AutoTileSystem` 改 deferred 结构写并接入并行岛。
- `PlayerControl/Time/Interaction/MapTransition` 保持主线程。

### Step 3（P1）事件耦合系统拆分
- `ActionSoundSystem`：检测与事件发送分离。
- `StateSystem`：动画 key 解析与 `PlayAnimationEvent` 发送分离。
- `AnimationSystem`：时间推进与 `AnimationEvent/Finished` 发送分离。
- `PickupSystem`：`ScatterMotionUpdate`（并行）与 `CollectAndInventory`（主线程）分离。

### Step 4（P1）Movement 并行化准备
- Movement 先保持“独占 stage”（可放 worker，但不与其他 transform 写任务同 wave）。
- 推进实体域拆分（玩家/非玩家/掉落物）并做并行正确性验证。

### Step 5（P1）并行覆盖率收口
- 探索模式中，除 `Time/PlayerControl/Interaction/MapTransition` 外，大部分 stage 进入任务图并可并发执行。
- Gate1/Gate2 语义保持不变。

## 5. 验收标准（DoD）
- 功能正确：
  - Gate 语义与现有行为一致；
  - 交互、对话、动画事件时序与现有用例一致。
- 并发正确：
  - TSAN 无新增 data race；
  - `ActionSound/State/Animation/Pickup` 具备“并行计算 + 主线程 flush”双路径测试。
- 性能与覆盖：
  - `SchedulerProfiler` 显示并行任务数显著高于 Phase 5；
  - 提供至少一组固定场景改造前后 tick P50/P95 对比。

## 6. 待办清单
- [ ] T5.5-1 扩展 `ParallelWaveScheduler` 支持 mixed-policy wave 并行执行
- [ ] T5.5-2 引入 `TaskEventBuffer`，替代 worker 直接触碰 `dispatcher`
- [ ] T5.5-3 `DayNightSystem` 接入并行岛并补资源声明/测试
- [ ] T5.5-4 `AutoTileSystem` 改 deferred 结构写并接入并行岛
- [ ] T5.5-5 拆分 `ActionSoundSystem`（检测 vs 事件发送）
- [ ] T5.5-6 拆分 `StateSystem`（解析 vs 派发）
- [ ] T5.5-7 拆分 `AnimationSystem`（推进 vs 事件发送）
- [ ] T5.5-8 拆分 `PickupSystem`（散落运动 vs 收集入包）
- [ ] T5.5-9 Movement 并行化域分析与第一版接入
- [ ] T5.5-10 完成 ASAN/TSAN + `system_scheduler_*` + 新增并行回归测试
