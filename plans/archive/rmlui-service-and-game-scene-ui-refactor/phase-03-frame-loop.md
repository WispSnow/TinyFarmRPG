### Phase 3: 主循环三段式恢复

**目标**：把 RmlUi 的 `Update()` 从 `GLRenderer::present()` 中移出来，恢复显式的 `ProcessEvents / Update / Render`。

本阶段完成后，应满足：

- `ProcessEvents`: `InputManager -> RmlUiRuntime::processEvent()`
- `PrepareUi`: `Scene / SceneManager` 在 `RmlUiRuntime::update()` 前完成 retained UI composition
- `Update`: `GameApp` 显式调用 `RmlUiRuntime::update()`
- `Render`: `GLRenderer -> render hook -> RmlUiRenderBackendGl::render(...)`

进入本阶段前，默认前提是：

- Phase 2 的过渡 hook 已经存在
- 该 hook 仍临时执行 `update() + render()`

本阶段的核心就是把这部分临时 update 从 hook 中拿出来。

#### 本阶段要做的事

1. 提取 render 前的 UI 准备阶段
   - 新增 `Scene::prepareUi(float interpolation_alpha)`
   - 新增 `SceneManager::prepareUi(float interpolation_alpha)`
   - 把 `GameScene` 中依赖插值的 anchored widget 位置同步从 `render()` 前移到 `prepareUi()`

2. 调整 `GameApp` 帧流程
   - 在 `render()` 中显式执行 `prepareUi -> RmlUiRuntime::update -> scene render`
   - 不再依赖 `GLRenderer::present()` 内部顺带更新 RmlUi

3. 精简 `GLRenderer::present()`
   - 保留世界合成
   - 保留 overlay vfx
   - 通过 hook 触发 RmlUi render
   - 保留 ImGui 和 swap

4. 审计 `queueFocus*()` 调用点
   - 确认现有 queue 行为主要发生在 init / event / state transition 阶段
   - 若发现 render 阶段 late queue，再补一个“render 前二次 drain”过渡策略

5. 验证 focus 时序
   - Title / Pause / SaveSlotSelect / Battle / Inventory 的默认 focus
   - action menu 打开 / 关闭后的 focus 恢复

#### 本阶段不做

- 不新增或删除场景类
- 不引入 `GameSceneUiController`
- 不删除兼容壳

#### 涉及文件

- 修改 `src/engine/core/game_app.cpp`
- 修改 `src/engine/core/game_app.h`
- 修改 `src/engine/scene/scene.h`
- 修改 `src/engine/scene/scene.cpp`
- 修改 `src/engine/scene/scene_manager.h`
- 修改 `src/engine/scene/scene_manager.cpp`
- 修改 `src/game/scene/game_scene.h`
- 修改 `src/game/scene/game_scene.cpp`
- 修改 `tests/engine/core/game_app_rmlui_frame_loop_test.cpp`
- 修改 `tests/engine/scene/render_interpolation_pipeline_test.cpp`

#### 验证

- RmlUi 不再在 render hook 中做逻辑更新
- `GameApp` 显式执行 `prepareUi -> update -> render`
- `GameScene` 的 anchored bubble 位置同步不再藏在 `render()` 里
- 默认 focus 和延迟 focus 行为不退化
- `queueFocus*()` 调用点审计结果：全部位于 init / event / state transition，未发现 render-stage late queue
- 因此本阶段不需要“render 前二次 drain”过渡策略

#### 完成标记

- [x] 新增 `Scene::prepareUi(float interpolation_alpha)`
- [x] 新增 `SceneManager::prepareUi(float interpolation_alpha)`
- [x] `GameScene` 将 anchored bubble 的 Rml 位置同步从 `render()` 挪到 `prepareUi()`
- [x] `GameApp` 显式增加 RmlUi update 阶段
- [x] render hook 从临时的 `update + render` 收缩为纯 `render`
- [x] `queueFocus*()` 调用点审计完成
- [x] 未发现 render-stage late queue，因此无需补二次 drain
