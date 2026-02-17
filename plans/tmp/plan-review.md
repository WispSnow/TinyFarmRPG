# FND-004 计划审阅

## 审阅结论：通过，有 3 项建议修改

计划整体目标清晰、范围收敛，与现有代码结构适配良好。以下逐项评审。

---

## 一、计划与现有代码的一致性验证

### 1.1 基线描述准确性 — 准确

- `on_stage_executed` hook 确实存在于 `TickParams`，当前 `GameScene::update()` 未传入（默认 `{}`）。
- `system_scheduler.cpp` 无任何 `std::chrono` / 计时代码。
- 现有 Game Debug 面板确实是 6 个（Player / GameTime / Inventory / SaveLoad / MapInspector / BlueprintInspector），无 scheduler 相关。
- `clean()` 中 `unregisterPanels(PanelCategory::Game)` 可覆盖新面板的生命周期。

### 1.2 TickParams 扩展方案可行性 — 可行但有更优方案（见建议 P1）

计划提议在 `TickParams` 中增加 `SchedulerProfiler*` 指针。当前 `TickParams` 已有 `on_stage_executed` 回调，这是一个天然的采样注入点。

### 1.3 新增文件与 CMake 注册 — 正确

- `scheduler_profiler.h/.cpp` 放 `game/runtime/` 合理（与 `system_scheduler` 同层）。
- `scheduler_debug_panel.h/.cpp` 放 `game/debug/` 合理（与其他 panel 同层）。
- `src/CMakeLists.txt` 中 runtime 源文件在无条件编译段，debug panel 在 `if(ENABLE_DEBUG_UI)` 段，计划未明确区分但从"SchedulerProfiler 默认关闭"语义推断应放无条件段。

---

## 二、具体建议

### P1（Important）：不需要扩展 TickParams — 利用已有 `on_stage_executed` 即可

**现状**：`TickParams` 已有 `std::function<void(SchedulerStage)> on_stage_executed{}`，在每个 `execute_stage()` 之前调用。`GameScene::update()` 当前未传入。

**计划方案**：在 `TickParams` 新增 `SchedulerProfiler*` 指针，让 scheduler 内部做计时。

**问题**：
1. 这让 `SystemScheduler`（一个纯调度器）知道了 `SchedulerProfiler` 的存在，产生耦合。
2. scheduler 内部需要在每个 `execute_stage` 前后分别调用 profiler，增加了 `tick()` 内部的条件分支复杂度。
3. 与已有 `on_stage_executed` 功能重叠。

**建议方案**：
- 不改 `TickParams`，不改 `system_scheduler.h/.cpp`。
- 将 `on_stage_executed` 的签名从 `void(SchedulerStage)` 改为 `void(SchedulerStage, bool before)` 或使用两个回调（`on_before_stage` / `on_after_stage`）。
- 但**最简方案**：保持 `on_stage_executed` 不变（它在 `execute_stage` **之前**调用），在 `GameScene::update()` 中通过 lambda 利用 `SchedulerProfiler` 做计时：

```cpp
// GameScene::update() 中：
auto profiler_hook = [&](SchedulerStage stage) {
    profiler_->endStage();    // 结束上一个 stage
    profiler_->beginStage(stage); // 开始新 stage
};
scheduler_->tick({..., profiler_hook, ...});
profiler_->endFrame(); // 帧结束
```

但这有个问题：`on_stage_executed` 在 `execute_stage` 之前（`trace_stage` 调用），无法捕获 stage 执行后的时间。

**实际最优方案**：将现有 `trace_stage` 调整为 before/after 双调用模式。具体做法：

```cpp
// system_scheduler.cpp execute_stage 改为：
void execute_stage(const TickParams& params, SchedulerStage stage) {
    if (params.on_stage_executed) params.on_stage_executed(stage);  // before
    // ... switch dispatch ...
    if (params.on_stage_completed) params.on_stage_completed(stage);  // after
}
```

在 `TickParams` 增加一个 `on_stage_completed` 回调（而非 profiler 指针），保持 scheduler 对 profiler 无感知。`SchedulerProfiler` 逻辑全部在 `GameScene` 侧注入。

