# TinyFarmRPG 多线程改造分析（Codex）

## 1. 结论

当前项目适合采用 **“主线程提交 + Worker 线程池计算/IO”** 的渐进式多线程架构，不建议直接把现有 `SystemScheduler` 阶段并行化。

核心原因：
- 运行时主循环和 ECS 调度目前是严格串行。
- 渲染与资源生命周期对 OpenGL 上下文有强线程亲和性。
- `registry/dispatcher` 存在大量共享可变状态，直接并行会引入高风险数据竞争与时序回归。

---

## 2. 现状诊断（代码证据）

### 2.1 主循环与事件结算是串行
- 主循环串行执行：输入 -> fixed tick -> frame update -> render -> dispatcher update  
  `src/engine/core/game_app.cpp:157`  
  `src/engine/core/game_app.cpp:218`

### 2.2 Gameplay Scheduler 是固定顺序串行 stage
- stage 通过 `switch` 串行执行  
  `src/game/runtime/system_scheduler.cpp:93`
- Exploration 模式下硬编码执行顺序  
  `src/game/runtime/system_scheduler.cpp:232`

### 2.3 GameScene 的 fixed/render 都单线程调用
- `fixedUpdate` 中串行调用 scheduler  
  `src/game/scene/game_scene.cpp:156`
- render 路径串行渲染各系统  
  `src/game/scene/game_scene.cpp:201`

### 2.4 OpenGL 线程亲和明确，资源加载含 GL 调用
- 创建并绑定 GL 上下文  
  `src/engine/render/opengl/render_context.cpp:132`  
  `src/engine/render/opengl/render_context.cpp:138`
- 纹理加载直接调用 `glGenTextures/glTexImage2D`  
  `src/engine/resource/texture_loader.cpp:40`
- 字体图集操作直接调用 `glTexSubImage2D/glGenTextures/glDeleteTextures`  
  `src/engine/resource/font_manager.cpp:277`  
  `src/engine/resource/font_manager.cpp:376`  
  `src/engine/resource/font_manager.cpp:408`

### 2.5 地图加载/预加载目前同步阻塞
- `MapManager::loadMap` 同步创建 loader 并调用 `loadLevel`  
  `src/game/world/map_manager.cpp:333`  
  `src/game/world/map_manager.cpp:378`
- `preloadMap/preloadLevelData` 也是同步  
  `src/game/world/map_manager.cpp:262`  
  `src/engine/loader/level_loader.cpp:175`

### 2.6 并发隐患：全局无锁缓存
- Tiled JSON 缓存为全局 `inline` 容器，无同步保护  
  `src/engine/loader/tiled_json_cache.h:21`  
  `src/engine/loader/tiled_json_cache.h:24`  
  `src/engine/loader/tiled_json_cache.h:76`

### 2.7 运行时几乎未引入并发原语
- 检索 `std::thread/std::mutex/std::future/std::atomic` 未发现实际运行时代码使用。
- 仅存在 `thread_local` RNG（非并行架构）  
  `src/engine/utils/math.h:155`

---

## 3. 推荐目标线程模型（现代 C++ 实践）

### 3.1 主线程（保持单线程）
- SDL 事件采样与输入状态推进
- `entt::registry` 写入与实体生命周期管理
- `entt::dispatcher` 事件分发/结算
- 场景栈修改、地图切换提交
- 全部 OpenGL 调用（渲染、GPU 纹理创建/销毁、字体图集上传）

### 3.2 Worker 线程池
- 文件 IO（地图/tileset/json）
- JSON 解析与预处理
- 图片解码（CPU 像素数据准备）
- 纯计算类任务（不触碰 registry/dispatcher/GL）

### 3.3 推荐基础设施
- `std::jthread` + `std::stop_token`：可取消任务，RAII 管理线程结束。
- 有界任务队列（避免无限积压）：
  - `WorkQueue`：主线程 -> worker
  - `MainThreadCommandQueue`：worker -> 主线程提交
- 统一线程边界约束：
  - worker 禁止直接调用 `registry` 写接口
  - worker 禁止直接调用 `dispatcher.trigger/enqueue`
  - worker 禁止直接调用任何 GL API

---

## 4. 为什么不建议直接并行 SystemScheduler

