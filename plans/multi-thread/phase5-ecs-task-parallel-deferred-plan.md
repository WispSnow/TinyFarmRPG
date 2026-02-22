# 2026-02-22 Phase 5 ECS 系统级并行调度开发计划

## 元信息
- 阶段：`Phase 5`
- 主题：`基于 entt::flow 的系统依赖图与 Wave 并行调度`
- 优先级：`P2（学习驱动）`
- 状态：`Planned`
- 范围边界：
  - **系统级并行**（system-level parallelism）：不同系统在线程池中并发执行。不做组件级并行迭代（`std::execution::par`）。
  - 系统接口直接改造为接受 `DeferredCommands&`，**不保留旧签名**、不做适配层间接包装。项目未上线，不需要向后兼容。
  - 不改变帧内行为确定性：相同输入产生相同组件状态（同一 Wave 内系统的事件/tag 操作顺序不保证，但 Wave 间严格有序）。
  - `entt::registry` / `entt::dispatcher` 不做并发裸写。Dispatcher 在依赖图中视为共享 RW 资源。
  - 串行与并行路径**统一使用 `DeferredCommands`**（串行时每系统执行后立即 drain），消除双代码路径。

## 0. 学习目标

本阶段以学习 entt 多线程工具为核心驱动：

| 工具 | 头文件 | 用途 |
|------|--------|------|
| `entt::flow` | `entt/graph/flow.hpp` | 手动声明资源依赖，构建任务 DAG |
| `entt::adjacency_matrix<directed_tag>` | `entt/graph/adjacency_matrix.hpp` | 有向图表示、边遍历 |
| `entt::dot()` | `entt/graph/dot.hpp` | DOT 格式导出，可视化依赖图 |
| `entt::organizer` | `entt/entity/organizer.hpp` | 基于函数签名的自动依赖推导（教学对比） |

额外实践：Wave-based parallel execution 模式、`std::latch` barrier 同步、`DeferredCommands` 命令缓冲。

## 1. 核心设计

### 1.1 整体架构

```
初始化阶段（一次性）                         运行阶段（每帧 tick）
┌───────────────────────────────┐           ┌──────────────────────────────────────┐
│ SystemTaskDecl[] (22 个系统)   │           │ RemoveEntity (串行)                   │
│   ↓                           │           │ Gate1 check                           │
│ entt::flow::bind/ro/rw/sync   │   build   │   ↓                                  │
│   ↓                           │ ────────→ │ Wave 0: [Time]                        │
│ adjacency_matrix<directed>    │           │ Wave 1: [DayNight, PlayerControl]     │
│   ↓                           │           │ Wave 2: [NPCWander ∥ AnimalBehavior] │
│ BFS topological → waves[]     │           │ Wave 3: [Chest, ItemUse, ...]         │
│   ↓                           │           │ ...                                   │
│ ParallelWaveScheduler         │           │ DeferredCommands::drain() per barrier │
└───────────────────────────────┘           └──────────────────────────────────────┘
```

### 1.2 SystemTaskDecl（系统资源声明）

```cpp
// src/engine/system/system_task_decl.h
struct SystemTaskDecl {
    std::string name;                            // 系统名称（DOT 可视化用）
    std::function<void(DeferredCommands&)> run;  // 系统 update 回调（统一接受引用）
    std::vector<entt::id_type> ro_resources;     // 只读组件/资源 type_hash
    std::vector<entt::id_type> rw_resources;     // 读写组件/资源 type_hash
    bool sync_point = false;                     // 是否为强制同步点
};
```

**资源 ID 约定**：使用 `entt::type_hash<Component>::value()` 标识组件类型。对非组件共享资源使用 `entt::hashed_string`：
- `"dispatcher"_hs` — `entt::dispatcher` 访问
- `"spatial_index"_hs` — `SpatialIndexManager` 读写
- `"input"_hs` — `InputManager` 读取
- `"camera"_hs` — `Camera` 状态写入
- `"game_time_ctx"_hs` — `GameTime` registry context

### 1.3 依赖图构建（entt::flow）

