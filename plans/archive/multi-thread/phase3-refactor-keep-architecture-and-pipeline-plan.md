# 2026-02-21 多线程重构计划（保留架构 + Pipeline 解耦）

## 元信息
- 阶段：`Refactor A+B`
- 主题：`固化现有并发架构 + 抽取 AsyncPreloadPipeline 降低 MapManager 复杂度`
- 优先级：`P0`
- 状态：`Planned`
- 范围边界：
  - 不引入 `std::async/std::future` 作为主路径。
  - 不改 `SystemScheduler` 并行模型。
  - 不改 `GameApp` 的唯一主线程提交点（`updateFrame -> drainMainThreadCommands -> render`）。
  - 可自由重构，不考虑向后兼容旧内部接口。

## 1. 实现思路（最优方案）

### 1.1 计划 1：保留并固化现有架构（不变项）
保留当前已经验证过的主干模型：`ThreadPool + WorkQueue + MainThreadCommandQueue + MapPreloadTaskState`。  
重构重点不是替换并发原语，而是把“必须保留的约束”显式化，避免后续迭代再次扩散复杂度：

1. 主线程拥有唯一提交权：只有 `GameApp` 执行 `drainMainThreadCommands()`。
2. Worker 仅做 CPU 任务（IO/解析/解码），不触碰 GL/registry/dispatcher。
3. 异步任务必须支持有界队列、背压、超时和过期结果丢弃（generation）。
4. `Running/Ready/Failed/Applied` 语义保持稳定，避免状态含义漂移。

### 1.2 计划 2：抽取 AsyncPreloadPipeline（减复杂）
将 `MapManager::scheduleAsyncPreloadTask` 中的线程任务拼装、解码与命令入队逻辑迁移到独立 `AsyncPreloadPipeline`，让 `MapManager` 回归“地图业务编排者”角色：

1. `MapManager` 仅负责：
  - 何时触发预加载；
  - 何时等待预算；
  - 何时降级同步；
  - 何时消费 `Ready` 结果并标记 `Applied`。
2. `AsyncPreloadPipeline` 负责：
  - Worker 任务提交；
  - 预处理与贴图解码；
  - 主线程命令封装与排队；
  - 状态/代次更新与错误归一化。
3. 通过“单一职责拆分”替代“继续堆叠 if/状态分支”，降低 `MapManager` 的认知复杂度和测试负担。
4. `generation_counter` 与异步任务共享状态完全内聚到 Pipeline 内部，`MapManager` 不再持有该类并发控制细节。
5. Pipeline 对外显式声明线程安全契约：
  - 任务表（task map）仅主线程访问，不额外引入 mutex；
  - `schedule/cancel/clear/setLoadingSettings/getTaskState` 约定主线程调用；
  - worker 线程只通过捕获的共享状态原子字段回写结果。

## 2. 需要新增的文件

- `src/game/world/async_preload_pipeline.h`
- `src/game/world/async_preload_pipeline.cpp`
- `tests/game/world/async_preload_pipeline_test.cpp`

## 预计修改的文件

- `src/game/world/map_manager.h`
- `src/game/world/map_manager.cpp`
- `src/game/world/map_loading_settings.h`
- `tests/game/map_manager_async_preload_test.cpp`
- `tests/CMakeLists.txt`
- `src/CMakeLists.txt`

## 3. 实现步骤

### Step 1：定义 Pipeline 接口与内聚模型
- 在 `async_preload_pipeline.h` 中直接定义任务共享类型（不再拆 `types` 文件）。
- 将 `generation_counter`、任务共享状态、状态写入规则与过期丢弃规则集中到 Pipeline 内部。
- 在头文件方法注释中显式写明线程安全契约（主线程调用边界、worker 回写边界）。

### Step 2：落地 AsyncPreloadPipeline 主流程（计划 2）
- 在 `async_preload_pipeline.cpp` 封装以下流程：
  - 提交 worker 任务；
  - 执行 `LevelLoader::preprocessLevelDataWorker`；
  - 执行 `ImageDecodeService::decodeRGBA`；
  - 组装主线程命令并 `enqueueWithWait`；
  - 命令执行后写回 `Ready/Failed`。
- 将错误日志格式化为统一入口，保留关键指标（queued/preprocess/decode/commit）。
- 固化析构顺序约束：Pipeline 析构时先 `stop` ThreadPool，再释放内部任务状态与引用资源；并在代码注释中写明其与 `MapManager/Scene/GameApp` 生命周期依赖关系。

### Step 3：MapManager 改为薄编排层
- `MapManager` 只保留触发、轮询、预算等待、fallback、状态消费逻辑。
- 删除 `MapManager` 内部与业务无关的 worker/command 拼装细节。
- 将 `waitForAsyncPreloadReady()` 显式改造为委托 `pipeline_->getTaskState(map_id)` 轮询，不再直接触碰内部任务容器。
- 保持现有外部行为：`preloadAllMaps`、`loadMap`、邻接图预加载语义不变。

### Step 4：测试与回归
- 新增 `async_preload_pipeline_test` 覆盖：
  - 提交流程；
  - generation 过期结果丢弃；
  - command queue 满时失败路径；
  - 状态终态收敛；
  - Pipeline 析构时仍有 `Running` 任务的安全退出（无死锁、无悬空引用、无崩溃）。
- 扩展 `map_manager_async_preload_test`，验证 MapManager 仅编排、不承担提交执行。
- 跑现有并发与 TSAN 回归集，确认无 data race/语义回退。

## 4. 待办清单（后续进度追踪）

- [ ] T1 新增 `AsyncPreloadPipeline` 头源文件，并在头文件内联定义任务共享类型与线程安全契约
- [ ] T2 将 `generation_counter` 与任务状态管理完全内聚到 Pipeline（MapManager 移除对应字段）
- [ ] T3 完成 worker->main 提交链路迁移，并在析构路径固化 `stop ThreadPool -> 释放其余状态` 顺序注释
- [ ] T4 改造 `MapManager::waitForAsyncPreloadReady()` 为委托 `pipeline_->getTaskState(map_id)` 轮询
- [ ] T5 新增 `async_preload_pipeline_test`，覆盖失败/过期/背压/析构中运行任务安全退出
- [ ] T6 扩展 `map_manager_async_preload_test`，确保 MapManager 只编排不提交
- [ ] T7 跑通 `build` 与 `build-tsan` 关键测试回归

## 5. 疑问与待澄清

- 暂无。可按该计划直接进入实现。