当前系统高度共享写：
- 多系统直接修改同一实体组件集（`Transform/Velocity/StateDirty/TransformDirty` 等）
- 空间索引也在同帧读写（`MovementSystem` 与 `SpatialIndexSystem`）
- 事件系统时序有语义约束（`trigger` 与 `enqueue + update`）

关键示例：
- `MovementSystem` 写 `Transform` 并标记 `TransformDirtyTag`  
  `src/engine/system/movement_system.cpp:16`
- `SpatialIndexSystem` 读取/移除 `TransformDirtyTag` 并更新索引  
  `src/engine/system/spatial_index_system.cpp:15`

在现状下做 stage 并行，收益不确定，风险很高。

---

## 5. 分阶段实施路线（建议顺序）

## Phase 1：先做“异步加载预处理”（低风险高收益）
- 目标：减少切图卡顿和主线程阻塞。
- 改造方向：
  1. 将 map/tileset 文件读取、JSON 解析、图片 decode 放入 worker。
  2. worker 产出中间产物（CPU 数据 + metadata），投递到主线程命令队列。
  3. 主线程统一执行 GPU 上传与实体创建提交。

首批切入点：
- `MapManager::preloadMap/loadMap`  
  `src/game/world/map_manager.cpp:262`  
  `src/game/world/map_manager.cpp:333`
- `LevelLoader::preloadLevelData/loadLevel`  
  `src/engine/loader/level_loader.cpp:67`  
  `src/engine/loader/level_loader.cpp:175`

## Phase 2：建立“主线程提交点”
- 在主循环增加固定的 `drainMainThreadCommands()` 位置（建议 fixed update 前）。
- 将 worker 结果封装为命令（例如：`UploadTextureCommand`、`ApplyMapDataCommand`）。
- 保证主线程提交时序可追踪（日志 + profiler 标记）。

## Phase 3：逐步尝试 gameplay 并行（可选，最后做）
- 前提：系统改造成“读快照 + 写命令缓冲（deferred apply）”。
- 先从纯读或弱耦合模块试点，不要一开始并行核心移动/交互链路。
- 每个并行批次之后设置明确 barrier，再统一合并写入。

---

## 6. 改造前必须处理的风险点

1. `tiled_json_cache` 的并发安全  
- 方案 A：加 `std::shared_mutex`（读多写少）  
- 方案 B：改为 loader 实例级缓存，减少全局共享

2. 资源系统线程边界  
- `TextureLoader/FontManager` 明确主线程调用约束（加线程断言）

3. 事件系统线程边界  
- 统一约定：worker 只能提交 command，不直接触发 dispatcher

4. 生命周期与关闭流程  
- 线程池需要在 `GameApp::close()` 前后有确定的 shutdown 顺序，避免资源析构与后台任务交叉。

---

## 7. 观测与验证方案

### 7.1 性能基线
- 使用现有 `SchedulerProfiler` 建立改造前后对比  
  `src/game/debug/scheduler_profiler.h:12`

关注指标：
- 地图切换耗时（P50/P95）
- 首帧可交互时间
- fixed tick 超时帧占比

### 7.2 并发正确性
- 增加 TSAN 构建目标（当前 CMake 未内建 sanitizer 入口）  
  `cmake/CompilerSettings.cmake:7`
- 压测场景：
  1. 高频切图 + 预加载
  2. 资源反复加载/清理
  3. 事件洪峰（大量 enqueue）

### 7.3 回归保障
- 增加地图切换一致性测试（实体数量、关键组件、摄像机位置）
- 增加资源句柄生命周期检查（避免 GL 资源提前释放）

---

## 8. 可执行的短期任务清单（建议一周内）

1. 新建 `ThreadPool + WorkQueue + MainThreadCommandQueue` 基础模块（不改业务行为）。
2. 为 `LevelLoader::preloadLevelData` 增加异步入口（先仅异步 JSON 读取与解析）。
3. `tiled_json_cache` 完成并发安全改造（锁或实例化）。
4. 在主循环增加命令队列 drain 点，并做 trace 日志。
5. 新增 TSAN 构建配置与至少一个切图并发测试用例。

完成这 5 项后，再决定是否进入 scheduler 的并行化探索。

