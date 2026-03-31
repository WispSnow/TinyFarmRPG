# RmlUi Runtime / Render Backend / GameScene UI Controller 重构计划

## Context

当前项目里的 RmlUi 已经能工作，但结构上混了三层职责：

- retained-mode UI runtime
  - `Rml::Context`
  - 文档加载与 owner 管理
  - active scene 交互隔离
  - 焦点查询、直接聚焦、延迟聚焦
  - 导航与确认
  - SDL 事件转发
  - `context->Update()`
- GL 渲染 backend
  - `RenderInterface_GL3_STB`
  - viewport / logical size
  - texture filter
  - `BeginFrame / Render / EndFrame`
- 游戏层 UI 组合
  - `GameScene` 中的 HUD / tooltip / dialogue bubble / overlay / fade

当前 `RmlUILayer` 把前两层全部揉在一起，并且被 `GLRenderer` 持有。结果是：

1. 上层业务代码要通过 `Context -> GLRenderer -> RmlUILayer` 才能操作 UI runtime。
2. 官方 sample 里清晰分开的 `ProcessEvents / Update / Render`，在本项目里变成了：
   - `ProcessEvents`: `GameApp::handleEvents() -> InputManager -> RmlUILayer::processEvent()`
   - `Update`: 藏在 `GLRenderer::present() -> RmlUILayer::update()`
   - `Render`: 藏在 `GLRenderer::present() -> RmlUILayer::render()`
3. `GameScene` 自己还承担了大量 UI 组合逻辑，类职责过重。

这也是当前结构不如 ImGui 清晰的根因。ImGui 在本项目里本来就是 debug immediate-mode UI，放在 renderer 末端统一收尾是合理的；RmlUi 则是 retained-mode runtime，逻辑和渲染本来就应该分开。

因此，这次计划不再停留在“把 ownership 从 `GLRenderer` 挪出去”这一层，而是直接做更干净的三层拆分：

1. `RmlUiRuntime`
2. `RmlUiRenderBackendGl`
3. `GameSceneUiController`

## 实现思路

### 1. 把当前 `RmlUILayer` 正式拆成 runtime 和 render backend

最终目标不再是保留一个“功能全面的 `RmlUILayer`”，而是把它拆成两个语义明确的类：

- `RmlUiRuntime`
  - 面向场景和 UI wrapper
  - 负责 `Rml::Context` 生命周期
  - 负责文档管理、scene ownership、焦点、导航、事件处理、`Update()`
  - 负责 viewport state 对输入坐标和 `Context` 尺寸的同步
  - 对外暴露 `getContext()`
- `RmlUiRenderBackendGl`
  - 面向渲染层
  - 负责 `RenderInterface_GL3_STB`
  - 负责 logical size、texture filter
  - 负责按给定 viewport 把 `Rml::Context` 渲染到当前 GL frame

对应地，下面这组 API 应归入 `RmlUiRuntime`：

- `processEvent(SDL_Event&)`
- `update()`
- `getContext()`
- `syncViewport(...)`
- `loadDocument() / unloadDocument() / unloadDocumentsByOwner()`
- `showDocument() / hideDocument()`
- `setActiveScene()`
- `getFocusedElement()`
- `focusElement() / focusElementById() / focusFirstEnabledElementByClass()`
- `queueFocusElement() / queueFocusElementById() / queueFocusFirstEnabledElementByClass()`
- `navigateUp()/Down()/Left()/Right()`
- `confirmFocusedElement()`

而下面这组 API 应归入 `RmlUiRenderBackendGl`：

- `setLogicalSize()`
- `render(Rml::Context&, const RmlUiViewport&)`
- `setTextureFilterMode() / getTextureFilterMode()`

这一步完成后，游戏逻辑将不再碰 renderer-side backend，renderer 也不再暴露 UI 业务入口。

### 2. RmlUi 全局初始化和销毁由 `GameApp::initRmlUi()` 统一协调

`RmlUi` 这里有一个不能忽略的现实约束：`Rml::Initialise()` 是全局初始化，而且要求 `SystemInterface` 和 `RenderInterface` 都已经注册完成。

当前代码里这几个步骤都藏在 `RmlUILayer::init()` 内部；拆分后必须把这个顺序显式固定下来。

推荐方案：

