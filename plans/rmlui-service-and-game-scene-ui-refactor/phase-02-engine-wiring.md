### Phase 2: GameApp Wiring / Context 入口 / 引擎层接线

**目标**：先把引擎层依赖方向修正，再进入游戏层迁移。

本阶段完成后，引擎层应满足：

- `GameApp` 持有 runtime 和 backend
- `GLRenderer` 只保留 render hook
- `Context` 直接暴露 `getRmlUi()`
- `src/engine/**` 中不再出现 `getRmlUILayer()`

#### 本阶段要做的事

1. `GameApp` 接管 ownership
   - 新增 `rmlui_runtime_`
   - 新增 `rmlui_render_backend_`
   - 在 init 和 resize 后同步 viewport 给 runtime
   - 固定销毁顺序：
     - `scene_manager_`
     - `context_`
     - `rmlui_runtime_`
     - `rmlui_render_backend_`
     - `gl_renderer_`

2. `GLRenderer` 改为 render hook 接入
   - 新增 `setRmlUiRenderHook(...)`
   - `present()` 在世界合成后只触发 hook
   - 不再持有或查询 runtime
   - 过渡期内，render hook 暂时同时执行：
     - `rmlui_runtime_->update()`
     - `rmlui_render_backend_->render(*context, viewport)`
   - 这样可以在 Phase 2 保持现有“每帧末段 update + render”语义；Phase 3 再把 `update()` 从 hook 中移出

3. `Context` 新增 UI 模块入口
   - 在 `context_modules.h` 中新增 `UiServices`
   - `Context` 新增 `ui()` 和 `getRmlUi()`
   - 返回可空 `RmlUiRuntime*`

4. 迁移引擎层公共调用点
   - `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()`
   - `SceneManager::syncRmlActiveScene()`
   - `InputManager` 的 RmlUi event forwarder
   - `UINavigationController` 的导航接线
   - `HoverFocusSyncListener`
   - `RmlUiDebugPanel`

5. 说明 `RmlScreenFade` 的阶段归属
   - `RmlScreenFade` 文件位于 `src/engine/ui/rmlui/`，但当前主要由 `GameScene` 组合和持有
   - 因此本计划将它和游戏层 wrapper 一起放到 Phase 4 处理
   - 若实现时发现它只需要纯引擎层签名调整，可提前，但不强制

6. 处理 `RmlUiDebugPanel` 的双依赖
   - runtime 通过 `Context` 获取
   - backend 通过构造参数注入

#### 本阶段不做

- 不调整主循环里的 RmlUi update 时机
- 不迁移游戏层场景和 UI wrapper
- 不删除兼容壳

#### 涉及文件

- 修改 `src/engine/core/context_modules.h`
- 修改 `src/engine/core/context.h`
- 修改 `src/engine/core/context.cpp`
- 修改 `src/engine/core/game_app.h`
- 修改 `src/engine/core/game_app.cpp`
- 修改 `src/engine/input/input_manager.h`
- 修改 `src/engine/input/input_manager.cpp`
- 修改 `src/engine/render/opengl/gl_renderer.h`
- 修改 `src/engine/render/opengl/gl_renderer.cpp`
- 修改 `src/engine/scene/scene.h`
- 修改 `src/engine/scene/scene.cpp`
- 修改 `src/engine/scene/scene_manager.h`
- 修改 `src/engine/scene/scene_manager.cpp`
- 修改 `src/engine/debug/panels/rmlui_debug_panel.h`
- 修改 `src/engine/debug/panels/rmlui_debug_panel.cpp`
- 修改 `src/engine/ui/rmlui/hover_focus_sync_listener.h`
- 修改 `src/engine/ui/rmlui/hover_focus_sync_listener.cpp`

#### 验证

- 项目可编译
- 引擎层场景加载、active scene、输入路由、导航接线仍正常
- `RmlUiDebugPanel` 可继续访问文档状态和 texture filter
- 过渡期 render hook 内同时执行 update + render，行为与旧主循环保持一致
- grep `src/engine/**` 时 `getRmlUILayer()` 为零

#### 完成标记

- [ ] `GameApp` 持有 `rmlui_runtime_`
- [ ] `GameApp` 持有 `rmlui_render_backend_`
- [ ] `GameApp` 在 init 和 resize 后同步 RmlUi viewport
- [ ] `GLRenderer` 新增 `setRmlUiRenderHook(...)`
- [ ] `GLRenderer::present()` 改为调用 RmlUi render hook
- [ ] 过渡期 render hook 暂时同时包含 RmlUi update + render
- [ ] `Context` 新增 `UiServices`
- [ ] `Context::getRmlUi()` 返回 `RmlUiRuntime*`
- [ ] `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()` 切到 `getRmlUi()`
- [ ] `SceneManager::syncRmlActiveScene()` 切到 `getRmlUi()`
- [ ] `InputManager` 的 RmlUi event forwarder 改为使用 `RmlUiRuntime`
- [ ] `UINavigationController` 的导航接线改为连接 `RmlUiRuntime`
- [ ] `HoverFocusSyncListener` 改为依赖 `RmlUiRuntime`
- [ ] `RmlUiDebugPanel` 同时改用 runtime 和 render backend 的正确入口
- [ ] 阶段 A grep 检查通过：`src/engine/**` 中 `getRmlUILayer()` 为零
