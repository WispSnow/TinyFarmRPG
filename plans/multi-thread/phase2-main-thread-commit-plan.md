# 2026-02-21 Phase 2 主线程提交点标准化开发计划

## 元信息
- 阶段：`Phase 2`
- 主题：`主线程提交点标准化（Main-thread Commit Pipeline）`
- 优先级：`P0`
- 状态：`Completed（实现完成，关键测试与 TSAN 回归通过）`
- 范围边界：不做 gameplay 并行，不改 `SystemScheduler` 并发模型；仅重构 worker -> main 的提交链路。

## 1. 实现思路

基于 `plans/tmp/codex-multi.md` 的 Phase 2 目标，采用精简最优方案：
**“预算化 drain + GameApp 唯一提交权 + MapManager 去私有 drain”**。

本阶段明确不引入 `Envelope/Metrics/lane` 抽象，避免过度工程化。

### 方案对比（收敛）
1. 保持现状（`drain` 分散在业务中）  
   优点：改动最小。  
   缺点：提交时序不可控，边界持续扩散。  

2. 双提交点（fixed 内 + render 前）  
   优点：理论时延更低。  
   缺点：实现/调试复杂，阶段语义易混乱。  

3. 推荐：单提交点标准化（保留 `updateFrame -> drain -> render`）+ 预算化执行  
   优点：时序清晰、可验证、改动量可控。  
   缺点：命令可能延后一帧生效（可接受）。

### 关键设计
- 只有 `GameApp` 可执行 `drainMainThreadCommands()`。
- `MainThreadCommandQueue` 在现有接口上增加 `DrainPolicy` 与 `DrainResult`。
- `MapManager` 仅做状态查询/等待，不再主动 `drain`。
- 保持兼容现有状态枚举，但明确语义：
  - `Running`：worker 执行中，或 worker 已入队主线程命令但尚未由 `GameApp` 提交。
  - `Ready`：主线程命令已执行完成（GPU 上传完成）。
  - `Applied`：该预加载结果已被 `loadMap()` 消费并标记为已应用。

### MapManager 状态机时序（移除私有 drain 后）
1. `preloadMap()` 提交 worker，状态 `Running`。
2. worker 完成 CPU 阶段并入队 main-thread command，状态仍为 `Running`。
3. `GameApp` 在固定提交点 `drainMainThreadCommands()` 执行命令，状态变为 `Ready` 或 `Failed`。
4. `loadMap()` 处理规则：`Ready/Applied` 走异步预热成功路径；`Running` 仅轮询等待至预算上限，超时降级同步加载；`Failed/NotScheduled` 直接同步加载。
5. `loadMap()` 完成后把已消费预热结果标记为 `Applied`。

该语义保证：不会在“GPU 未上传”时把任务误判为 `Ready` 并消费。

## 2. 需要新增的文件

- `tests/engine/async/main_thread_command_queue_budget_test.cpp`  
  校验预算化 drain 的命令执行上限、时间上限与结果统计。

说明：
- 本阶段不新增引擎生产代码文件，集中修改现有：
  - `src/engine/async/main_thread_command_queue.h`
  - `src/engine/async/main_thread_command_queue.cpp`
  - `src/engine/core/game_app.cpp`
  - `src/game/world/map_manager.cpp`
- `GameApp` 顺序校验优先复用并扩展现有 `tests/engine/core/game_app_dispatcher_trace_test.cpp`（源码契约测试），不新增重型 `GameApp::run()` 全链路实例化测试。
- `MapManager` 仍以现有 `tests/game/map_manager_async_preload_test.cpp` 为主，补充边界断言，不拆新套件。

## 3. 实现步骤

### Step 1：MainThreadCommandQueue 预算化
- 增加 `DrainPolicy`（`max_commands` + `time_budget_us`）。
- `drain(policy)` 返回 `DrainResult`（执行数、剩余深度、耗时）。
- 保留线程亲和检查与无异常约束。

### Step 2：GameApp 提交点固化
- 保持唯一提交点在 `updateFrame(...)` 后、`render(...)` 前。
- `GameApp::drainMainThreadCommands()` 使用 `DrainPolicy`，并输出慢帧告警日志。
- 若当帧 budget 不足，剩余命令下帧继续处理（有界延迟）。

### Step 3：MapManager 去私有 drain（核心）
- 移除 `MapManager::waitForAsyncPreloadReady()` 中对 `command_queue.drain()` 的直接调用。
- 改为仅做状态轮询与预算等待；超时按策略同步 fallback（保持现有调用语义）。
- 显式注释 `Running/Ready/Applied` 语义，避免后续误用。
- 保证提交执行权始终在 `GameApp`。

### Step 4：测试与验收
- 新增 queue budget 单测（功能测试，不做字符串存在性检查）。
- 扩展 `game_app_dispatcher_trace_test`，校验 `updateFrame -> drainMainThreadCommands -> render` 顺序契约。
- 扩展 `map_manager_async_preload_test`，校验移除私有 `drain` 后仍可在外部 drain 驱动下收敛到终态。
- TSAN 回归：`ThreadPool/MainThreadCommandQueue/MapManagerAsyncPreload`。

## 4. 待办清单（用于进度追踪）

- [x] T1 `MainThreadCommandQueue` 增加 `DrainPolicy` 与 `DrainResult`
- [x] T2 `GameApp::drainMainThreadCommands()` 切换为预算化执行
- [x] T3 `GameApp` 增加慢 drain 告警日志（量化阈值）
- [x] T4 `MapManager::waitForAsyncPreloadReady()` 移除内部 `drain()`
- [x] T5 为 `Running/Ready/Applied` 增加注释并统一使用点
- [x] T6 新增 `main_thread_command_queue_budget_test`
- [x] T7 扩展 `game_app_dispatcher_trace_test` 的提交顺序断言
- [x] T8 扩展 `map_manager_async_preload_test` 覆盖“仅外部 drain 驱动”路径
- [x] T9 在 `build` 与 `build-tsan` 跑通目标回归集

## 5. 验收标准（Phase 2 DoD）

- 业务代码（含 `MapManager`）不再直接调用 `MainThreadCommandQueue::drain()`。
- 主线程命令提交仅在 `GameApp` 统一提交点执行，时序可追踪。
- `drain(policy)` 返回 `DrainResult{executed, remaining, elapsed_us, budget_hit}`。
- 当单帧 `drain` 耗时超过阈值（默认 `2ms`）时输出 `warning` 日志。
- 地图异步预加载在新边界下无行为回退（功能测试通过）。
- TSAN 构建与关键并发测试通过。

## 6. 疑问与待澄清

- 暂无。按此计划可直接进入 Phase 2 实施。
