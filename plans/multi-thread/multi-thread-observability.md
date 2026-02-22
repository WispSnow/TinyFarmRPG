# 多线程改造观测与验收指标

## 目标
- 为每个阶段提供统一的性能与并发正确性验收口径，避免“感觉变快/感觉没问题”。

## 核心观测项

| 观测项 | 指标定义 | 适用阶段 |
|------|----------|----------|
| 地图切换耗时 | `loadMap` 端到端耗时 P50/P95 | Phase 1/2/3 |
| 主线程命令提交耗时 | 单帧 `drainMainThreadCommands()` 耗时与执行条数 | Phase 2/3 |
| 存档写盘耗时 | `capture` 耗时、后台 `serialize+write` 耗时、总完成时长 | Phase 4 |
| fixed tick 超时率 | `tick > budget` 帧占比 | Phase 5 触发判断 |
| TSAN 回归结果 | 是否存在 data race/线程安全告警 | 全阶段 |

## 工具与入口
- `SchedulerProfiler`：`src/game/debug/scheduler_profiler.h`
- TSAN 构建：`cmake -S . -B build-tsan -DENABLE_TSAN=ON -DBUILD_TESTING=ON`
- 测试回归：`ctest --test-dir build-tsan --output-on-failure`

## 阶段验收要求
- 每个阶段至少提交一次“改造前/改造后”对比数据。
- 功能测试与 TSAN 必须同时通过，不接受“性能提升但有竞态”。
- 未达到验收标准的阶段不得标记为 `Completed`。
