# 2026-02-22 Phase 4 后台存档 I/O 开发计划（Review 后）

## 元信息
- 阶段：`Phase 4`
- 主题：`后台存档 I/O（Main-thread Capture + Worker Write）`
- 优先级：`P1`
- 状态：`Planned`
- 范围边界：
  - 仅改造“保存”链路为异步，不改 `loadFromFile()` 同步语义。
  - 不改变主线程边界：`registry/map_manager/dispatcher/GL` 继续只在主线程访问。

## 1. 实现思路（最优方案）

采用 **“主线程快照 + 后台写盘 + 主线程轮询完成”**：

1. 主线程阶段：
  - 执行 `map_manager_.snapshotCurrentMap()`。
  - 调用 `capture()` 产出 `SaveData` 值对象。
2. 后台阶段（`SaveService` 内部 `std::jthread`）：
  - `serialize(data) -> json.dump(2) -> write tmp -> rename`。
  - 只处理值对象与文件路径，不访问 `registry_`、`world_state_`、`map_manager_`、`context_`。
  - 生命周期采用 **per-save 线程**：每次异步保存创建一个 `std::jthread`，任务结束后线程退出；不引入常驻线程与 `condition_variable`。
3. 状态管理：
  - `isSaving()` 作为防重入闸门；保存进行中拒绝再次触发。
  - `consumeAsyncSaveResult()` 在主线程消费终态（成功/失败+错误信息）。
  - 若 `saveToFileAsync()` 触发时存在“未消费终态”，先隐式清理旧终态并记录 warning，再启动新任务。
4. UI 集成（`PauseMenuScene`）：
  - 触发保存成功后立即关闭 `SaveSlotSelectScene`，主菜单显示 `Saving...`。
  - `update()` 每帧轮询结果；完成后显示 `Saved` 或 `Save failed: ...`。
  - 保存期间禁用 `Save/Load/BackToTitle` 按钮，避免并发读写与切场景时序复杂化。
  - 即使出现其他路径触发场景销毁，仍由 `SaveService` 析构 join 兜底保证写盘完成。

### 1.1 纯写盘函数契约（明确签名）

```cpp
// 纯函数：不访问 SaveService 成员，可在任意线程调用
[[nodiscard]] static bool writeSaveFile(
    const SaveData& data,
    const std::filesystem::path& file_path,
    std::string& out_error);
```

- 同步路径：`saveToFile() = snapshotCurrentMap() + capture() + writeSaveFile()`。
- 异步路径：`saveToFileAsync() = snapshotCurrentMap() + capture() + jthread(writeSaveFile)`。

## 2. 需要新增的文件

- `tests/game/save_service_async_test.cpp`  
  覆盖异步保存成功、失败、防重入、析构等待、未消费终态再触发等行为。

## 预计修改的文件（Phase 4）

- `src/game/save/save_service.h`
- `src/game/save/save_service.cpp`
- `src/game/scene/pause_menu_scene.h`
- `src/game/scene/pause_menu_scene.cpp`
- `tests/CMakeLists.txt`

## 3. 实现步骤（拆分执行）

### Step 1：提取纯写盘函数
- 从 `saveToFile()` 中拆出 `writeSaveFile(const SaveData&, const std::filesystem::path&, std::string&)`。
- 明确该函数无成员依赖、无主线程依赖，可被同步/异步路径复用。

### Step 2：SaveService 异步状态机
- 保留同步 `saveToFile()`；新增异步启动接口 `saveToFileAsync(...)`。
- 状态机定义为：`Idle -> Running -> Succeeded/Failed -> Consumed(Idle)`。
- 对“未消费终态”增加容错：下一次异步保存会清理旧终态后继续。
- 异步执行采用 per-save `std::jthread`；析构路径显式 join 等待。

### Step 3：PauseMenuScene 适配异步保存
- `onSaveClicked()` 中改为启动异步保存而非阻塞保存。
- 在 `update()` 中轮询并消费保存结果，更新消息文本。
- 保存进行中禁用 `Save/Load/BackToTitle`，结果落地后恢复按钮可用状态。
- 校验 `SaveSlotSelectScene` 关闭后 `PauseMenuScene::update()` 仍可驱动轮询链路。

### Step 4：测试与并发回归
- 新增 `save_service_async_test`，已覆盖 5 组用例：
  1) 正常异步保存成功并落盘；
  2) 不可写路径导致异步失败并可消费错误；
  3) `isSaving()` 防重入；
  4) 析构时有运行中任务，析构需等待并保证文件完整；
  5) 未消费终态再次触发保存，旧终态被丢弃且流程可继续。
- 扩展 `PauseMenuScene` 测试，覆盖“保存中按钮禁用 + 保存完成恢复”。
- 执行 TSAN 回归，确认无 data race。

## 4. 待办清单（用于后续追踪）

- [x] T1 从 `SaveService::saveToFile()` 拆出纯写盘函数（值输入 + 路径输入）
- [x] T2 保留同步 `saveToFile()`，改为复用 `writeSaveFile()`
- [x] T3 新增 `saveToFileAsync()` 启动接口（per-save `std::jthread`）
- [x] T4 新增 `isSaving()` 与异步结果消费接口
- [x] T5 增加“未消费终态自动清理并告警”逻辑
- [x] T6 `PauseMenuScene` 接入异步保存轮询与消息提示
- [x] T7 保存进行中禁用 `Save/Load/BackToTitle`，完成后恢复
- [x] T8 新增 `save_service_async_test.cpp`（5 组行为级核心用例）
- [x] T9 扩展 PauseMenuScene 测试覆盖异步保存 UI 行为
- [x] T10 跑通 `build` 与 `build-tsan` 关键回归（针对本次新增测试集）

## 5. 疑问与待澄清
- 暂无。按此计划可直接进入实现。