- `GameApp` 新增单一入口：`initRmlUi()`
- `RmlUiRenderBackendGl` 先创建自己的 `RenderInterface_GL3_STB`
- `RmlUiRuntime::create(...)` 显式接收 backend 的 render interface 引用，仅用于初始化阶段
- `RmlUiRuntime` 内部完成：
  - `SystemInterface_SDL` 创建
  - `Rml::SetSystemInterface(...)`
  - `Rml::SetRenderInterface(...)`
  - `Rml::Initialise()`
  - `Rml::CreateContext(...)`
- 字体加载不放在 runtime 或 backend 构造函数中，而是放在 `GameApp::initRmlUi()` 末尾统一执行

这样可以同时满足三件事：

1. 用一个统一入口锁定初始化顺序
2. 用类型签名体现“runtime 创建依赖一个已存在的 render backend”
3. 避免字体加载和全局初始化散落在多个构造函数里

与之对称地，`Rml::Shutdown()` 应由 `RmlUiRuntime` 在销毁阶段负责调用，因为它拥有 `Rml::Context` 生命周期。

销毁顺序必须固定为：

1. `rmlui_runtime_` 调用 `Rml::Shutdown()`
2. `rmlui_render_backend_` 释放 render interface
3. `gl_renderer_` 最后销毁自身 GL 资源

### 3. viewport 责任拆开：runtime 持有状态，backend 在 render 时消费

当前 `setViewport()` 同时承担三件事：

- 鼠标事件坐标校正
- `Context` 尺寸和 `dp_ratio` 更新
- render interface 的 viewport 设置

因此它不能被简单地整个塞进 render backend。

推荐拆法：

- `RmlUiRuntime`
  - 持有 `RmlUiViewport` 状态
  - `syncViewport(...)` 负责事件坐标映射、`Context::SetDimensions()`、`SetDensityIndependentPixelRatio()`
- `RmlUiRenderBackendGl`
  - `render(context, viewport)` 时消费物理 viewport
  - 在 `BeginFrame/EndFrame` 前后应用正确的 render viewport
- `GameApp`
  - 作为协调者，在初始化后和窗口 resize 后，把 renderer 当前 viewport 同步给 runtime

也就是说：

- runtime 负责“UI 逻辑看到的坐标空间”
- backend 负责“UI 最终被画到哪个物理区域”

不要让 backend 反过来查询 `GLRenderer` 内部状态。

### 4. 主循环显式恢复 `ProcessEvents / Update / Render` 三段式

这次重构的另一个关键目标，是把当前藏在 `GLRenderer::present()` 里的 RmlUi 运行阶段重新显式化。

目标帧流程：

1. `handleEvents()`
   - `InputManager::sampleInputEvents()`
   - 事件经回调进入 `RmlUiRuntime::processEvent()`
2. `update() / updateFrame()`
   - 先跑场景逻辑
   - 再显式调用 `RmlUiRuntime::update()`
3. `render()`
   - 场景世界内容绘制
   - `GLRenderer` 完成世界合成
   - `RmlUiRenderBackendGl` 渲染 RmlUi
   - ImGui debug UI
   - swap

也就是说，RmlUi 的 `Update()` 不再从属于 `GLRenderer::present()`；它应该回到 `GameApp` 的 update 阶段。

### 5. `Context` 只暴露 runtime，不暴露 backend

`Context` 中应该新增 UI 模块入口，但这个入口应该是 runtime，而不是 render backend。

建议新增：

```cpp
struct UiServices {
    engine::ui::rmlui::RmlUiRuntime* rmlui{nullptr};
};
```

`Context` 增加：

- `UiServices ui_;`
- `[[nodiscard]] const UiServices& ui() const`
- `[[nodiscard]] engine::ui::rmlui::RmlUiRuntime* getRmlUi() const`

这里继续使用可空指针而不是引用，原因仍然成立：

- headless / 无 UI runtime 时自然退化
- 单元测试更容易构造
- 上层能显式处理“当前没有 RmlUi runtime”

但 `Context` 不提供 `getRmlUiRenderBackend()`。render backend 不属于业务依赖入口。

### 6. ownership 由 `GameApp` 统一管理，`GLRenderer` 只保留 render hook

为了让依赖方向正确，建议由 `GameApp` 作为 composition root 直接持有：

- `std::unique_ptr<RmlUiRuntime> rmlui_runtime_;`
- `std::unique_ptr<RmlUiRenderBackendGl> rmlui_render_backend_;`

