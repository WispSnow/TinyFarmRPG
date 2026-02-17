# FND-004 Code Review

## 总览

| 项目 | 内容 |
|---|---|
| 计划 | `plans/foundation/FND-004.md` |
| 变更规模 | 12 个文件（7 modified + 5 new），+81 行改动（已跟踪文件）+ ~590 行新文件 |
| 编译 | 通过 |
| 测试 | 150/150 通过（3 audio skip） |

**审阅结论：通过，有 1 项 Important 和 2 项 Minor。**

---

## 一、变更清单

### 新增文件
| 文件 | 行数 | 说明 |
|---|---|---|
| `src/game/debug/scheduler_profiler.h` | 72 | SchedulerProfiler 类：帧样本、环形缓冲、阶段聚合查询 |
| `src/game/debug/scheduler_profiler.cpp` | 228 | 完整实现：beginFrame/onStageStarted/onStageCompleted/endFrame + 查询 + trace 输出 |
| `src/game/debug/scheduler_debug_panel.h` | 31 | SchedulerDebugPanel 类声明 |
| `src/game/debug/scheduler_debug_panel.cpp` | 137 | ImGui 面板：mode / capture 开关 / 最新帧耗时表 / 近 N 帧聚合表 |
| `tests/game/system_scheduler_profiler_test.cpp` | 124 | 4 个测试用例 |

### 改动文件
| 文件 | 变更 |
|---|---|
| `src/game/runtime/system_scheduler.h` | +2 行：`on_stage_started` / `on_stage_completed` 回调 |
| `src/game/runtime/system_scheduler.cpp` | +7 行：`trace_stage` 调 `on_stage_started`，`execute_stage` 末尾调 `on_stage_completed` |
| `src/game/scene/game_scene.h` | +7 行：forward decl + `#ifdef TF_ENABLE_DEBUG_UI` 下 profiler 成员 |
| `src/game/scene/game_scene.cpp` | +50/-5：profiler 创建、tick 回调接线、panel 注册 |
| `src/CMakeLists.txt` | +2 行：profiler + panel 在 `if(ENABLE_DEBUG_UI)` 段 |
| `tests/CMakeLists.txt` | +6 行：profiler test 在 `if(ENABLE_DEBUG_UI)` 段 |
| `plans/foundation/FND-004.md` | 状态更新为 Done + 进度日志 |

---

## 二、架构与设计审查

### 2.1 审阅建议采纳情况

| 审阅建议 | 采纳情况 |
|---|---|
| P1：不注入 `SchedulerProfiler*` 到 TickParams，改用回调 | **采纳** — 增加了 `on_stage_started` / `on_stage_completed` 回调，scheduler 对 profiler 完全无感知 |
| P2：profiler 放 `game/debug/`，在 `ENABLE_DEBUG_UI` 下编译 | **采纳** — 所有新文件都在 `game/debug/`，CMake 条件编译正确 |
| P3：首版不做 trace 日志 | **部分采纳** — trace 输出实现了但由 `emit_trace` 参数控制，仅在 profiler 启用 + spdlog trace 级别时才输出。可以接受。|

### 2.2 Scheduler 回调放置位置 — 正确

- `on_stage_started`：在 `trace_stage()` 中，`on_stage_executed` 之前调用 → 在 stage dispatch 之前
- `on_stage_completed`：在 `execute_stage()` 末尾，switch 之后调用 → 在 stage dispatch 之后
- 回调与实际 stage 执行精确包裹，计时准确

### 2.3 环形缓冲设计 — 正确

- `frames_` 预分配固定大小（默认 120），`frame_cursor_` 循环写入
- `frame_count_` 被 `std::min(count+1, capacity)` 限制，不溢出
- `latestFrame()` / `recentFrames()` 从 cursor 反向读取，返回顺序正确（最新在前）

### 2.4 零开销路径 — 正确

- 非 debug 构建：`#ifdef TF_ENABLE_DEBUG_UI` 完全排除 profiler 相关代码
- debug 构建但 profiler 未启用：走 `else` 分支的原始 tick 路径，无回调开销
- `SchedulerProfiler` 方法内部有 `if (!enabled_)` 守卫，双重保险

### 2.5 Debug Panel — 实现完备

- mode 显示、Capture 开关、Clear 按钮、Recent Frames 滑块
- 最新帧阶段耗时表（3 列：序号 / Stage / Elapsed）
- 聚合汇总表（4 列：Stage / Avg / Max / Samples）
- 生命周期安全：panel 持有 profiler 引用，profiler 由 GameScene 持有且在 `clean()` 前注销

