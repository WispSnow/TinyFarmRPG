### Phase 3: 主循环三段式恢复

**目标**：把 RmlUi 的 `Update()` 从 `GLRenderer::present()` 中移出来，恢复显式的 `ProcessEvents / Update / Render`。

本阶段完成后，应满足：

- `ProcessEvents`: `InputManager -> RmlUiRuntime::processEvent()`
- `Update`: `GameApp` 显式调用 `RmlUiRuntime::update()`
- `Render`: `GLRenderer -> render hook -> RmlUiRenderBackendGl::render(...)`

进入本阶段前，默认前提是：

- Phase 2 的过渡 hook 已经存在
- 该 hook 仍临时执行 `update() + render()`

本阶段的核心就是把这部分临时 update 从 hook 中拿出来。

#### 本阶段要做的事

1. 调整 `GameApp` 主循环
   - 在合适的 update 阶段显式调用 `RmlUiRuntime::update()`
   - 不再依赖 `GLRenderer::present()` 内部顺带更新 RmlUi

2. 精简 `GLRenderer::present()`
   - 保留世界合成
   - 保留 overlay vfx
   - 通过 hook 触发 RmlUi render
   - 保留 ImGui 和 swap

3. 审计 `queueFocus*()` 调用点
   - 确认现有 queue 行为主要发生在 init / event / state transition 阶段
   - 若发现 render 阶段 late queue，再补一个“render 前二次 drain”过渡策略

4. 验证 focus 时序
   - Title / Pause / SaveSlotSelect / Battle / Inventory 的默认 focus
   - action menu 打开 / 关闭后的 focus 恢复

#### 本阶段不做

- 不新增或删除场景类
- 不引入 `GameSceneUiController`
- 不删除兼容壳

#### 涉及文件

- 修改 `src/engine/core/game_app.cpp`
- 修改 `src/engine/render/opengl/gl_renderer.cpp`
- 必要时修改 `src/engine/ui/rmlui/rml_ui_runtime.h`
- 必要时修改 `src/engine/ui/rmlui/rml_ui_runtime.cpp`

#### 验证

- RmlUi 不再在 `present()` 中做逻辑更新
- render hook 只负责 render，不再负责 update
- 默认 focus 和延迟 focus 行为不退化
- 没有重复 update 或漏 update

#### 完成标记

- [ ] `GameApp` 显式增加 RmlUi update 阶段
- [ ] `GLRenderer::present()` 不再承担 RmlUi runtime update
- [ ] render hook 从临时的 `update + render` 收缩为纯 `render`
- [ ] `queueFocus*()` 调用点审计完成
- [ ] 若有 late queue，补 render 前二次 drain 过渡策略