推荐初始化顺序：

1. `initGLRenderer()`
2. `initRmlUi()`
3. `initInputManager()`
4. `initUINavigationController()`
5. `initContext()`

推荐销毁顺序：

1. `scene_manager_`
2. `context_`
3. `rmlui_runtime_`
4. `rmlui_render_backend_`
5. `gl_renderer_`

`GLRenderer` 在最终结构里只需要承担两件事：

- 世界渲染与合成
- 在正确的时机调用 RmlUi render hook 和 ImGui

它不再：

- 创建 RmlUi runtime
- 销毁 RmlUi runtime
- 暴露 `getRmlUi()` 这类业务入口
- 持有焦点、文档、导航等 UI manager 能力

这里推荐采用 callback 注入，而不是让 `GLRenderer` 同时认识 runtime 和 backend。

建议新增：

```cpp
using RmlUiRenderHook = std::function<void(const RmlUiViewport&)>;
```

由 `GameApp` 在初始化时注入：

```cpp
gl_renderer_->setRmlUiRenderHook([this](const RmlUiViewport& viewport) {
    if (!rmlui_runtime_ || !rmlui_render_backend_) {
        return;
    }
    if (auto* context = rmlui_runtime_->getContext()) {
        rmlui_render_backend_->render(*context, viewport);
    }
});
```

这样：

- `GLRenderer` 只知道“这里有个 UI render hook”
- render backend 不需要反查 renderer
- runtime 和 backend 的组合关系留在 `GameApp`

### 7. `RmlUiService` 不再需要，重点改成真正的职责切分

审阅意见里对 thin forwarding layer 的担忧是合理的。单独加一个 `RmlUiService` 只会把旧结构再包一层，并不能解决：

- ownership 方向错误
- `Update / Render` 阶段混在一起
- render API 和 runtime API 混在同一对象里

因此本次直接跳过 service 层，改成真实的 runtime / backend 切分。

### 8. 焦点 API、`HoverFocusSyncListener`、`InventoryMenuScene` 都必须显式纳入

这次不能只迁移文档加载入口。以下高频路径必须作为首批完整迁移范围：

- direct focus
- queued focus
- `getFocusedElement()`
- `HoverFocusSyncListener`
- `InventoryMenuScene` 的 tooltip、拖拽、焦点恢复、`Rml::DataTypeRegister`

其中 `HoverFocusSyncListener` 的最终依赖应改为 `RmlUiRuntime&`，而不是旧的 `RmlUILayer&`。

关于 `queueFocus*()` 的时序，需要额外加一层验证。

当前 `pending focus` 是在 `update()` 内 `context_->Update()` 之后处理的；把 `update()` 从 `present()` 提前到 `GameApp` update 阶段后，焦点应用会比现在更早。

基于当前代码检索，现有 `queueFocus*()` 调用主要位于：

- 场景 `initUI()`
- 点击/菜单状态切换逻辑
- action menu 打开/关闭逻辑

没有发现明确放在 render 阶段的调用点。

但实施时仍应把这一条写成显式检查项：

- 先完成一次 `queueFocus*()` 调用点审计
- 若发现仍有 render 阶段的 late queue，再补一个“render 前二次 drain”过渡策略
- 最终目标仍是把 queue 行为限制在 init/update/event 处理阶段

### 9. `GameSceneUiController` 收拢游戏层 UI 组合

`GameScene` 当前最明显的问题不是“缺一个 UI service”，而是场景级 UI 组合职责太重。

建议新增：

- `game::ui::GameSceneUiController`

它直接持有 `Context&`，不把 `InputManager&`、`Renderer&`、`RmlUiRuntime&` 拆成零散参数往下传。

建议职责：

- 创建和销毁
  - `TimeClockHud`
  - `HotbarUI`
  - `ItemTooltipUI`
  - `DialogueBubbleController`
  - `DialogueBubbleView`
  - overlay prompt bar 文档与 data bridge
  - `RmlScreenFade`
- 每帧更新
  - `update(float delta_time, entt::registry&)`
  - `refreshAnchoredWidgets(Camera&, float interpolation_alpha)`
- 对 `GameScene` 暴露高层接口
  - `toggleHotbar()`
  - `applyHotbarChanged(...)`
  - `applyHotbarSlotChanged(...)`
  - `setPromptBarVisible(bool visible)`
  - `[[nodiscard]] engine::ui::IScreenFade* screenFade() const`

