# 2026-02-23 Phase 5.5 ECS 全面并行重构计划（审阅修订版）

## 元信息
- 阶段：`Phase 5.5`
- 主题：`ECS 并行覆盖扩展（以事件安全为核心）`
- 优先级：`P1`
- 状态：`Planned (Revised after review)`
- 上游依赖：`./phase5-ecs-task-parallel-deferred-plan.md`

## 1. 结论与范围收敛
- `Phase 5` 的定位可以明确为：并行调度基础框架 + 并发测试基线（已完成）。
- `Phase 5.5` 不再追求“一次性全量并行化”，改为聚焦三件事：
  - 建立线程安全的任务事件输出路径（`TaskEventBuffer`）；
  - 扩展少量高价值、低风险系统并行覆盖；
  - 保持 `dispatcher` 帧末统一分发语义，避免事件级联行为漂移。

不纳入本阶段（Deferred）：
- `mixed-policy wave`（同 wave 混合主线程/worker 的执行模型）
- `LightToggleSystem` 并行化（Gate 区收益低）
- `MovementSystem` 并行化（动态网格耦合高风险）
- `AutoTileSystem` worker 化（当前缺少并行伙伴，收益有限）

## 2. 系统并行潜力（修订）

| 系统 | 当前状态 | 并行化级别 | 关键约束 | Phase 5.5 决策 |
|------|----------|------------|----------|----------------|
| `DayNightSystem` | 主线程 | 中 | `ctx().emplace<GlobalLightingState>` 不能在 worker；`update()` 签名需统一 | 本阶段纳入（前提：主线程预热 ctx 值） |
| `NPCWanderSystem` | 已并行 | 已完成 | - | 保持 |
| `AnimalBehaviorSystem` | 已并行 | 已完成 | - | 保持 |
| `ActionSoundSystem` | 主线程 | 中/高 | `dispatcher.enqueue` 非线程安全 | 依赖 `TaskEventBuffer` 后纳入 |
| `StateSystem` | 主线程 | 中/高 | `dispatcher.enqueue` + `remove<StateDirtyTag>`；与动画事件链级联 | 依赖 `TaskEventBuffer + DeferredCommands` |
| `AnimationSystem` | 主线程 | 中/高 | `enqueue(AnimationEvent/Finished)`；回调写 `AnimationComponent/SpriteComponent` | 依赖事件链时序方案后纳入 |
| `PickupSystem` | 主线程 | 中 | `ScatterMotion` 写 `Transform`，与 Movement 潜在冲突 | 本阶段不拆 worker 化，Deferred |
| `AutoTileSystem` | 主线程 | 中 | `remove<AutoTileDirtyTag>` 需 deferred；读 `SpatialIndex` 与后段冲突 | Deferred |
| `LightToggleSystem` | Gate 区主线程 | 低 | Gate 区执行频率低，收益不明显 | 保持主线程 |
| `MovementSystem` | 主线程 | 高风险 | 读写 dynamic grid + transform；与 SpatialIndex 强耦合 | Deferred |
| `Time/PlayerControl/Interaction/MapTransition` | 主线程 | 保持主线程 | 输入采样/Gate/同步 trigger 路径 | 保持主线程 |

## 3. 关键架构约束（修订后）

### 3.1 TaskEventBuffer（必做）
- 目标：替代 worker 中任何 `dispatcher.enqueue/trigger` 调用。
- 设计：采用与 `DeferredCommands` 一致的类型擦除模型。
  - 形态：`std::function<void(entt::dispatcher&)>` 队列。
  - worker 任务只写入本地/任务级 buffer。
  - barrier 后主线程统一 flush 到 `dispatcher.enqueue(...)`。

### 3.2 Flush 时序（必须固定）
- wave 内顺序：`worker/main task run -> DeferredCommands drain -> TaskEventBuffer flush_to_dispatcher`
- 帧内顺序保持现状：`GameApp` 帧末 `dispatcher.update()` 统一分发。
- 目的：
  - 保持现有“事件在帧末统一处理”的行为契约；
  - 避免 per-system 立即分发导致的事件级联顺序变化。

### 3.3 registry.ctx 并发策略
- worker 只允许读/写“已存在”的 `ctx` 值。
- worker 禁止 `ctx().emplace`。
- 需要并行访问的 `ctx` 对象必须在主线程并行段前预热创建。

## 4. 事件级联链路说明（重点风险）

关键链路：
- `StateSystem::update` -> `PlayAnimationEvent`
- `AnimationSystem::onPlayAnimationEvent` -> 写 `AnimationComponent/SpriteComponent`
- `AnimationSystem::update` -> `AnimationFinishedEvent`
- `StateSystem::onAnimationFinishedEvent` -> 写 `ActionLockedTag/StateDirtyTag`

