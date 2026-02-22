# 2026-02-22 Phase 5 ECS 系统级并行调度开发计划（修订版）

## 元信息
- 阶段：`Phase 5`
- 主题：`安全并行最小闭环（Parallel Island）+ 主线程统一提交`
- 优先级：`P2（学习驱动）`
- 状态：`Completed（含审阅意见回收与补强）`
- 关键决策：
  - `NPCWander ∥ AnimalBehavior` **暂缓**（作为延迟项），待实体域重构后再并行。
  - `dispatcher` 采用**主线程唯一写入/分发**策略，worker 线程禁止直接访问。
  - 调度观测采用**主线程聚合**，不允许从 worker 线程直接调用调试/回调接口。

## 0. 目标与范围

### 0.1 目标
- 在不破坏现有主线程边界和 Gate 语义的前提下，引入一条可验证的 ECS 系统级并行路径。
- 学习并落地 `entt::flow`/`adjacency_matrix`/`dot` 的图建模能力。
- 建立后续扩并并行组的基础设施（资源声明、波次执行、可视化、测试）。

### 0.2 本阶段范围（严格）
- 仅落地一个并行岛（Parallel Island）：
  - `SpatialIndex` 与 `CameraFollow` 并行执行（Gate2 之后）。
- 其余系统继续主线程顺序执行。
- 不做组件级并行（`std::execution::par`）和逻辑/渲染线程拆分。

### 0.3 延迟项（Deferred）
- `NPCWander ∥ AnimalBehavior` 并行暂缓。
  - 原因：当前实现存在实体集重叠，不满足“数据域不相交”前提。
  - 触发条件：完成动物/NPC 组件域重构与系统过滤条件收敛后再评估。

## 1. 全局硬约束
- `SDL_PollEvent()` 仅主线程调用（现状保持）。
- OpenGL/ImGui 调用仅主线程。
- `entt::registry` 不做并发裸写；worker 线程内禁止结构性修改（emplace/remove/create/destroy）。
- `entt::dispatcher` 仅主线程访问（`sink/trigger/enqueue/update` 全部主线程）。
- Gate 语义保持不变：
  1. `RemoveEntity`
  2. Gate1（transition active -> `TransitionUpdatePre` + `LightTogglePre` 并 return）
  3. 中段系统
  4. `TransitionUpdatePost` + `LightTogglePost`
  5. Gate2（transition active -> return）
  6. 后段系统

## 2. 架构决策（已定）

### 2.1 事件模型（Question 2 决策）
**选择：`dispatcher` 主线程专用。**

- worker 任务禁止直接触达 `dispatcher`。
- 本阶段 worker 任务只允许：
  - 只读 `registry` 或读写其声明资源（非结构性）
  - 通过 `DeferredCommands` 记录结构性操作，Barrier 后主线程 drain
- 好处：
  - 杜绝“worker 线程触发 `trigger` 导致监听器在错误线程执行”的隐患
  - 依赖图资源声明更清晰，避免把 `dispatcher` 当作跨线程共享锁

### 2.2 观测模型（Question 3 决策）
**选择：主线程聚合观测，移除 worker 回调路径。**

- `SystemScheduler::tick` 产出 `TickTrace`（或等价结构），由主线程写入 profiler。
- worker 仅写本任务局部计时结果（线程本地或预分配槽位），Barrier 后主线程汇总。
- 不再依赖“系统内部/worker 内回调 on_stage_started/on_stage_completed”。
- 好处：
  - 数据竞争面收敛
  - 观测顺序可控且可复现

## 3. 核心设计

### 3.1 执行拓扑（本阶段）

```text
主线程顺序阶段（Gate 前后保持原样）
  RemoveEntity -> Gate1 -> Time..Movement -> TransitionPost/LightPost -> Gate2
                                           |
                                           v
                           Parallel Island: SpatialIndex ∥ CameraFollow
                                           |
                                           v
                                Pickup -> Interaction -> Animation
```

### 3.2 系统声明结构

```cpp
enum class ExecutionPolicy {
    MainThreadOnly,
    WorkerEligible
};

struct SystemTaskDecl {
    SchedulerStage stage;
    std::string name;
    ExecutionPolicy policy{ExecutionPolicy::MainThreadOnly};
    std::function<void()> run; // worker-eligible 任务内部不得直接结构改写/dispatcher访问
    std::vector<entt::id_type> ro_resources;
    std::vector<entt::id_type> rw_resources;
};
```

说明：
- 本阶段无需全量系统签名重写；通过调度侧策略先落地并行基础设施。
- 后续若扩并行范围，再增量迁移系统签名。

### 3.3 `DeferredCommands`（泛化版）

```cpp
class DeferredCommands {
public:
    template<typename Component, typename... Args>
    void emplaceOrReplace(entt::entity entity, Args&&... args);

    template<typename Component>
    void remove(entt::entity entity);

    void destroy(entt::entity entity);
    void drain(entt::registry& registry); // 主线程
};
```