```cpp
entt::flow builder;
for (size_t i = 0; i < decls.size(); ++i) {
    builder.bind(static_cast<entt::id_type>(i));
    for (auto r : decls[i].ro_resources) builder.ro(r);
    for (auto r : decls[i].rw_resources) builder.rw(r);
    if (decls[i].sync_point) builder.sync();
}
auto matrix = builder.graph(); // adjacency_matrix<directed_tag>
```

`entt::flow` 内部算法：
1. `setup_graph` — 从资源冲突构建原始边（RW→RW 串行、RW→RO 串行、RO 可并行）
2. `transitive_closure` — Floyd-Warshall 传递闭包
3. `transitive_reduction` — 消除冗余边，保留最小依赖集

### 1.4 Wave 提取（BFS 拓扑排序 + 层级分组）

```cpp
// 对 adjacency_matrix 做 BFS 分层
std::vector<Wave> extractWaves(const entt::adjacency_matrix<entt::directed_tag>& matrix) {
    const auto n = matrix.size();
    std::vector<size_t> in_degree(n, 0);
    for (auto [from, to] : matrix.edges()) { ++in_degree[to]; }

    std::vector<Wave> waves;
    std::vector<size_t> current_wave;
    for (size_t v = 0; v < n; ++v) {
        if (in_degree[v] == 0) current_wave.push_back(v);
    }

    while (!current_wave.empty()) {
        waves.push_back({current_wave});
        std::vector<size_t> next_wave;
        for (auto v : current_wave) {
            for (auto [from, to] : matrix.out_edges(v)) {
                if (--in_degree[to] == 0) next_wave.push_back(to);
            }
        }
        current_wave = std::move(next_wave);
    }
    return waves;
}
```

同一 Wave 内的系统无依赖边，可并行执行。

### 1.5 DeferredCommands（延迟命令缓冲）

并行系统中不安全的 ECS 结构性修改（add/remove component、create/destroy entity）推迟到 Wave barrier 后在主线程执行：

```cpp
// src/engine/system/deferred_commands.h
class DeferredCommands {
public:
    // 线程安全：多个并行系统可并发调用
    template<typename Tag>
    void emplaceTag(entt::entity entity);

    template<typename Tag>
    void removeTag(entt::entity entity);

    // Wave barrier 后在主线程调用
    void drain(entt::registry& registry);

    [[nodiscard]] bool empty() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::function<void(entt::registry&)>> commands_;
};
```

**为什么需要 DeferredCommands**：`entt::registry` 的 `emplace`/`remove` 操作会修改 sparse_set 内部数据结构（可能触发扩容或元素移动），即使操作不同实体也不是线程安全的。

### 1.6 并行执行

串行与并行路径统一使用 `DeferredCommands`，每个 Wave 结束后 drain：

```cpp
void ParallelWaveScheduler::execute(entt::registry& registry) {
    for (auto& wave : waves_) {
        DeferredCommands deferred;

        if (wave.tasks.size() == 1) {
            // 单任务 wave：直接在主线程执行，避免线程池提交开销
            wave.tasks[0].run(deferred);
        } else {
            // 多任务 wave：并行执行
            std::latch done(wave.tasks.size());
            for (auto& task : wave.tasks) {
                thread_pool_.submit([&task, &deferred, &done] {
                    task.run(deferred);
                    done.count_down();
                });
            }
            done.wait();
        }

        deferred.drain(registry);  // 统一 drain，串行/并行同一路径
    }
}
```

### 1.7 DOT 可视化（调试与学习）

```cpp
std::ostringstream dot_output;
entt::dot(dot_output, matrix, [&decls](auto& out, auto vertex) {
    out << "label=\"" << decls[vertex].name << "\",shape=\"box\"";
});
// 输出到文件或 ImGui 调试面板
```

生成的 DOT 图可以用 Graphviz 渲染，直观查看系统间的依赖关系。

## 2. 系统依赖矩阵

### 2.1 完整资源声明（Exploration 模式 22 个系统）

