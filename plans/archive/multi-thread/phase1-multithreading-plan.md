# 2026-02-21 Phase 1 多线程改造开发计划

## 元信息
- 阶段：`Phase 1`
- 主题：`异步加载预处理（不改 gameplay 并行）`
- 优先级：`P0`
- 状态：`Completed（实现完成，已做针对性验证）`
- 范围边界：仅实现“Worker 做 IO/解析/解码 + 主线程提交执行”，不并行 `SystemScheduler`。

## 目标
- 消除地图预加载与切图过程中的主线程重阻塞点。
- 建立可复用的线程基础设施（线程池、任务队列、主线程命令队列）。
- 严格保持线程边界：`registry/dispatcher/OpenGL` 只允许主线程写入/调用。
- 为后续 Phase 2（主线程提交点标准化）和 Phase 3（局部 gameplay 并行）打基础。

## 1. 实现思路（最优方案）

采用 **“双队列 + 中间产物”** 方案：

1. 主线程把耗时任务（地图文件读取、JSON 解析、图片/字体 CPU 预处理）投递到 `WorkQueue`。
2. Worker 线程只产出 CPU 侧中间产物（如 `DecodedImage`、`ParsedLevelData`），绝不触碰 `OpenGL`、`entt::registry`、`entt::dispatcher`。
3. Worker 将“可提交结果”封装为 `MainThreadCommand` 投递到 `MainThreadCommandQueue`。
4. 主线程在固定提交点 `drainMainThreadCommands()` 执行命令：GPU 上传、资源注册、实体构建、事件分发。
5. `MapManager/LevelLoader` 先接入异步预热路径，默认走异步；必要时保留同步 fallback 仅用于故障兜底与测试。

关键设计决策：
- 使用 `std::jthread + std::stop_token` 管理 worker 生命周期，避免 `detach`。
- 队列采用有界容量（背压），防止切图频繁时任务无限堆积。
- `tiled_json_cache` 改为线程安全实现（`shared_mutex` 或实例级缓存），杜绝全局无锁容器竞态。
- OpenGL 调用点加线程断言（调试构建），快速暴露越界调用。
- `FontManager` 在 Phase 1 的范围：worker 负责字体预处理（HarfBuzz/FreeType 栅格化生成位图），主线程只负责图集纹理创建/上传/释放（`glTexSubImage2D/glGenTextures/glDeleteTextures`）。

## 2. 需要新增的文件

- `src/engine/async/thread_pool.h`
- `src/engine/async/thread_pool.cpp`
- `src/engine/async/work_queue.h`
- `src/engine/async/main_thread_command_queue.h`
- `src/engine/async/main_thread_command_queue.cpp`
- `src/engine/loader/level_preprocess_data.h`
- `src/engine/loader/level_preprocess_service.h`
- `src/engine/loader/level_preprocess_service.cpp`
- `src/engine/resource/decoded_image.h`
- `src/engine/resource/image_decode_service.h`
- `src/engine/resource/image_decode_service.cpp`
- `src/engine/resource/font_preprocess_data.h`
- `src/engine/resource/font_preprocess_service.h`
- `src/engine/resource/font_preprocess_service.cpp`
- `tests/engine/async/thread_pool_test.cpp`
- `tests/engine/loader/level_preprocess_service_test.cpp`
- `tests/engine/resource/font_preprocess_service_test.cpp`
- `tests/game/world/map_manager_async_preload_test.cpp`

## 预计修改的文件（Phase 1）

- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`
- `src/game/world/map_manager.h`
- `src/game/world/map_manager.cpp`
- `src/engine/loader/level_loader.h`
- `src/engine/loader/level_loader.cpp`
- `src/engine/loader/tiled_json_cache.h`
- `src/engine/resource/resource_manager.h`
- `src/engine/resource/resource_manager.cpp`
- `src/engine/resource/font_manager.h`
- `src/engine/resource/font_manager.cpp`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `cmake/CompilerSettings.cmake`

## 3. 实现步骤（拆分执行）

### Step 1：建立线程基础设施
- 新增线程池与双队列组件，支持任务投递、取消、有界容量和优雅停机。
- 先写单元测试锁定基础行为（并发提交、停止后拒绝新任务、队列背压）。

### Step 2：定义预处理中间产物模型
- 抽象 `ParsedLevelData` / `DecodedImage` 等 CPU 侧结果对象。
- 明确这些对象可跨线程传递，但不持有 GL 句柄、不持有 `registry` 引用。

### Step 3：实现异步预处理服务
- 落地 `LevelPreprocessService`：读取地图/tileset JSON、收集图层与 tileset 元数据。
- 落地 `ImageDecodeService`：将纹理文件解码成像素缓冲（仅 CPU）。
- 落地 `FontPreprocessService`：将字体 shaping/rasterize 结果转换为可提交的 glyph bitmap（仅 CPU）。
- 修复 `tiled_json_cache` 并发安全。

### Step 4：接入 MapManager/LevelLoader 异步预热入口
- 在 `MapManager::preloadMap/loadMap` 发起异步任务，接收可提交结果。
- `LevelLoader` 拆分为“预处理（worker）”与“提交（main）”两个阶段函数。
- 明确任务状态机：`NotScheduled -> Running -> Ready | Failed -> Applied`。
- 明确 `loadMap` 主路径策略：  
  1) `Ready`：直接提交异步结果。  
  2) `Running`：按预算时间等待（例如 `map_async_wait_budget_ms`），超时后降级同步加载。  
  3) `Failed/NotScheduled`：直接走同步 fallback。  
  4) 异步结果晚到：通过 generation/token 校验后丢弃过期结果，避免旧任务覆盖当前地图。

### Step 5：增加主线程提交点并执行命令
- 在 `GameApp::run()` 中精确插入 `drainMainThreadCommands()`：位于 fixed-tick 循环结束、`updateFrame(...)` 之后、`render(interpolation_alpha)` 之前（对应 `src/engine/core/game_app.cpp` 渲染调用前位置）。
- 提交命令中执行 GPU 上传、资源登记、实体构建，保证顺序可控。

### Step 6：故障回退与可观测性
- 异步任务失败时提供同步 fallback（仅兜底，不作为常态路径）。
- 增加日志和统计：任务耗时、排队时长、提交时长、失败率。

### Step 7：测试与验收
- 增加并发正确性测试和切图稳定性测试。
- 进行回归：地图切换、预加载、资源释放流程必须稳定无崩溃。

### Step 8：TSAN 构建配置与并发验收
- 在 `cmake/CompilerSettings.cmake` 增加 `ENABLE_TSAN` 选项与对应编译/链接参数。
- 增加 TSAN 构建与测试命令，作为 Phase 1 验收的必跑项。

## 4. 待办清单（用于后续追踪）

- [x] T1 新增 `ThreadPool/WorkQueue/MainThreadCommandQueue` 并通过基础并发测试
- [x] T2 定义 `ParsedLevelData/DecodedImage` 中间产物结构并补充注释契约
- [x] T3 实现 `LevelPreprocessService`（JSON 读取 + 解析）并补测试
- [x] T4 实现 `ImageDecodeService`（图片解码）并补测试
- [x] T4b 实现 `FontPreprocessService`（字体栅格化 CPU 阶段）并补测试
- [x] T5 改造 `tiled_json_cache` 为线程安全实现
- [x] T6 `MapManager` 接入异步预加载入口（可查询任务状态）
- [x] T6a 定义 `MapPreloadTaskState` 与 `loadMap` 的等待/降级/过期结果丢弃策略
- [x] T7 `LevelLoader` 拆分预处理阶段与主线程提交阶段
- [x] T8 `GameApp::run()` 在 `updateFrame(...)` 后、`render(...)` 前增加 `drainMainThreadCommands()`
- [x] T9 接入失败回退与统计日志（队列长度/任务耗时/失败原因）
- [x] T10 新增 `map_manager_async_preload_test` 并完成关键回归
- [x] T11 在 `cmake/CompilerSettings.cmake` 增加 `-DENABLE_TSAN=ON` 配置并跑通 TSAN 构建/测试

## 验收标准（Phase 1 DoD）
- 地图预加载中的文件读取/JSON 解析/图片解码不再阻塞主线程。
- 任意 Worker 线程不直接调用 OpenGL、`registry` 写接口、`dispatcher` 分发接口。
- 资源上传与实体构建统一由主线程命令执行。
- `tiled_json_cache` 在并发预加载下无 data race（TSAN 构建可通过）。
- `ENABLE_TSAN=ON` 配置可用，且 `cmake -S . -B build-tsan -DENABLE_TSAN=ON -DBUILD_TESTING=ON` 与对应 `ctest` 可通过。
- 发生任务异常时，系统可回退并继续运行，不崩溃。

## 风险与应对
- 风险：任务依赖关系处理不当导致提交顺序错乱。  
  应对：命令携带 map/version 序号，提交前校验当前上下文。
- 风险：过多预加载任务造成内存高峰。  
  应对：有界队列 + 解码结果大小阈值 + 超限丢弃策略。
- 风险：析构期后台任务仍在运行引发悬空引用。  
  应对：`GameApp::close()` 中先停线程池，再清理资源与场景。
- 风险：字体预处理并发下 FreeType/HarfBuzz 上下文误共享导致竞态。  
  应对：worker 使用线程局部或任务局部字体上下文，不跨线程共享可变 face/buffer 对象。

## 5. 疑问与待澄清
- 暂无。按此计划可直接进入实现。
