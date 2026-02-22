# TinyFarmRPG 多线程改造索引（2026-02-21）

## 目标
- 在不破坏主线程边界的前提下，把 CPU/IO 重任务迁移到后台，降低卡顿并保持行为确定性。

## 核心模型

```
Worker 线程                      主线程
┌────────────────────┐         ┌─────────────────────────────────┐
│ 文件 IO/JSON 解析   │         │ SDL 事件采样                    │
│ 图片/字体 CPU 预处理 │──入队→ │ registry/dispatcher/GL/ImGui    │
│ 存档序列化与写盘     │         │ updateFrame -> drain -> render  │
└────────────────────┘         └─────────────────────────────────┘
```

## 全局硬约束
- `SDL_PollEvent()` 仅主线程调用。
- OpenGL 调用必须在上下文绑定线程（主线程）。
- `entt::registry` 与 `entt::dispatcher` 不做并发读写。
- 主线程命令只在 `GameApp` 提交点 `updateFrame -> drainMainThreadCommands -> render` 执行。

## 阶段索引

| 阶段 | 主题 | 优先级 | 状态 | 详细计划 |
|------|------|--------|------|----------|
| Phase 1 | 异步加载预处理 | P0 | **Completed** | `./phase1-multithreading-plan.md` |
| Phase 2 | 主线程提交点标准化 | P0 | **Completed** | `./phase2-main-thread-commit-plan.md` |
| Phase 3 | AsyncPreloadPipeline 解耦 | P0 | **Completed** | `./phase3-refactor-keep-architecture-and-pipeline-plan.md` |
| Phase 4 | 后台存档 I/O | P1 | **Completed** | `./phase4-background-save-io-plan.md` |
| Phase 5 | ECS 系统级并行调度（修订版） | P2（学习驱动） | **Planned (Revised 2026-02-22)** | `./phase5-ecs-task-parallel-deferred-plan.md` |
| Phase 6 | 逻辑/渲染分离 | Optional/不推荐 | Deferred | `./phase6-logic-render-split-deferred-plan.md` |

## Phase 5 修订同步（2026-02-22）
- 执行策略从“全量系统并行化”收敛为“并行岛（Parallel Island）”最小闭环：先落地 `SpatialIndex ∥ CameraFollow`。
- `NPCWander ∥ AnimalBehavior` 因当前实体域重叠，改为 Deferred Backlog，待组件域重构后再评估并行。
- `dispatcher` 明确为主线程专用，worker 线程禁止直接访问（`sink/trigger/enqueue/update` 均不进入 worker）。
- 调度观测改为主线程聚合模型（`TickTrace`），不再依赖 worker 线程直接回调 profiler。
- 保留 Gate1/Gate2 语义与主线程边界，新增环检测、任务完整性校验、`submit` 失败回退防死锁。

## 观测与验证
- 指标与回归策略见 `./multi-thread-observability.md`。