| # | 系统 | RO 资源 | RW 资源 | 备注 |
|---|------|---------|---------|------|
| 0 | RemoveEntity | — | `NeedRemoveTag`, `registry`(destroy) | **sync_point**：销毁实体影响所有后续系统 |
| 1 | TransitionUpdatePre | — | `dispatcher`, `MapTransition` | Gate 逻辑，特殊处理 |
| 2 | LightTogglePre | `game_time_ctx` | `PointLightComponent`, `LightDisabledTag`, `dispatcher` | |
| 3 | Time | — | `game_time_ctx`, `dispatcher` | 写 GameTime context；emit HourChanged/DayChanged |
| 4 | DayNight | `game_time_ctx` | `GlobalLightingState` | 读 GameTime，写光照状态 |
| 5 | PlayerControl | `input`, `game_time_ctx` | `VelocityComponent`, `StateComponent`, `StateDirtyTag`, `TargetComponent`, `ActorComponent`, `InvisibleTag`, `ActionLockedTag`, `dispatcher` | 重度系统；读输入，写多组件，emit 事件 |
| 6 | NPCWander | `TransformComponent` | `VelocityComponent`, `WanderComponent`, `StateComponent`, `StateDirtyTag` | **不用 dispatcher**；操作 NPC 实体集 |
| 7 | AnimalBehavior | `game_time_ctx` | `VelocityComponent`, `SleepRoutine`, `AnimalBehaviorState`, `WanderComponent`, `StateComponent`, `StateDirtyTag` | **不用 dispatcher**；操作 Animal 实体集 |
| 8 | Chest | `MapId`, `AnimationComponent` | `ChestComponent`, `NeedRemoveTag`, `dispatcher` | |
| 9 | ItemUse | `InventoryComponent`, `ItemStack` | `dispatcher` | |
| 10 | Dialogue | `TransformComponent`, `NameComponent` | `DialogueComponent`, `StateDirtyTag`, `dispatcher` | |
| 11 | ActionSound | `StateComponent`, `TransformComponent` | `ActionSoundComponent`, `dispatcher` | |
| 12 | AutoTile | `TransformComponent` | `AutoTileComponent`, `AutoTileDirtyTag`, `SpriteComponent`, `dispatcher`, `spatial_index` | |
| 13 | State | `AnimationComponent` | `StateComponent`, `StateDirtyTag`, `ActionLockedTag`, `dispatcher` | |
| 14 | Movement | `VelocityComponent`, `AABBCollider`, `CircleCollider` | `TransformComponent`, `TransformDirtyTag`, `spatial_index` | **不用 dispatcher** |
| 15 | TransitionUpdatePost | — | `dispatcher`, `MapTransition` | Gate 逻辑 |
| 16 | LightTogglePost | `game_time_ctx` | `PointLightComponent`, `LightDisabledTag`, `dispatcher` | |
| 17 | SpatialIndex | `TransformDirtyTag`, `TransformComponent`, `AABBCollider`, `CircleCollider` | `spatial_index` | **不用 dispatcher**；消费 TransformDirtyTag |
| 18 | Pickup | `ScatterMotionComponent`, `PickupComponent`, `MapId`, `CircleCollider` | `TransformComponent`, `TransformDirtyTag`, `NeedRemoveTag`, `dispatcher` | |
| 19 | Interaction | `DialogueComponent`, `ChestComponent`, `MapId`, `TransformComponent` | `dispatcher`, `spatial_index`, `input` | |
| 20 | CameraFollow | `PlayerTag`, `TransformComponent`, `input` | `camera` | **不用 dispatcher** |
| 21 | Animation | `AnimationComponent` | `SpriteComponent`, `dispatcher` | emit AnimationEvent/AnimationFinishedEvent |

### 2.2 并行组识别

基于依赖矩阵，以下系统组在 `entt::flow` 生成的 DAG 中无边相连，可安排到同一 Wave：

**组 B：NPCWander ∥ AnimalBehavior**
- 都不访问 `dispatcher`。
- 共享写入组件类型（`VelocityComponent`, `StateComponent`, `StateDirtyTag`），但操作**不相交的实体集合**：NPC 实体有 `WanderComponent` 无 `AnimalBehaviorState`，Animal 实体有 `AnimalBehaviorState`。
- `entt::flow` 会因为相同 RW 资源 ID 而认为它们冲突。**解决方案**：为 NPC 实体集和 Animal 实体集的共享组件使用**分域资源 ID**（如 `"npc:Velocity"_hs` vs `"animal:Velocity"_hs`），反映实际不相交的数据访问。
- `StateDirtyTag` 的 `emplace_or_replace` 通过 `DeferredCommands&` 延迟执行。

