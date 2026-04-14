# RmlUi 6.2 集成方案（修订版）

## Context

项目将拓展 RPG 玩法（商店、任务、回合制战斗、技能系统），现有纯代码 UI 已接近扩展上限。RmlUi 提供 HTML/CSS 风格 UI 描述能力，更适合复杂 UI 迭代。本方案目标是完成 **可运行、可交互、可与现有 ImGui 共存** 的基础集成。

---

## 基于 Review 的关键修订

1. **必须恢复 GL 状态**：`RmlUi_Renderer_GL3::BeginFrame()` 会关闭 `GL_FRAMEBUFFER_SRGB`，渲染结束后需在引擎侧显式恢复，避免后续帧颜色空间错误。
2. **修正 `src/CMakeLists.txt` 路径**：不能写 `external/...` 相对路径，必须用 `${CMAKE_SOURCE_DIR}/external/...`。
3. **输入消费语义修正**：RmlUi 消费事件后，`KEY_UP/MOUSE_BUTTON_UP` 仍需进入 InputManager，避免动作卡在 HELD。
4. **补齐 `Context::Update()` 调用时序**：每渲染帧都要有明确 update 点，不只 render。
5. **避免 CMake 全局污染**：`BUILD_SHARED_LIBS` 与 RMLUI 选项需保存/恢复，不影响主工程其它依赖。
6. **补齐验证矩阵**：新增 `ENABLE_RMLUI=OFF` 回归、输入边界、渲染状态恢复验证。

---

## 架构决策

### 1) CMake：独立模块 `cmake/RmlUiDependencies.cmake`

与 Effekseer / Scripting 同级，避免继续膨胀 `Dependencies.cmake`。

### 2) 不使用 `rmlui_backend_SDL_GL3` 目标

保持“只用核心库 + 自选后端源码”模式，避免引入 SDL_image、窗口管理代码与内置 GL loader 的冲突。

### 3) GL Loader 统一使用项目 GLAD

编译 `RmlUi_Renderer_GL3.cpp` 时定义：
- `RMLUI_GL3_CUSTOM_LOADER=<glad/glad.h>`

确保 RmlUi 不走内置 GLAD。

### 4) 纹理加载统一使用 stb_image

继承 `RenderInterface_GL3`，override `LoadTexture()`，复用项目现有解码路径并做 premultiplied alpha 处理。

### 5) 事件路由采用“可传播”契约

新增 RmlUi 事件回调（bool 返回值）：
- `true`：继续传播
- `false`：被 UI 消费

但对 `KEY_UP` / `MOUSE_BUTTON_UP` 采用白名单放行，避免输入状态卡死。

---

## 实施步骤（执行状态：2026-02-27）

### Step 1 [x]: 新建 `cmake/RmlUiDependencies.cmake`

已完成 `setup_rmlui_dependencies()`，包含：

- 幂等检查：若 `TARGET rmlui_core` / `rmlui_debugger` 已存在则直接返回。
- 源码解析策略：优先本地源码，缺失时在线拉取。
  - 本地目录变量：`RMLUI_LOCAL_SOURCE_DIR`（默认 `external/RmlUi-6.2`）
  - 在线仓库变量：`RMLUI_GIT_REPOSITORY=https://github.com/mikke89/RmlUi.git`
  - 版本变量：`RMLUI_GIT_TAG=6.2`
  - 解析结果：`RMLUI_SOURCE_DIR_RESOLVED`
- 保存并恢复以下变量（含 CACHE）：
  - `BUILD_SHARED_LIBS`
  - `RMLUI_FONT_ENGINE`
  - `RMLUI_SAMPLES`
  - `RMLUI_TESTS`
  - `RMLUI_LUA_BINDINGS`
  - `RMLUI_LOTTIE_PLUGIN`
  - `RMLUI_SVG_PLUGIN`
  - `RMLUI_COMPILER_OPTIONS`
  - macOS 下 `CMAKE_OSX_DEPLOYMENT_TARGET`
