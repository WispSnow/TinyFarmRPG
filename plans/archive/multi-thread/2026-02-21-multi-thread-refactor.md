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
| Phase 5 | ECS 系统级并行调度（基础） | P1 | **Completed (2026-02-23)** | `./phase5-ecs-task-parallel-deferred-plan.md` |
| Phase 5.5 | ECS 全面并行重构（扩展） | P1 | **Completed (2026-02-23)** | `./phase5.5-ecs-full-parallel-refactor-plan.md` |
| Phase 6 | 逻辑/渲染分离 | Optional/不推荐 | Deferred | `./phase6-logic-render-split-deferred-plan.md` |

## 观测与验证
- 指标与回归策略见 `./multi-thread-observability.md`。