**组 D：SpatialIndex ∥ CameraFollow**
- 无共享写组件：SpatialIndex 写 `spatial_index`，CameraFollow 写 `camera`。
- 都不访问 `dispatcher`。
- SpatialIndex 的 `remove<TransformDirtyTag>` 通过 `DeferredCommands&` 延迟执行。

**受限系统（不适合并行）**：
- 大量系统访问 `dispatcher`（视为 RW 共享资源），自然串行化。
- `PlayerControl` 写多种组件且 emit 事件，形成大量依赖边。
- `RemoveEntity` 销毁实体，标记为 `sync_point`。

### 2.3 预期 Wave 布局（Exploration 模式）

```
Wave 0: [RemoveEntity]                    ← sync_point, 独立 wave
  ── Gate1 check ──
Wave 1: [Time]                            ← 写 game_time_ctx + dispatcher
Wave 2: [DayNight, PlayerControl]         ← DayNight(RW:GlobalLighting) ∥ PlayerControl(RW:dispatcher)
                                             注：都 RO game_time_ctx；但 PlayerControl RW dispatcher,
                                             DayNight 不用 dispatcher → 无冲突 → 可并行
Wave 3: [NPCWander ∥ AnimalBehavior]      ← 分域资源，真正并行 + DeferredCommands
Wave 4: [Chest, ItemUse, Dialogue, ActionSound]  ← 都 RW dispatcher → 串行化为同一 wave 内序列
                                                    或拆为多 wave
Wave 5: [AutoTile]
Wave 6: [State]
Wave 7: [Movement]
  ── [TransitionUpdatePost, LightTogglePost] ──
  ── Gate2 check ──
Wave 8: [SpatialIndex ∥ CameraFollow]     ← 无共享写 → 可并行
Wave 9: [Pickup]
Wave 10: [Interaction]
Wave 11: [Animation]
```

> 注：实际 Wave 分配由 `entt::flow` 自动计算，上表为根据依赖矩阵的预期结果。

## 3. 需要新增的文件

| 文件 | 职责 |
|------|------|
| `src/engine/system/system_task_decl.h` | `SystemTaskDecl` 结构体定义 |
| `src/engine/system/deferred_commands.h` | `DeferredCommands` 线程安全命令缓冲 |
| `src/engine/system/parallel_wave_scheduler.h` | `ParallelWaveScheduler` 类声明 |
| `src/engine/system/parallel_wave_scheduler.cpp` | 实现：entt::flow 构建、Wave 提取、并行执行 |
| `tests/engine/parallel_wave_scheduler_test.cpp` | 图构建、Wave 提取、并行执行、DeferredCommands 测试 |

## 预计修改的文件

| 文件 | 改动内容 |
|------|----------|
| `src/game/runtime/system_scheduler.h` | 持有 `ParallelWaveScheduler`；`tick()` 统一走 wave 调度 |
| `src/game/runtime/system_scheduler.cpp` | 构建 `SystemTaskDecl[]`；所有系统的 `run` lambda 传递 `DeferredCommands&` |
| `src/game/system/npc_wander_system.h` / `.cpp` | `update(float dt)` → `update(float dt, DeferredCommands& deferred)` |
| `src/game/system/animal_behavior_system.h` / `.cpp` | 同上 |
| `src/engine/system/spatial_index_system.h` / `.cpp` | `update(registry)` → `update(registry, DeferredCommands& deferred)` |
| 所有其他含 tag emplace/remove 的系统 | 签名统一加 `DeferredCommands&`，结构性修改走 deferred |
| `tests/CMakeLists.txt` | 添加新测试 |

## 4. 实现步骤（拆分执行）

### Step 1：DeferredCommands 基础设施
- 实现 `DeferredCommands` 类：
  - `emplaceTag<Tag>(entity)` — 收集 `registry.emplace_or_replace<Tag>(entity)` 操作
  - `removeTag<Tag>(entity)` — 收集 `registry.remove<Tag>(entity)` 操作
  - `drain(registry)` — 在主线程按收集顺序执行所有命令
  - 线程安全：内部 `std::mutex` 保护 commands 容器