### 2.6 测试覆盖 — 充分

| 测试 | 覆盖 |
|---|---|
| `StageHooksCanCollectDurations` | 集成测试：scheduler tick + profiler 联动，验证回调顺序与 stage 数量一致 |
| `CaptureOnRecordsStageOrderAndDurations` | 单元测试：手动调用 profiler API，验证 stage 顺序、mode、gate 状态 |
| `CaptureOffKeepsHistoryEmpty` | 守卫测试：disabled 时不记录 |
| `FixedCapacityKeepsRecentFrames` | 环形缓冲测试：3 帧写入容量 2 的 buffer，验证覆盖与顺序 |

---

## 三、问题

### P1（Important）：`gameModeToString` 重复定义

`scheduler_profiler.cpp:17` 和 `scheduler_debug_panel.cpp:10` 各有一个相同的匿名命名空间 `gameModeToString()` 函数。

**问题**：
- 同一逻辑重复两处，后续 `GameMode` 增加新值时需要同步修改两处
- `SchedulerStage` 已有 `toString(SchedulerStage)` 在 `system_scheduler.h` 中作为公共 free function，`GameMode` 应该遵循相同模式

**建议**：在 `game_mode.h` 中增加 `const char* toString(GameMode)` free function（与 `toString(SchedulerStage)` 对称），删除两处匿名命名空间的 `gameModeToString`。

### P2（Minor）：`onStageStarted` 中的 "上一 stage 补录" 逻辑是死代码

`scheduler_profiler.cpp:83-89` — 当 `stage_active_` 为 true 时，`onStageStarted` 会先补录上一个 stage 的耗时。但在正常 scheduler 流程中，`on_stage_completed` 总是在 `on_stage_started` 之前被调用（除了第一个 stage），所以 `stage_active_` 在进入 `onStageStarted` 时总是 false。

这段代码是防御性 fallback，不会在当前架构下触发。按 code-guide "不做过度防御" 原则，可以删除此分支，简化为直接赋值：

```cpp
void SchedulerProfiler::onStageStarted(SchedulerStage stage) {
    if (!enabled_ || !frame_active_) return;
    current_stage_ = stage;
    stage_started_at_ = Clock::now();
    stage_active_ = true;
}
```

不过这不影响正确性，属于风格优化。

### P3（Minor）：`GameScene::update()` lambda 中对 `scheduler_profiler_` 的 null check 多余

`game_scene.cpp:167-168` 和 `172-173`：

```cpp
[this](const game::runtime::SchedulerStage stage) {
    if (scheduler_profiler_) {            // <-- 多余
        scheduler_profiler_->onStageStarted(stage);
    }
}
```

这两个 lambda 只在 `if (scheduler_profiler_ && scheduler_profiler_->isEnabled())` 内部构建并传入 `tick()`，此时 `scheduler_profiler_` 已确认非 null 且在 tick 执行期间不会变为 null（单线程同步调用）。内部 null check 属于过度防御。

可简化为：
```cpp
[this](const game::runtime::SchedulerStage stage) {
    scheduler_profiler_->onStageStarted(stage);
}
```

---

## 四、验证结果

| 检查项 | 结果 |
|---|---|
| `cmake --build build --target game_tests` | 通过 |
| `ctest --test-dir build --output-on-failure -j4` | 150/150 通过（3 audio skip） |
| scheduler 原有 3 个测试（profile/gate/invariant） | 通过，行为无回退 |
| profiler 新增 4 个测试 | 全部通过 |
| `#ifdef TF_ENABLE_DEBUG_UI` 守卫完整性 | 头文件成员、cpp 创建/接线/includes 均在守卫内 |
| CMake 条件编译 | profiler + panel 在 `if(ENABLE_DEBUG_UI)`，test 同样 |
| scheduler 对 profiler 无耦合 | 确认：`system_scheduler.h/.cpp` 无任何 profiler/debug include |

---

## 五、总结

实现忠实遵循了修改后的计划，正确采纳了 P1（回调解耦）和 P2（debug guard 编译保护）两项核心审阅建议。Scheduler 保持纯调度职责不变（仅增加两个通用回调），profiler 与 panel 完全在 debug 编译段内，非 debug 构建零开销。

**P1**（`gameModeToString` 重复）建议在提交前修复，其余两项为可选优化。