说明：
- 不再限制为 Tag-only，避免后续并行扩展时 API 不足。
- 本阶段实际重点使用：`SpatialIndexSystem` 的 `remove<TransformDirtyTag>` 延迟执行。

### 3.4 并行波次调度器

`ParallelWaveScheduler` 负责：
- 基于 `entt::flow` 构图
- 提取 wave（拓扑分层）
- 执行 wave（worker + 主线程 Barrier）
- 失败回退策略（重要）：
  - `submit` 失败时任务改为主线程 inline 执行
  - Barrier 仅等待成功提交的任务，避免 `latch` 永久等待

### 3.5 环检测与完整性校验

wave 提取后必须校验：
- `scheduled_vertex_count == graph.size()`
- 不满足则判定图声明错误并 fail-fast（日志 + 断言/错误返回），禁止静默丢任务。

## 4. 资源与并行组（本阶段）

### 4.1 并行岛任务（Gate2 后）
- `SpatialIndex`
  - RO: `TransformDirtyTag`, `TransformComponent`, `AABBCollider`, `CircleCollider`
  - RW: `spatial_index`
  - 结构操作：`remove<TransformDirtyTag>` 走 `DeferredCommands`
- `CameraFollow`
  - RO: `PlayerTag`, `TransformComponent`, `input`
  - RW: `camera`

### 4.2 不并行系统（原因）
- 访问 `dispatcher` 的系统：主线程专用（本阶段不进入 worker）。
- 依赖 `registry.ctx()` 且与其他写路径交错的系统：主线程专用。
- `NPCWander` / `AnimalBehavior`：延迟项。

## 5. 文件变更（修订）

## 5.1 新增文件
- `src/engine/system/system_task_decl.h`
- `src/engine/system/deferred_commands.h`
- `src/engine/system/deferred_commands.cpp`
- `src/engine/system/parallel_wave_scheduler.h`
- `src/engine/system/parallel_wave_scheduler.cpp`
- `tests/engine/system/parallel_wave_scheduler_test.cpp`