### 10. 迁移过程保留一个短期兼容层，但最终目标是删除 `RmlUILayer`

虽然项目不要求向后兼容，但分步实施仍然应该保持每一步可编译。

因此推荐采用两阶段迁移：

- 阶段 A
  - 先引入 `RmlUiRuntime` 和 `RmlUiRenderBackendGl`
  - 旧 `RmlUILayer` 暂时保留为过渡兼容壳，内部转发到新对象
  - 先把调用点批量迁走
- 阶段 B
  - 删除 `RmlUILayer`
  - 删除 `GLRenderer::getRmlUILayer()`
  - 删除所有 renderer-backdoor 式调用

这样可以兼顾最终结构干净和中途分支稳定。

同时把阶段边界写明确：

- 阶段 A 完成标志
  - `src/engine/**` 中 `getRmlUILayer()` 调用为零
  - 引擎层全部改走 `Context::getRmlUi()` 或 render hook
  - 兼容壳里只剩游戏层调用
- 阶段 B 启动条件
  - 全仓库 `getRmlUILayer()` 调用为零
  - 可以删除兼容壳与 renderer backdoor

### 11. `RmlUiDebugPanel` 是少数同时需要 runtime 和 backend 的类

大多数业务对象只应该看到 runtime。

`RmlUiDebugPanel` 是一个例外，因为它同时需要：

- runtime
  - 已加载文档
  - data binding 测试
- backend
  - texture filter mode

因此建议它采用混合注入：

- 通过 `Context` 访问 runtime
- 通过构造参数接收 `RmlUiRenderBackendGl*`

这样能避免为了一个 debug panel 而把 backend 暴露进 `Context`。

### 12. 测试重点放在“空 runtime 安全退化”和“阶段接线正确”

不把测试目标放在 headless 环境里真正初始化整套 RmlUi + OpenGL。

本次测试重点改为：

- `Context::getRmlUi() == nullptr` 时，`Scene` / `SceneManager` / UI wrapper 安全 no-op
- `InputManager` 的 RmlUi forwarder 改到 runtime 后，空 runtime 不崩溃
- `UINavigationController` 接到 runtime 后，签名与 sink 兼容
- `GameSceneUiController` 在有无 runtime 两种情况下都能安全构造与清理

## 需要新增的文件

- `src/engine/ui/rmlui/rml_ui_viewport.h`
- `src/engine/ui/rmlui/rml_ui_runtime.h`
- `src/engine/ui/rmlui/rml_ui_runtime.cpp`
- `src/engine/ui/rmlui/rml_ui_render_backend_gl.h`
- `src/engine/ui/rmlui/rml_ui_render_backend_gl.cpp`
- `src/game/ui/game_scene_ui_controller.h`
- `src/game/ui/game_scene_ui_controller.cpp`
- `plans/rmlui-service-and-game-scene-ui-refactor.md`

建议新增测试文件：

- `tests/engine/ui/rmlui_runtime_access_test.cpp`
- `tests/game/game_scene_ui_controller_smoke_test.cpp`

## 预计修改的主要文件

- `src/engine/core/context_modules.h`
- `src/engine/core/context.h`
- `src/engine/core/context.cpp`
- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`
- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `src/engine/render/renderer.h`
- `src/engine/render/renderer.cpp`
- `src/engine/render/opengl/gl_renderer.h`
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/engine/scene/scene.h`
- `src/engine/scene/scene.cpp`
- `src/engine/scene/scene_manager.h`
- `src/engine/scene/scene_manager.cpp`
- `src/engine/debug/panels/rmlui_debug_panel.h`
- `src/engine/debug/panels/rmlui_debug_panel.cpp`
- `src/engine/ui/rmlui/hover_focus_sync_listener.h`
- `src/engine/ui/rmlui/hover_focus_sync_listener.cpp`
- `src/engine/ui/rmlui/rml_screen_fade.h`
- `src/engine/ui/rmlui/rml_screen_fade.cpp`
- `src/engine/ui/rmlui/rml_ui_layer.h`
- `src/engine/ui/rmlui/rml_ui_layer.cpp`
- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- `src/game/scene/title_scene.cpp`
- `src/game/scene/pause_menu_scene.cpp`
- `src/game/scene/save_slot_select_scene.cpp`
- `src/game/scene/rest_dialog_scene.cpp`
- `src/game/scene/battle_scene.cpp`
- `src/game/scene/inventory_menu_scene.cpp`
- `src/game/ui/hotbar_ui.h`
- `src/game/ui/hotbar_ui.cpp`
- `src/game/ui/time_clock_hud.h`
- `src/game/ui/time_clock_hud.cpp`
- `src/game/ui/item_tooltip_ui.h`
- `src/game/ui/item_tooltip_ui.cpp`
- `src/game/ui/dialogue_bubble_view.h`
- `src/game/ui/dialogue_bubble_view.cpp`