**这比计划方案更符合 code-guide 的"精简 + 最优"原则**：scheduler 保持纯调度、无计时逻辑；profiler 完全在 GameScene 侧组装。

如果觉得增加回调也有代价，退一步的折中方案：直接接受计划的 `SchedulerProfiler*` 注入，但这不是最优。

### P2（Important）：`SchedulerProfiler` 应该整体在 `TF_ENABLE_DEBUG_UI` 下编译保护

**计划描述**："采样默认关闭，由 debug panel 显式开启；关闭时不做时钟采样"。

**问题**：计划将 `scheduler_profiler.cpp` 放在无条件编译段（runtime 源文件与 `system_scheduler.cpp` 同列），但 `SchedulerProfiler` 的唯一消费方是 debug panel。

- 如果 `SchedulerProfiler` 在非 debug 构建中也存在，则 GameScene 需要无条件持有它并在 `tick` 中注入，引入不必要的运行时对象。
- 如果改为在 `#ifdef TF_ENABLE_DEBUG_UI` 下编译，则与所有 panel 一样干净——非 debug 构建零开销。

**建议**：
- `scheduler_profiler.h/.cpp` 放到 `src/game/debug/` 目录下（而非 `game/runtime/`），在 `if(ENABLE_DEBUG_UI)` 段编译。
- `GameScene` 中 profiler 的创建、持有和 tick 注入全部在 `#ifdef TF_ENABLE_DEBUG_UI` 下。
- 这样非 debug 构建完全零开销，不需要"运行时开关"。

### P3（Minor）：trace 日志的必要性待商榷

**计划描述**："双开关"设计——`capture_enabled` 控制 profiler 采样，`trace_enabled` 控制 spdlog trace 日志输出，两者独立。

**问题**：
- 如果采用 P1 建议（scheduler 不感知 profiler），trace 日志就不在 scheduler 内部输出，而是在 profiler 自己的 `endFrame()` 中输出。
- 即便保持计划方案，trace 日志在每帧输出 22+ 个 stage 的耗时信息，容量很大，而实际调试场景下 debug panel 的可视化已经足够。
- spdlog 当前未在 `system_scheduler.cpp` 中引入（无 `#include <spdlog/spdlog.h>`），增加此依赖只为 trace 输出，性价比低。

**建议**：首版不做 trace 日志输出，profiler 数据通过 debug panel 消费。如果后续需要文件级记录，可在 profiler 内增加 dump 功能。这样 scheduler 保持零新依赖。

---

## 三、其他确认项（无问题）

| 检查项 | 结论 |
|---|---|
| `SchedulerProfiler` 环形缓冲设计 | 合理，120 帧容量足够观察抖动 |
| Panel UI 内容（mode/gate/耗时表/平均值） | 实用，覆盖了调试所需信息 |
| Panel 控件（Capture 开关/帧数/Clear） | 简洁够用 |
| 测试计划（3 个测试用例） | 覆盖核心行为：开/关/环形覆盖 |
| 既有测试回归列表 | 完整，包含 3 个 scheduler 测试 |
| 新增文件列表 | 完整（5 个新文件 + 6 个改动文件） |
| 风险缓解与回滚策略 | 合理 |

---

## 四、建议采纳后的改动总结

| 项 | 计划方案 | 建议调整 |
|---|---|---|
| TickParams | 新增 `SchedulerProfiler*` | 新增 `on_stage_completed` 回调（或保持计划方案作为折中） |
| scheduler 内部 | 增加计时 + trace 日志 | 不改或仅加 `on_stage_completed` 调用 |
| profiler 文件位置 | `game/runtime/` 无条件编译 | `game/debug/` 在 `ENABLE_DEBUG_UI` 下编译 |
| trace 日志 | scheduler 内 spdlog trace 输出 | 首版不做，panel 可视化已足够 |
| profiler 注入方式 | scheduler 内部按需调用 profiler | GameScene 通过 on_stage_executed/completed 回调注入 |

---

## 五、结论

计划可执行，建议优先采纳 **P1**（scheduler 保持无 profiler 耦合）和 **P2**（profiler 整体在 debug guard 下编译）。P3 为可选优化。采纳后计划的实际改动文件可能减少（`system_scheduler.h/.cpp` 改动最小化或仅增加一个回调）。