本阶段策略：
- 不改变“dispatcher 帧末统一 update”机制；
- 仅改变事件产生方式（worker -> `TaskEventBuffer` -> 主线程 enqueue）；
- 不引入 per-system 立即 dispatch，避免链路语义漂移。

## 5. 实施步骤（精简版）

### Step 1（P0）实现 `TaskEventBuffer`
- 新增 `engine/system/task_event_buffer.h/.cpp`。
- 接入 `ParallelWaveScheduler`，支持 wave barrier 后主线程 flush。
- 增加单测：多任务并发写 buffer、flush 顺序稳定性、空 buffer 零开销。

### Step 2（P0）`DayNightSystem` 并行接入
- 主线程预热 `GlobalLightingState` ctx 对象。
- 调整 `DayNightSystem` 接口以满足任务调度统一签名（可接收 deferred 参数但不使用）。
- 将其接入中段并行岛，与 `NPCWander/AnimalBehavior` 同批并发。

### Step 3（P1）`ActionSoundSystem` 改造
- worker 阶段只计算触发结果并写 `TaskEventBuffer`。
- 不在 worker 中触碰 `dispatcher`。
- 保持帧末 `dispatcher.update()` 分发。

### Step 4（P1）`AnimationSystem` 改造
- 动画推进阶段写 `TaskEventBuffer`（`AnimationEvent/Finished`）。
- 保持 `onPlayAnimationEvent` 在主线程事件分发时执行。
- 验证与 `StateSystem` 链路语义一致。

### Step 5（P1）`StateSystem` 改造
- `remove<StateDirtyTag>` 迁移到 deferred。
- `PlayAnimationEvent` 由 `TaskEventBuffer` 输出。
- 与 `AnimationSystem` 联合回归。

### Step 6（P0）全量验证与收益评估
- ASAN/TSAN 回归；`system_scheduler_*` + 并行专项测试。
- 补充 live `ThreadPool` 并发执行用例（同 wave 内并发计数峰值 > 1）。
- `SchedulerProfiler` 固定场景对比：tick P50/P95 + 并行任务数。

## 6. 验收标准（DoD）
- 功能正确：
  - Gate1/Gate2 语义不变；
  - 状态/动画/音效事件链路行为与现有回归一致。
- 并发正确：
  - TSAN 无新增 race；
  - worker 路径中无直接 `dispatcher` 调用。
  - live `ThreadPool` 用例验证同 wave 任务确实并发执行。
- 性能与覆盖：
  - 至少新增一组稳定并行组（中段 `DayNight ∥ NPCWander ∥ AnimalBehavior`）；
  - 提供改造前后固定场景 P50/P95 对比。

## 7. 待办清单
- [x] T5.5-1 新增 `TaskEventBuffer`（类型擦除）并接入 `ParallelWaveScheduler`
- [x] T5.5-2 明确并实现 flush 顺序：`deferred -> enqueue-buffer -> frame-end dispatcher.update`
- [x] T5.5-3 中段并行岛接入 `DayNightSystem`（含 ctx 预热）
- [x] T5.5-4 `ActionSoundSystem` 改 TaskEventBuffer 输出
- [x] T5.5-5 `AnimationSystem` 改 TaskEventBuffer 输出
- [x] T5.5-6 `StateSystem` 改 `DeferredCommands + TaskEventBuffer`
- [x] T5.5-7 新增事件级联回归测试（State <-> Animation）
- [ ] T5.5-8 完成 ASAN/TSAN 与 profiler 对比报告
- [x] T5.5-9 新增 live `ThreadPool` 并行执行验证测试
- [ ] T5.5-10 将 `ActionSound/State/Animation` 接入并行岛任务图（当前仅完成 buffer/deferred 双路径改造）

当前验证进度（2026-02-23）：
- ASAN：`engine_tests`/`game_tests` 关键并行与事件链路用例通过。
- TSAN：`engine_tests`/`game_tests` 同组用例通过，无新增 race 报告。
- profiler 基准对比（P50/P95）待补。

## 8. Deferred Backlog（从 5.5 移出）
- D5.5-1 `ParallelWaveScheduler` mixed-policy wave
- D5.5-2 `LightToggleSystem` 并行化
- D5.5-3 `MovementSystem` 并行化（含 dynamic grid 分区）
- D5.5-4 `PickupSystem` worker 拆分
- D5.5-5 `AutoTileSystem` worker 化