## 实现步骤

### Step 1: 提取 `RmlUiRenderBackendGl`

先从旧类中拆出 `RenderInterface_GL3_STB`、texture filter、logical size 和 `render(context, viewport)`。

### Step 2: 提取 `RmlUiRuntime`

再拆出 runtime 侧职责，让文档、焦点、导航、事件处理、`Update()`、`syncViewport(...)` 都有独立归宿。

### Step 3: 新增 `RmlUiViewport` 值对象

把物理 viewport 尺寸与偏移整理成明确的数据对象，供 runtime、render hook、backend 共享，避免四个裸整数在多处传播。

### Step 4: `GameApp::initRmlUi()` 接管全局 bootstrap

新增统一的 `initRmlUi()`，在这里固定：

- backend 创建
- runtime 创建
- `Rml::Initialise()`
- `Rml::CreateContext()`
- 默认字体加载

同时明确 shutdown 对称关系。

### Step 5: 暂时保留 `RmlUILayer` 兼容壳

在调用点尚未迁完之前，让旧 `RmlUILayer` 作为短期 facade 存在，内部转发到 runtime 和 render backend，保证中间状态可编译。

### Step 6: `GameApp` 接管 runtime 和 backend 的 ownership

新增 `rmlui_runtime_` 和 `rmlui_render_backend_`，并调整初始化、销毁顺序，以及窗口 resize 后的 viewport 同步。

### Step 7: `GLRenderer` 改为 render hook 接入

新增 `setRmlUiRenderHook(...)`，让 `GLRenderer` 在世界合成后只触发 hook，不再持有或查询 RmlUi runtime。

### Step 8: `Context` 新增 `UiServices` 与 `getRmlUi()`

让场景、wrapper、bridge、listener 都能通过 `Context` 直接拿到 runtime，不再走 `GLRenderer`。

### Step 9: 迁移引擎层公共调用点

迁移以下公共入口：

- `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()`
- `SceneManager::syncRmlActiveScene()`
- `InputManager` 的 RmlUi forwarder
- `UINavigationController` 的导航接线
- `RmlUiDebugPanel`
- `HoverFocusSyncListener`

### Step 10: 阶段 A 检查点

执行一次 grep 检查，确认：

- `src/engine/**` 中 `getRmlUILayer()` 调用为零
- 引擎层只剩 runtime 入口和 render hook

完成这一检查后，再进入游戏层迁移。

### Step 11: 调整主循环中的 RmlUi update 时机

在 ownership、render hook 和引擎层接线稳定后，再把 RmlUi `update()` 从 `GLRenderer::present()` 移到 `GameApp` update 阶段。

这一步同时完成：

- `queueFocus*()` 调用点复查
- 若必要则增加 render 前二次 drain 过渡策略

### Step 12: 迁移菜单场景与 `InventoryMenuScene`

把 Title / Pause / SaveSlotSelect / Rest / Battle / Inventory 的 runtime 调用点切到 `Context::getRmlUi()`。

重点验证：

- `InventoryMenuScene` 的拖拽
- 焦点恢复
- tooltip
- `Rml::DataTypeRegister`

### Step 13: 新增 `GameSceneUiController`

把 `GameScene` 的 HUD / overlay / tooltip / dialogue bubble / fade 组合逻辑移出去。

### Step 14: 收缩 `GameScene`

让 `GameScene` 只保留玩法场景职责，通过 `GameSceneUiController` 驱动场景级 UI。

### Step 15: 阶段 B 检查点

执行全仓库 grep，确认 `getRmlUILayer()` 调用为零。

### Step 16: 删除兼容壳与旧 renderer 入口

确认所有调用点完成迁移后，删除：

- `RmlUILayer`
- `GLRenderer::getRmlUILayer()`
- 所有从 renderer 间接拿 runtime 的路径