- 单元测试：多线程并发 emplace/remove + drain 正确性

### Step 2：SystemTaskDecl 与资源 ID 体系
- 定义 `SystemTaskDecl` 结构体
- 为所有组件和共享资源建立 `entt::id_type` 常量（使用 `type_hash` 和 `hashed_string`）
- 定义分域资源 ID 方案（NPC vs Animal 共享组件）
- 单元测试：验证资源 ID 唯一性

### Step 3：entt::flow 依赖图构建
- 实现 `TaskGraphBuilder`：从 `SystemTaskDecl[]` 构建 `entt::flow` 并生成 `adjacency_matrix`
- 实现 DOT 导出函数：使用 `entt::dot()` 带系统名称标签
- 单元测试：
  1. 3 个无依赖系统 → 无边
  2. A(RW:X) → B(RO:X) → 有边 A→B
  3. A(RO:X) ∥ B(RO:X) → 无边（只读可并行）
  4. sync_point → 所有后续系统依赖它
  5. 完整 22 系统声明 → 验证 DOT 输出可解析

### Step 4：Wave 提取算法
- 实现 BFS 拓扑排序 + 层级分组，产出 `std::vector<Wave>`
- 每个 `Wave` 包含该层可并行的系统索引列表
- 单元测试：
  1. 线性链（A→B→C）→ 3 个 wave，每 wave 1 个系统
  2. 全独立（A, B, C）→ 1 个 wave，3 个系统
  3. 菱形 DAG（A→B, A→C, B→D, C→D）→ 3 个 wave: [A], [B,C], [D]
  4. sync_point 在中间 → 强制分割 wave

### Step 5：ParallelWaveScheduler 核心
- 接收 `std::vector<SystemTaskDecl>` + `ThreadPool&`
- 初始化时构建图、提取 waves
- `execute(registry)` 按 wave 顺序执行（串行与并行统一路径）：
  - 每个 wave 创建 `DeferredCommands deferred`
  - 单任务 wave → 主线程直接调用 `run(deferred)`，避免线程池开销
  - 多任务 wave → `ThreadPool::submit()` + `std::latch` barrier
  - wave 结束后统一 `deferred.drain(registry)`
- 提供 `dumpDot()` 方法输出 DOT 图
- 单元测试：
  1. 模拟 3 个独立系统 → 验证确实并发执行（使用 `std::atomic` 计数器 + timing）
  2. 有依赖的系统 → 验证执行顺序约束
  3. DeferredCommands 在 drain 后正确生效

### Step 6：系统签名改造
- 所有涉及 tag emplace/remove 的系统，签名统一加 `DeferredCommands&`：
  ```cpp
  // Before
  void NPCWanderSystem::update(float dt);
  // After
  void NPCWanderSystem::update(float dt, DeferredCommands& deferred);
  ```
- 系统内所有结构性修改（`emplace_or_replace`、`remove`）改为 `deferred.emplaceTag<T>(entity)` / `deferred.removeTag<T>(entity)`
- 不涉及 tag 操作的系统（如 `TimeSystem`、`CameraFollowSystem`）也统一接受 `DeferredCommands&`，保持调用接口一致（内部不使用即可）
- 同步修改 `execute_stage()` 及所有调用点
- 单元测试：改造后系统行为与改造前一致（通过 registry 最终状态比对）

### Step 7：集成到 SystemScheduler
- `SystemScheduler` 持有 `ParallelWaveScheduler`（lazy 初始化）
- 新增 `buildTaskDecls()` 方法：为 22 个系统构建 `SystemTaskDecl[]`
  - 每个系统的 `run` lambda 捕获 `GameSystemBundle` 引用，调用 `system->update(..., deferred)`
  - 使用 2.1 节的资源声明
- `tick()` 统一走 wave 调度路径（不再区分 parallel/sequential 开关）：
  - 需要并行的 wave 走线程池
  - 单任务 wave 自动退化为主线程串行（由 ParallelWaveScheduler 内部处理）
