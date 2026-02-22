# 2026-02-22 Phase 4 后台存档 I/O 开发计划（Review 后）

## 元信息
- 阶段：`Phase 4`
- 主题：`后台存档 I/O（Main-thread Capture + Worker Write）`
- 优先级：`P1`
- 状态：`Planned`
- 范围边界：
  - 仅改造“保存”链路为异步，不改 `loadFromFile()` 同步语义。
  - 不改变主线程边界：`registry/map_manager/dispatcher/GL` 继续只在主线程访问。

## Review 结论（针对原 Phase 4 草案）
- 原方案总体方向可行：`SaveData` 为值类型，`serialize + dump + write` 可安全迁移到后台线程。
- 需收敛的点：
  - 并发执行器必须单一：不再保留“`jthread` 或 `ThreadPool`”二选一，统一采用 `SaveService` 内聚 `std::jthread`（单 in-flight 保存任务）。
  - 结果回传接口必须单一：不混用回调与 `future`，统一由 `SaveService` 提供可轮询/可消费的完成结果。
  - 生命周期必须可验证：`SaveService` 析构时要等待后台写盘结束，避免退出时丢存档或悬空访问。

## 1. 实现思路（最优方案）

采用 **“主线程快照 + 后台写盘 + 主线程轮询完成”**：

1. 主线程阶段：
  - 执行 `map_manager_.snapshotCurrentMap()`。
  - 调用 `capture()` 产出 `SaveData` 值对象。
2. 后台阶段（`SaveService` 内部 `std::jthread`）：
  - `serialize(data) -> json.dump(2) -> write tmp -> rename`。
  - 只处理值对象与文件路径，不访问 `registry_`、`world_state_`、`map_manager_`、`context_`。
3. 状态管理：
  - `isSaving()` 作为防重入闸门；保存进行中拒绝再次触发。
  - `consumeAsyncSaveResult()` 在主线程消费终态（成功/失败+错误信息）。
4. UI 集成（`PauseMenuScene`）：
  - 触发保存成功后立即关闭 `SaveSlotSelectScene`，主菜单显示 `Saving...`。
  - `update()` 每帧轮询结果；完成后显示 `Saved` 或 `Save failed: ...`。
  - 保存期间禁用 `Save/Load` 按钮，避免并发读写。

## 2. 需要新增的文件

- `tests/game/save_service_async_test.cpp`  
  覆盖异步保存成功、失败、防重入、析构等待与结果回传。

## 预计修改的文件（Phase 4）

- `src/game/save/save_service.h`
- `src/game/save/save_service.cpp`
- `src/game/scene/pause_menu_scene.h`
- `src/game/scene/pause_menu_scene.cpp`
- `tests/CMakeLists.txt`

## 3. 实现步骤（拆分执行）

### Step 1：提取纯写盘函数
- 从 `saveToFile()` 中拆出“`SaveData -> JSON -> tmp 写入 -> rename`”纯函数。
- 确保该函数不依赖任何主线程对象，只接收值参数与 `filesystem::path`。

### Step 2：SaveService 异步状态机
- 新增异步启动接口（例如 `saveToFileAsync(...)`）。
- 新增 `isSaving()` 与结果消费接口，形成 `Idle -> Running -> Succeeded/Failed -> Idle` 闭环。
- 用 `std::jthread` 执行后台写盘；析构路径显式等待线程结束。

### Step 3：PauseMenuScene 适配异步保存
- `onSaveClicked()` 中改为启动异步保存而非阻塞保存。
- 在 `update()` 中轮询并消费保存结果，更新消息文本。
- 保存进行中禁用 `Save/Load`，结果落地后恢复按钮可用状态。

### Step 4：测试与并发回归
- 新增 `save_service_async_test`（临时目录 + 实盘写入验证）。
- 扩展 `PauseMenuScene` 相关测试，覆盖“保存中禁用按钮”行为。
- 执行 TSAN 回归，确认无 data race。

## 4. 待办清单（用于后续追踪）

- [ ] T1 从 `SaveService::saveToFile()` 拆出纯写盘函数（值输入 + 路径输入）
- [ ] T2 新增 `saveToFileAsync()` 启动接口
- [ ] T3 新增 `isSaving()` 与异步结果消费接口
- [ ] T4 后台写盘统一使用 `std::jthread`，并补析构等待逻辑
- [ ] T5 `PauseMenuScene` 接入异步保存轮询与消息提示
- [ ] T6 保存进行中禁用 `Save/Load`，完成后恢复
- [ ] T7 新增 `save_service_async_test.cpp`
- [ ] T8 扩展 PauseMenuScene 测试覆盖异步保存 UI 行为
- [ ] T9 跑通 `build` 与 `build-tsan` 关键回归

## 5. 疑问与待澄清
- 暂无。按此计划可直接进入实现。
