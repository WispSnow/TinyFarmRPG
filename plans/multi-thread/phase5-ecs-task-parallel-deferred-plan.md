# 2026-02-22 Phase 5 ECS Task 并行计划（Deferred）

## 元信息
- 阶段：`Phase 5`
- 主题：`SystemScheduler 的依赖感知并行化`
- 优先级：`Optional`
- 状态：`Deferred`
- 范围边界：
  - 当前阶段不执行，只定义触发条件与最小可行落地路径。
  - 不在没有明确性能证据时引入复杂并行框架。

## 1. 实现思路（最优方案）

采用 **“先度量、后并行、分批 barrier 合并写入”**：

1. 先建立可重复的 `SystemScheduler::tick()` profiling 基线，确认主线程瓶颈来自 ECS 更新而非 IO/渲染。
2. 仅对“只读或弱耦合”系统组试点并行，写操作通过延迟写入/命令缓冲在 barrier 后合并。
3. 调度层采用显式依赖图（`entt::organizer` 或等价 task graph），不做“裸线程直接并行跑系统”。

不直接执行的原因：
- 当前 `entt` 写操作天然共享存储，线程安全边界复杂。
- 现有实体规模（数十到低百）下，线程调度开销大概率抵消收益。

## 2. 需要新增的文件（触发实施后）

- `src/game/system/system_scheduler_task_graph.h`
- `src/game/system/system_scheduler_task_graph.cpp`
- `tests/game/system_scheduler_parallel_graph_test.cpp`

## 预计修改的文件（触发实施后）

- `src/game/system/system_scheduler.h`
- `src/game/system/system_scheduler.cpp`
- `src/game/debug/scheduler_profiler.h`
- `src/game/debug/scheduler_profiler.cpp`

## 3. 实现步骤（触发后执行）

### Step 1：建立触发门槛与基线
- 固定测试地图与实体规模，记录 `tick` 的 P50/P95/P99。
- 满足触发条件后才进入并行改造。

### Step 2：依赖图建模
- 为每个系统声明读写组件集合，形成 DAG。
- 将强依赖链（例如 `Movement -> SpatialIndex`）保持串行。

### Step 3：并行试点
- 首先并行只读系统组，观察性能收益和一致性。
- 再引入弱耦合写系统，使用 barrier 后统一提交写入。

### Step 4：回归验收
- 功能一致性回归（状态机、碰撞、输入响应）。
- TSAN 与性能回归同时通过后再扩大并行范围。

## 4. 待办清单（用于后续追踪）

- [ ] T1 建立 `SystemScheduler::tick()` 稳定 profiling baseline
- [ ] T2 定义并确认 Phase 5 启动阈值（性能与实体规模）
- [ ] T3 产出系统读写依赖矩阵与 DAG
- [ ] T4 完成只读系统并行试点
- [ ] T5 完成弱耦合写系统并行试点（含 barrier 合并）
- [ ] T6 新增并行调度测试与 TSAN 回归
- [ ] T7 对比改造前后 P50/P95 与超时帧占比

## 5. 疑问与待澄清
- 暂无。保持 Deferred，等待触发条件满足后再进入实施。