- Gate 逻辑处理：
  1. `RemoveEntity`（sync_point wave，独立执行）
  2. Gate1 check → 若 transition active 则只执行 TransitionUpdatePre + LightTogglePre 后 return
  3. 执行 Wave 1..N（Time 到 Movement 段）
  4. `TransitionUpdatePost` / `LightTogglePost`
  5. Gate2 check → 若 transition active 则 return
  6. 执行 Wave M..K（SpatialIndex 到 Animation 段）

### Step 8：entt::organizer 教学演示（可选）
- 选取 2-3 个签名简单的系统（如虚构示例系统），演示 `entt::organizer` 用法：
  ```cpp
  // 示例：organizer 自动推导依赖
  void update_time(const GameTimeCtx& time) { /* ... */ }
  void update_daynight(const GameTimeCtx& time, GlobalLightingState& light) { /* ... */ }

  entt::organizer organizer;
  organizer.emplace<&update_time>("Time");
  organizer.emplace<&update_daynight>("DayNight");
  auto graph = organizer.graph(); // 自动推导 Time → DayNight
  ```
- 对比 organizer 自动图 vs 手动 flow 图的异同
- 说明 organizer 的限制：要求函数参数为 `registry&` / `view<>` / context 类型，不适合现有系统的异构签名

### Step 9：测试与 TSAN 验证
- 完整 `parallel_wave_scheduler_test.cpp`：
  1. 图构建正确性（线性/分叉/菱形/sync_point）
  2. Wave 提取正确性
  3. 并行执行：验证 wave 内系统确实并发（atomic 计数器 + sleep + timing check）
  4. DeferredCommands drain 顺序与正确性
  5. 单系统 wave 不经线程池
- `build-tsan` 回归：
  - `parallel_wave_scheduler_test` 无 data race
  - 游戏主循环 tick 无 data race
- 功能回归：并行模式 vs 串行模式运行相同输入序列，验证最终 registry 状态一致

## 5. entt::flow 关键 API 速查

```cpp
// 构建
entt::flow builder;
builder.bind(task_id)        // 绑定当前任务
       .ro(resource_id)      // 声明只读资源
       .rw(resource_id)      // 声明读写资源
       .sync();              // 标记为全局同步点

// 生成图（内部: setup_graph → transitive_closure → transitive_reduction）
auto matrix = builder.graph();  // adjacency_matrix<directed_tag>

// 查询
matrix.size();                  // 顶点数
matrix.contains(from, to);      // 是否有边
matrix.edges();                 // 所有边 [(from,to), ...]
matrix.out_edges(vertex);       // 出边
matrix.in_edges(vertex);        // 入边
matrix.vertices();              // 所有顶点

// 可视化
std::ostringstream oss;
entt::dot(oss, matrix, [&](auto& out, auto v) {
    out << "label=\"" << names[v] << "\"";
});
```

## 6. 待办清单（用于后续追踪）

- [ ] T1 实现 `DeferredCommands`（线程安全命令缓冲 + 单元测试）
- [ ] T2 定义 `SystemTaskDecl` 结构体与资源 ID 常量
- [ ] T3 实现 `TaskGraphBuilder`：`entt::flow` 封装 + DOT 导出
- [ ] T4 实现 Wave 提取算法（BFS 拓扑排序 + 层级分组）
- [ ] T5 实现 `ParallelWaveScheduler`（ThreadPool + latch barrier + 统一 DeferredCommands drain）
- [ ] T6 为 22 个系统声明资源依赖（`buildTaskDecls()`）
- [ ] T7 改造所有系统签名：统一接受 `DeferredCommands&`，结构性修改走 deferred
- [ ] T8 集成到 `SystemScheduler`（统一 wave 调度 + Gate 处理）
- [ ] T9 DOT 可视化验证依赖图合理性
- [ ] T10 `parallel_wave_scheduler_test.cpp`（图构建 + Wave 提取 + 并行执行 5 组用例）
- [ ] T11 TSAN 回归通过
- [ ] T12 功能回归：改造后 registry 最终状态与改造前一致
- [ ] T13 entt::organizer 教学演示（可选）

## 7. 疑问与待澄清
- 暂无。按此计划可直接进入实现。