### Step 17: 补测试与清理注释

补齐空 runtime 退化、接线路径和 `GameSceneUiController` smoke test，并在关键头文件写清职责边界。

## 待办追踪

- [ ] 新增 `RmlUiRuntime`
- [ ] 新增 `RmlUiRenderBackendGl`
- [ ] 新增 `RmlUiViewport`
- [ ] 将 `processEvent()` 迁入 `RmlUiRuntime`
- [ ] 将 `update()` 迁入 `RmlUiRuntime`
- [ ] 将 `syncViewport(...)` 迁入 `RmlUiRuntime`
- [ ] 将文档管理 API 迁入 `RmlUiRuntime`
- [ ] 将 active scene API 迁入 `RmlUiRuntime`
- [ ] 将全部焦点 API 迁入 `RmlUiRuntime`
- [ ] 将导航与确认 API 迁入 `RmlUiRuntime`
- [ ] 将 `render()` 迁入 `RmlUiRenderBackendGl`
- [ ] 将 logical size API 迁入 `RmlUiRenderBackendGl`
- [ ] 将 texture filter API 迁入 `RmlUiRenderBackendGl`
- [ ] `RmlUiRuntime::create(...)` 显式依赖已创建的 render backend 接口
- [ ] `GameApp` 新增统一的 `initRmlUi()`
- [ ] 默认字体加载迁移到 `GameApp::initRmlUi()`
- [ ] `Rml::Shutdown()` 责任迁移到 `RmlUiRuntime`
- [ ] 保留一个短期 `RmlUILayer` 兼容壳
- [ ] `GameApp` 新增 `rmlui_runtime_`
- [ ] `GameApp` 新增 `rmlui_render_backend_`
- [ ] `GameApp` 调整 RmlUi 初始化顺序
- [ ] `GameApp` 调整 RmlUi 销毁顺序
- [ ] `GameApp` 在 init 和 resize 后同步 RmlUi viewport
- [ ] `GLRenderer` 新增 `setRmlUiRenderHook(...)`
- [ ] `GLRenderer::present()` 改为调用 RmlUi render hook
- [ ] `GameApp` 显式增加 RmlUi update 阶段
- [ ] `queueFocus*()` 调用点审计完成
- [ ] 若有 late queue，补 render 前二次 drain 过渡策略
- [ ] `Context` 新增 `UiServices`
- [ ] `Context::getRmlUi()` 返回 `RmlUiRuntime*`
- [ ] `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()` 切到 `getRmlUi()`
- [ ] `SceneManager::syncRmlActiveScene()` 切到 `getRmlUi()`
- [ ] `InputManager` 的 RmlUi event forwarder 改为使用 `RmlUiRuntime`
- [ ] `UINavigationController` 的导航接线改为连接 `RmlUiRuntime`
- [ ] `HoverFocusSyncListener` 改为依赖 `RmlUiRuntime`
- [ ] `RmlUiDebugPanel` 同时改用 runtime 和 render backend 的正确入口
- [ ] 阶段 A grep 检查通过：`src/engine/**` 中 `getRmlUILayer()` 为零
- [ ] `TitleScene` 迁移
- [ ] `PauseMenuScene` 迁移
- [ ] `SaveSlotSelectScene` 迁移
- [ ] `RestDialogScene` 迁移
- [ ] `BattleScene` 迁移
- [ ] `InventoryMenuScene` 迁移
- [ ] `TimeClockHud` 迁移
- [ ] `HotbarUI` 迁移
- [ ] `ItemTooltipUI` 迁移
- [ ] `DialogueBubbleView` 迁移
- [ ] `RmlScreenFade` 迁移
- [ ] 新增 `GameSceneUiController`
- [ ] `GameScene` UI 初始化、update、render 前同步、clean 逻辑迁移到 controller
- [ ] 阶段 B grep 检查通过：全仓库 `getRmlUILayer()` 为零
- [ ] 删除 `RmlUILayer`
- [ ] 删除 `GLRenderer::getRmlUILayer()`
- [ ] 新增 `rmlui_runtime_access_test.cpp`
- [ ] 新增 `game_scene_ui_controller_smoke_test.cpp`

## 疑问

暂无阻塞性疑问。若后续实现时发现 `RmlUiRenderBackendGl` 的初始化必须进一步抽出一个更上层的 backend bootstrap，再追加一份小的补充计划即可，不影响当前主方向。