- 本模块内强制设置：
  - `RMLUI_FONT_ENGINE=freetype`
  - `BUILD_SHARED_LIBS=OFF`
  - `RMLUI_SAMPLES=OFF`
  - `RMLUI_TESTS=OFF`
  - `RMLUI_LUA_BINDINGS=OFF`
  - `RMLUI_LOTTIE_PLUGIN=OFF`
  - `RMLUI_SVG_PLUGIN=OFF`
  - `RMLUI_COMPILER_OPTIONS=OFF`
- 调用：`add_subdirectory(${RMLUI_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps/rmlui-build EXCLUDE_FROM_ALL)`
- 校验目标：`rmlui_core`、`rmlui_debugger`
- 不额外导出全局 include 变量，避免无效状态扩散；由目标链接关系传递头文件路径。

### Step 2 [x]: 修改根 `CMakeLists.txt`

- 新增开关：`option(ENABLE_RMLUI "启用 RmlUI" ON)`。
- 引入模块：`include(cmake/RmlUiDependencies.cmake)`。
- 条件调用：`if(ENABLE_RMLUI) setup_rmlui_dependencies() endif()`。
- 链接与宏：
  - `target_link_libraries(engine PUBLIC rmlui_core rmlui_debugger)`
  - `target_compile_definitions(engine PUBLIC TF_ENABLE_RMLUI)`

### Step 3 [x]: 修改 `src/CMakeLists.txt`（后端源码并入 engine）

在 `if(ENABLE_RMLUI)` 内已完成：

- 通过 `_rmlui_source_root` 选择后端源码目录（优先 `RMLUI_SOURCE_DIR_RESOLVED`，兼容在线拉取场景）。
- 添加源码：
  - `${_rmlui_source_root}/Backends/RmlUi_Platform_SDL.cpp`
  - `${_rmlui_source_root}/Backends/RmlUi_Renderer_GL3.cpp`
- 添加 include：
  - `${_rmlui_source_root}/Backends`
- 设置编译定义：
  - `RMLUI_GL3_CUSTOM_LOADER=<glad/glad.h>`
  - `RMLUI_SDL_VERSION_MAJOR=3`

### Step 4 [x]: 创建 RmlUi 封装层

**已新建**
- `src/engine/ui/rmlui/rml_ui_layer.h`
- `src/engine/ui/rmlui/rml_ui_layer.cpp`
- `src/engine/ui/rmlui/render_interface_gl3_stb.h`
- `src/engine/ui/rmlui/render_interface_gl3_stb.cpp`

`RmlUILayer` 已实现：

- 生命周期：`create()` / `clean()`
- 输入：`bool processEvent(SDL_Event& event)`（返回是否继续传播）
- 帧驱动：`update()` + `render()`
- 视口：`setViewport(int w, int h, int x, int y)`（支持 letterbox 偏移）
- 访问：`Rml::Context* getContext()`

`RenderInterface_GL3_STB` 已实现：

- override `LoadTexture()`
- 通过 `Rml::FileInterface` 读文件，`stb_image` 解码
- 转 premultiplied alpha
- 调用 `GenerateTexture()` 生成 GL 纹理

### Step 5 [x]: 集成到 `GLRenderer`

**已修改**
- `src/engine/render/opengl/gl_renderer.h`
- `src/engine/render/opengl/gl_renderer.cpp`

已完成要点：

- 新增成员（`#ifdef TF_ENABLE_RMLUI`）：`std::unique_ptr<RmlUILayer> rmlui_layer_`
- `init()`：RenderContext/Viewport 就绪后初始化 RmlUILayer
- `present()`：在 `ui_pass_->flush(viewport)` 之后、ImGui 之前执行
  - `rmlui_layer_->setViewport(...)`
  - `rmlui_layer_->update()`
  - `rmlui_layer_->render()`
  - **显式恢复** `glEnable(GL_FRAMEBUFFER_SRGB)`
