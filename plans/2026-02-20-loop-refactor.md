# 2026-02-20 逻辑循环 / 渲染循环拆分重构计划

## 当前基线（简要）
- 当前主循环在 `src/engine/core/game_app.cpp` 中是单循环：`time.update -> input.update -> scene.update(delta) -> render -> dispatcher.update`。
- `src/engine/core/time.*` 只有单一 `target_fps`，同时影响逻辑与渲染节拍。
- `src/engine/scene/scene.cpp` 的 `Scene::update` 会推进 UI，`src/game/scene/game_scene.cpp` 的 `GameScene::update` 会推进 gameplay scheduler。
- 渲染与逻辑已在 `GameScene` 内部分离为 `update`/`render` 两个函数，但两者仍共享同一外层帧节拍。

## 执行进度
- [x] Step 1：循环契约与边界冻结（见 `docs/loop_timing_contract.md`）
- [ ] Step 2：时间管理职责拆分
- [ ] Step 3：`GameApp::run()` accumulator 化
- [ ] Step 4：输入语义前置修正
- [ ] Step 5：Scene 双通道更新拆分
- [ ] Step 6：`GameScene` 固定逻辑迁移
- [ ] Step 7：渲染插值（可选后置）
- [ ] Step 8：配置与调试面板升级
- [ ] Step 9：测试与文档回归

## 重构步骤

### 1. 明确新循环契约与阶段边界
- 目标：确定“固定逻辑步长 + 可变渲染步长”的统一语义，避免后续实现阶段边界反复变动。
- 实现思路：在引擎层先锁定以下约束，再按约束推进改造：固定逻辑步长不随时间缩放变化；`time_scale` 仅作用于 accumulator 输入；`dispatcher.update()` 维持“每个渲染帧一次，且在 render 后”以保持现有事件语义；设置默认 `max_ticks_per_frame = 5` 防止追赶爆帧；场景切换后清空 accumulator 残余时间。  
  输出产物：`docs/loop_timing_contract.md`（已完成）。

### 2. 拆分时间管理职责（逻辑时钟 vs 渲染时钟）
- 目标：让时间模块同时支持固定逻辑步长和可变渲染帧间隔，而不是只有单一 `delta_time`。
- 实现思路：重构 `Time` 接口与内部状态，提供固定步长配置、frame delta 采样、accumulator、追赶计数与统计；明确 `fixed_dt` 为常量，`scaled_frame_delta = frame_delta * time_scale` 仅用于累积与 tick 预算计算。

### 3. 改造 `GameApp::run()` 为 accumulator 主循环
- 目标：在引擎入口真正把逻辑循环和渲染循环拆开。
- 实现思路：外层循环按“事件采样 + 时间推进 + 多次固定逻辑 tick + 一次渲染提交 + dispatcher.update”执行；把单帧 `update(delta)` 改为“按固定 dt 重复执行逻辑”，并接入 `max_ticks_per_frame` 限制与超限丢帧保护；若检测到场景栈变更，则立即清空 accumulator。

### 4. 前置修正输入帧语义（与步骤3紧耦合）
- 目标：确保 accumulator 上线后 `PRESSED/HELD/RELEASED` 语义不回退。
- 实现思路：将 InputManager 拆为“每渲染帧一次的事件采样阶段”和“每固定 tick 的状态消费阶段”，保证边沿输入在同一渲染帧内不会被重复触发或提前丢失。

### 5. 拆分 Scene 层更新职责并明确非 GameScene 策略
- 目标：避免 UI/表现逻辑被固定 tick 过驱动，同时保持场景栈冻结语义。
- 实现思路：Scene 基类增加“固定逻辑更新入口 + 帧表现更新入口”双通道，默认固定更新为空；仅 `GameScene` 实现固定逻辑更新，`TitleScene/PauseMenuScene/RestDialogScene` 等继续走帧表现更新；SceneManager 仍保持“仅更新栈顶场景”的规则。

### 6. 迁移 `GameScene` 调度路径到固定逻辑 tick
- 目标：保证 gameplay `SystemScheduler` 完全运行在固定步长下，渲染链路继续独立。
- 实现思路：将 `GameScene` 中 scheduler 调用迁移到固定逻辑入口，保留 `render` 中渲染系统链路；并在场景切换触发点验证 accumulator 清零策略，避免新场景收到残余时间爆发。

### 7. 渲染插值/快照通道（可选后置优化）
- 目标：在确认出现可见抖动时，再以最小范围引入插值提升观感。
- 实现思路：将该项标记为不阻塞主改造的后置优化；仅对 `Transform(position)` 与相机建立前后状态插值，不扩散到全部组件。

### 8. 配置与调试面板升级
- 目标：让新循环参数可配置、可观测，便于调优与排查。
- 实现思路：扩展配置项（逻辑频率、渲染节流、`max_ticks_per_frame` 默认 5、可选插值开关等），同步更新 Time Debug/相关诊断面板，展示逻辑 tick、渲染 FPS、accumulator/backlog、追赶丢帧统计。

### 9. 测试与文档回归
- 目标：锁定新循环行为，避免时序回退。
- 实现思路：补充循环时序与输入语义测试（固定步长执行次数、追赶上限、render 后 `dispatcher.update` 语义、输入边沿语义、场景切换清零 accumulator），并更新 `docs/entry_to_first_frame.md`、`docs/events.md`、`docs/input_system.md` 等文档。