### 5.2 预计修改文件
- `src/game/runtime/system_scheduler.h`
- `src/game/runtime/system_scheduler.cpp`
- `src/engine/system/spatial_index_system.h`
- `src/engine/system/spatial_index_system.cpp`
- `src/game/debug/scheduler_profiler.h`
- `src/game/debug/scheduler_profiler.cpp`
- `tests/game/system_scheduler_profile_test.cpp`
- `tests/game/system_scheduler_transition_gate_test.cpp`
- `tests/game/system_scheduler_profiler_test.cpp`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`

## 6. 实施步骤

### Step 1：并行基础设施
- 实现 `SystemTaskDecl` / `ExecutionPolicy`
- 实现泛化 `DeferredCommands`
- 为 `ParallelWaveScheduler` 建立最小可运行框架

### Step 2：图构建与 DOT
- 用 `entt::flow` 构建并行岛 DAG
- 支持 DOT 导出，节点带 stage 名称
- 加入环检测和完整性校验

### Step 3：安全执行器
- 实现 wave 执行
- `submit` 失败回退 inline 执行
- Barrier 后主线程 `DeferredCommands::drain()`

### Step 4：系统改造（最小集）
- `SpatialIndexSystem` 增加 deferred 入口，移除 `TransformDirtyTag` 改为延迟执行
- `CameraFollowSystem` 保持现有签名（无结构修改）

### Step 5：集成到 `SystemScheduler`
- 保留现有 Gate 流程
- Gate2 后把 `SpatialIndex` 与 `CameraFollow` 交给并行岛调度
- `Pickup/Interaction/Animation` 继续顺序执行

### Step 6：观测与 profiler 重构
- `tick` 输出 `TickTrace`
- profiler 从 `TickTrace` 聚合，不再依赖 worker 回调路径

### Step 7：测试
- `parallel_wave_scheduler_test.cpp`
  1. 无依赖 -> 同 wave
  2. 有依赖 -> 分 wave
  3. `submit` 失败回退不死锁
  4. 环检测生效
  5. `DeferredCommands` drain 正确
  6. live `ThreadPool` 下 worker 并发执行可观测（非回退路径）
- 回归测试：
  - Gate1/Gate2 行为不变
  - `Movement` 仍先于 `SpatialIndex`
  - `CameraFollow` 与 `SpatialIndex` 并行路径在 TSAN 下无 race

### Step 8：TSAN 验证
- `ENABLE_TSAN=ON` 运行 `engine_tests` / `game_tests` 子集：
  - `parallel_wave_scheduler_test`
  - `system_scheduler_*` 相关测试
  - `movement_system_dynamic_grid_sync_test`

## 7. 验收标准（DoD）
- 功能：
  - Gate 语义与现有行为一致
  - 并行岛稳定运行，无死锁/任务丢失
- 并发安全：
  - TSAN 无新增 data race
  - `dispatcher` 不在 worker 线程访问
- 可观测性：
  - DOT 图可导出并可读
  - profiler 能展示并行岛执行信息

## 8. 风险与回退
- 风险1：并行收益有限（仅 2 系统）
  - 处理：保持该阶段“基础设施优先”定位，收益验证用 profiling 数据说话
- 风险2：误把 dispatcher 路径放入 worker
  - 处理：代码审查规则 + 断言 + 线程标记检查（debug）
- 风险3：图声明错误导致任务遗漏
  - 处理：完整性校验 + fail-fast

## 9. Deferred Backlog
- D1：`NPCWander ∥ AnimalBehavior` 并行化
  - 前置：
    - 明确动物/NPC 组件域边界（实体集合不重叠）
    - 系统过滤条件改为可证明互斥
    - 补齐回归与 TSAN 用例
- D2：dispatcher 领域事件分层（scheduler 内事件与 UI/场景事件解耦）
- D3：按 profiling 结果扩展第二并行岛（若收益成立）

## 10. 待办清单
- [x] T1 新增 `SystemTaskDecl` / `ExecutionPolicy`
- [x] T2 实现泛化 `DeferredCommands`
- [x] T3 实现 `ParallelWaveScheduler`（含回退与环检测）
- [x] T4 接入 `SystemScheduler`（Gate2 后并行岛）
- [x] T5 改造 `SpatialIndexSystem` 使用 deferred remove
- [x] T6 输出 DOT 并完成人工核验
- [x] T7 完成 `parallel_wave_scheduler_test.cpp`
- [x] T8 调整 profiler 为主线程聚合模型
- [x] T9 更新现有 `system_scheduler_*` 测试
- [x] T10 TSAN 回归通过
- [x] T11 增补 live `ThreadPool` 并发执行验证（确认真正走 worker 并行）
- [x] T12 `SystemScheduler` 缓存并复用 post-gate 并行岛调度器（避免每帧重建 DAG）
- [x] T13 `SpatialIndexManager` 内部 registry 引用改为 `const entt::registry*`（类型层只读约束）

## 11. 已完成记录（2026-02-22）
- 已落地并行岛：`SpatialIndex ∥ CameraFollow`（Gate2 后执行），其余阶段保持原有 Gate 语义。
- 已完成 `MapLoadingSettings` 初始化顺序修复与 `InputManager` 鼠标滚轮状态初始化修复，解决 ASAN 下相关崩溃/不稳定问题。
- 已完成并行调度稳定性加固：
  - `ParallelWaveScheduler` 并行执行改为 `future` 聚合回收；
  - 增加 wave/索引边界校验与异常图 fail-fast；
  - 调度任务 capture 改为更安全的指针值捕获。
- 已完成验证（`build-debug-asan`）：
  - `cmake --preset debug-asan && cmake --build --preset debug-asan` 通过；
  - `ctest --output-on-failure -j8` 全量通过（274/274）；
  - `ParallelWaveSchedulerTest` 与 `system_scheduler_*` 相关测试通过；
  - 新增并通过稳定性回归用例：`ExplorationParallelIslandRemainsStableAcrossManyTicks`。
- 并发专项复验（`build-tsan`）：
  - `cmake --build . --target engine_tests game_tests -j8` 通过；
  - `ctest --output-on-failure -R "ParallelWaveSchedulerTest|SystemScheduler" -j8` 通过（18/18）。
- 基于审阅意见的补强已完成：
  - `ParallelWaveSchedulerTest` 新增 live worker 并发用例：`WorkerEligibleWaveRunsOnMultipleWorkers`；
  - `SystemScheduler` 改为缓存 `post_gate_parallel_island_scheduler_`，tick 期间仅执行并刷新上下文；
  - `SpatialIndexManager::registry_` 改为 `const entt::registry*`，收紧读路径类型边界；
  - `CameraFollow` 并行路径补充注释，明确 `InputManager` 写入发生在主线程 `handleEvents()`，并行阶段只读。

## 12. 审阅意见处理结论（2026-02-22）
- `并行安全/Deferred/submit 回退/环检测/Gate 语义/storage 预热/profiler 重构`：结论合理，已采纳并验证通过。
- `P2: SpatialIndexManager 非 const registry 读路径`：合理，已修复为 `const entt::registry*`。
- `P3: CameraFollow 读 InputManager 的时序前提`：合理，已通过代码注释显式化约束。
- `P2: 每帧重建 ParallelWaveScheduler`：合理，已改为调度器缓存复用。
- `P3: CameraFollow 顺序位置变化`：评估为安全（无数据依赖冲突），且现有测试通过；保持当前顺序。
- `P3: map_manager.h 声明顺序修复为顺手修复项`：合理但非阻塞，不影响 Phase 5 并行方案正确性。
