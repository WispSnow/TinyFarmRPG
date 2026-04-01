### Phase 1: Runtime / Backend / Viewport 拆分与 Bootstrap

**目标**：先把结构骨架搭起来，但不急着迁移所有调用点。

本阶段完成后，应具备：

- `RmlUiRuntime`
- `RmlUiRenderBackendGl`
- `RmlUiViewport`
- `GameApp::initRmlUi()`
- 仍可工作的 `RmlUILayer` 兼容壳

#### 本阶段要做的事

1. 新增 `RmlUiViewport`
   - 统一封装 viewport 宽高和偏移
   - 供 runtime、render hook、backend 共享

2. 提取 `RmlUiRenderBackendGl`
   - 持有 `RenderInterface_GL3_STB`
   - 提供 `setLogicalSize()`
   - 提供 `setTextureFilterMode() / getTextureFilterMode()`
   - 提供 `render(Rml::Context&, const RmlUiViewport&)`

3. 提取 `RmlUiRuntime`
   - 持有 `Rml::Context`
   - 持有 `SystemInterface_SDL`
   - 接管：
     - 文档管理
     - active scene
     - focus / queued focus
     - navigation
     - `processEvent()`
     - `update()`
     - `syncViewport(...)`

4. 新增 `GameApp::initRmlUi()`
   - 固定初始化顺序：
     - backend 创建
     - runtime 创建
     - `Rml::Initialise()`
     - `Rml::CreateContext()`
     - 默认字体加载
   - 让 `RmlUiRuntime::create(...)` 显式依赖已存在的 render backend interface

5. 迁移 shutdown 责任
   - `Rml::Shutdown()` 改由 `RmlUiRuntime` 销毁时负责
   - backend 只释放 render interface

6. 保留 `RmlUILayer` 兼容壳
   - 内部转发到 runtime + backend
   - 先不删旧接口，保证后续迁移阶段可编译

#### 本阶段不做

- 不调整 `GLRenderer` render hook
- 不新增 `Context::getRmlUi()`
- 不迁移场景和 wrapper 调用点
- 不调整主循环里的 RmlUi update 时机

#### 涉及文件

- 新建 `src/engine/ui/rmlui/rml_ui_viewport.h`
- 新建 `src/engine/ui/rmlui/rml_ui_runtime.h`
- 新建 `src/engine/ui/rmlui/rml_ui_runtime.cpp`
- 新建 `src/engine/ui/rmlui/rml_ui_render_backend_gl.h`
- 新建 `src/engine/ui/rmlui/rml_ui_render_backend_gl.cpp`
- 修改 `src/engine/core/game_app.h`
- 修改 `src/engine/core/game_app.cpp`
- 修改 `src/engine/ui/rmlui/rml_ui_layer.h`
- 修改 `src/engine/ui/rmlui/rml_ui_layer.cpp`

#### 验证

- 项目可编译
- 旧 `RmlUILayer` 路径仍可工作
- 默认字体仍能加载
- `RmlUiRenderBackendGl::render(context, viewport)` 签名确定，并可通过兼容壳或手动接线被调用产生正确的 GL 输出
- `Rml::Initialise()` / `Shutdown()` 顺序明确且对称

#### 完成标记

- [x] 新增 `RmlUiViewport`
- [x] 新增 `RmlUiRenderBackendGl`
- [x] 新增 `RmlUiRuntime`
- [x] `RmlUiRenderBackendGl::render(context, viewport)` 签名稳定
- [x] `GameApp` 新增统一的 `initRmlUi()`
- [x] 默认字体加载迁移到 `GameApp::initRmlUi()`
- [x] `Rml::Shutdown()` 责任迁移到 `RmlUiRuntime`
- [x] 保留一个可工作的 `RmlUILayer` 兼容壳