- `resize()`：同步 RmlUI viewport
- `clean()`：在 RenderContext 清理前先 `rmlui_layer_->clean()`
- 提供事件入口：`bool handleRmlUiEvent(SDL_Event& event)`

渲染顺序已落地：
`... -> UIPass -> RmlUI -> ImGui -> SwapWindow`

### Step 6 [x]: 集成输入事件路由

**已修改**
- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `src/engine/core/game_app.cpp`

已完成要点：

- 新增 `setRmlUiEventForwarder(...)`
- `sampleInputEvents()` 顺序：
  - RmlUi callback（先）
  - ImGui callback（仅在可传播时）
  - InputManager `processEvent(...)`
- RmlUi 消费事件时：
  - 默认跳过映射处理
  - **放行 `KEY_UP` / `MOUSE_BUTTON_UP`**，避免 HELD 卡死
- `GameApp::initInputManager()` 已注册 RmlUi + ImGui forwarder

### Step 7 [x]: 最小可视化验证资源

**已新增**
- `ui/rmlui/demo.rml`
- `ui/rmlui/demo.rcss`

并在 `RmlUILayer::init()` 中加载 demo 文档（用于首轮可视化联调）。已修复样式相对路径与字体声明问题。

### Step 8 [x]: 验证清单

1. **构建验证** ✅
   - `ENABLE_RMLUI=ON` 构建通过
   - `ENABLE_RMLUI=OFF` 构建通过（无回归）
2. **运行验证** ✅
   - Rml demo 文档可见且可交互（字体/样式加载正常）
3. **输入验证** ✅
   - Rml 消费按下后，释放事件仍能正确恢复动作状态
4. **渲染状态验证** ✅
   - Rml 渲染后 sRGB 状态恢复，后续帧色彩正常
5. **共存验证** ✅
   - ImGui 与 RmlUi 共存正常
6. **在线拉取验证** ✅
   - 本地源码缺失时可从 `https://github.com/mikke89/RmlUi` 拉取 `6.2` 并构建

### Step 9 [x]: 自动化测试

- `tests/engine/render/rmlui_pipeline_stage_test.cpp`
  - 校验 `present()` 中 RmlUI 调用顺序（位于 UI pass 与 ImGui 之间）
  - 校验 `clean()` 释放顺序（RmlUILayer 在 RenderContext 前）
- `tests/engine/input/input_manager_rmlui_routing_test.cpp`
  - 校验“消费按下 + 放行释放”行为
- 已加入 `tests/CMakeLists.txt` 并通过验证。

---

## 涉及文件

| 操作 | 文件 |
|------|------|
| 新建 | `cmake/RmlUiDependencies.cmake` |
| 修改 | `CMakeLists.txt` |
| 修改 | `src/CMakeLists.txt` |
| 新建 | `src/engine/ui/rmlui/rml_ui_layer.h` |
| 新建 | `src/engine/ui/rmlui/rml_ui_layer.cpp` |
| 新建 | `src/engine/ui/rmlui/render_interface_gl3_stb.h` |
| 新建 | `src/engine/ui/rmlui/render_interface_gl3_stb.cpp` |
| 修改 | `src/engine/render/opengl/gl_renderer.h` |
| 修改 | `src/engine/render/opengl/gl_renderer.cpp` |
| 修改 | `src/engine/input/input_manager.h` |
| 修改 | `src/engine/input/input_manager.cpp` |
| 修改 | `src/engine/core/game_app.cpp` |
| 新建 | `ui/rmlui/demo.rml` |
| 新建 | `ui/rmlui/demo.rcss` |
| 新建 | `tests/engine/render/rmlui_pipeline_stage_test.cpp` |
| 新建 | `tests/engine/input/input_manager_rmlui_routing_test.cpp` |
