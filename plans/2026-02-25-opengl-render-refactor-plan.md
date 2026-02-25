# 2026-02-25 OpenGL 渲染模块重构优化计划（整合版）

## 背景
- 目标模块：`src/engine/render/opengl`
- 输入来源：
  - 本次 Codex 审查（生命周期/Headless/状态管理/可扩展性）
  - 你补充的 Claude 11 条建议（含优先级表）
- 项目约束：按最优方案重构，**不考虑向后兼容**。

## 综合分析（两份建议合并）

### A. 立即修复的真实问题（P0）
1. Bloom HDR 精度丢失（Claude #2）：`Emissive` 为 `RGBA16F`，但 Bloom ping-pong 是 `RGB8`，会截断高亮。
2. SpriteBatch 冗余上传（Claude #1）：`flush` 中可能出现一次 frame 两次 vertex 上传。
3. GL 资源销毁顺序风险（Codex）：`GLRenderer::clean()` 与 `ShaderProgram` 析构时机需确保 context 仍有效。
4. Headless 光照入口空指针风险（Codex）：`addPointLight/addSpotLight/addDirectionalLight` 需统一 no-op 护栏。

### B. 稳定性与可维护性改进（P1）
5. 混合状态恢复脆弱（Claude #6）：需要 RAII guard。
6. FBO 创建样板重复（Claude #5）：应提取通用 helper。
7. 全屏 quad 重复资源（Claude #3）+ 默认 1x1 纹理重复（Claude #4）：应集中管理。
8. `clear()` 后 FBO 绑定恢复不一致（Claude #11）：统一“谁绑定谁恢复”。
9. CompositePass 每帧重复设置固定 sampler（Claude #7）：去掉重复设置。
10. ShaderProgram 注释与实现不一致（Claude #8）：实现缓存或修正文档。

### C. 中长期扩展优化（P2）
11. Pass 接口不统一（Claude #9）+ 动态组合需求（Codex）：为后续 VFX/粒子阶段准备统一 Pass 契约。
12. Sprite 顶点格式压缩（Claude #10）：可在大量精灵场景再推进。

## 实现思路
采用“三阶段推进”，先修 bug，再收敛基础设施，最后做结构升级。

### 阶段 1：P0 快速修复包（先落地）
- 修复 Bloom 纹理格式：`GL_RGB8 -> GL_RGB16F`（或 `GL_R11F_G11F_B10F`，默认选 `RGB16F`）。
- 合并 SpriteBatch `flush` 容量判断：一次 `ensureCapacity`、一次上传。
- 调整 GLRenderer 清理顺序：所有 GL 资源先释放，`RenderContext` 最后释放。
- 统一 headless 保护：所有 pass 调用前检查对象有效性，保证 no-op。
- 补齐 `LightingPass::clear` / `EmissivePass::clear` 的 FBO 解绑恢复。
- 删除 CompositePass 每帧重复 sampler uniform 设置。

### 阶段 2：P1 基础设施收敛包
- 新增 `ScopedBlendFunc`（可扩展为 `ScopedGLState`），替换手工 set/restore。
- 提取 `createFBOWithColorAttachment(...)` 公共工具，统一 Scene/Lighting/Emissive。
- 提取共享 `FullscreenQuad`，替换 Bloom/Composite 的静态全屏 VAO/VBO（LightingPass 因每帧动态更新 world/screen 空间顶点，保留独立 `GL_DYNAMIC_DRAW` VBO）。
- 提取共享 `DefaultTextures`（white/black），供 SpriteBatch/CompositePass 复用。
- `ShaderProgram::uniformLocation` 二选一：
  - 方案A（推荐）：加 `unordered_map` 缓存；
  - 方案B：明确注释“不缓存，由调用方缓存”。

### 阶段 3：P2 结构升级包（为 FND-010 铺路）
- 定义统一 `RenderPass` 接口（`clear/clean/reload/execute/resize`）。
- 把现有 Pass 逐步收敛到统一接口，降低新增 VFX Pass 的接入成本。
- 评估 Sprite 顶点颜色压缩（`vec4 float -> RGBA8 normalized`）并以 profile 数据决定是否落地。

## 需要新增的文件
- `src/engine/render/opengl/gl_state_scope.h`
- `src/engine/render/opengl/fbo_utils.h`
- `src/engine/render/opengl/fbo_utils.cpp`
- `src/engine/render/opengl/fullscreen_quad.h`
- `src/engine/render/opengl/fullscreen_quad.cpp`
- `src/engine/render/opengl/default_textures.h`
- `src/engine/render/opengl/default_textures.cpp`
- `src/engine/render/opengl/render_pass.h`（阶段3）
- `tests/engine/render/render_pass_interface_test.cpp`
- `tests/engine/render/gl_renderer_lifecycle_test.cpp`
- `tests/engine/render/opengl_pass_state_test.cpp`
- `tests/engine/render/bloom_precision_regression_test.cpp`

## 预计修改文件
- `src/engine/render/opengl/gl_renderer.h`
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/engine/render/opengl/sprite_batch.h`
- `src/engine/render/opengl/sprite_batch.cpp`
- `src/engine/render/opengl/bloom_pass.h`
- `src/engine/render/opengl/bloom_pass.cpp`
- `src/engine/render/opengl/composite_pass.h`
- `src/engine/render/opengl/composite_pass.cpp`
- `src/engine/render/opengl/lighting_pass.h`
- `src/engine/render/opengl/lighting_pass.cpp`
- `src/engine/render/opengl/emissive_pass.h`
- `src/engine/render/opengl/emissive_pass.cpp`
- `src/engine/render/opengl/scene_pass.h`
- `src/engine/render/opengl/scene_pass.cpp`
- `src/engine/render/opengl/shader_program.h`
- `src/engine/render/opengl/shader_program.cpp`
- `src/engine/render/opengl/gl_helper.h`
- `tests/CMakeLists.txt`

## 实现步骤
1. 修复 Bloom 精度问题（RGB16F），先消除可见视觉损失。  
2. 重写 SpriteBatch `flush` 容量检查流程，去除重复 vertex 上传。  
3. 收敛 `GLRenderer::clean()` 生命周期顺序，确保 context 最后释放。  
4. 补齐 headless 路径所有入口 no-op 防护。  
5. 修复 clear 后 FBO 绑定恢复一致性。  
6. 清理 CompositePass 重复 sampler uniform 设置。  
7. 引入 `ScopedBlendFunc` 并替换 Lighting/Emissive/Bloom 的手工恢复。  
8. 提取通用 FBO 创建工具，迁移 Scene/Lighting/Emissive。  
9. 提取共享 FullscreenQuad 与 DefaultTextures。  
10. 统一 `ShaderProgram::uniformLocation` 行为（实现缓存或修订注释）。  
11. （可选）引入统一 `RenderPass` 接口并逐步迁移各 Pass。  
12. （可选）评估并推进顶点格式压缩优化。  
13. 补齐回归测试与性能基线记录（DebugPanel + 测试）。

## 待办清单（用于追踪）
- [x] T1 Bloom ping-pong 纹理改为 HDR 格式（默认 `GL_RGB16F`）
- [x] T2 SpriteBatch `flush` 改为单次扩容 + 单次上传
- [x] T3 `GLRenderer` 资源释放顺序重排（context 最后）
- [x] T4 headless 入口防护补齐（含光照接口）
- [x] T5 Scene/Lighting/Emissive `clear()` 结束后恢复 FBO 绑定
- [x] T6 CompositePass 移除每帧固定 sampler 赋值
- [x] T7 引入 `ScopedBlendFunc` 并替换 3 个 pass 的手工恢复
- [x] T8 提取并接入 FBO 通用创建 helper
- [x] T9 提取并接入共享 FullscreenQuad（Bloom/Composite；LightingPass 保留动态 VBO）
- [x] T10 提取并接入共享 DefaultTextures
- [x] T11 `ShaderProgram::uniformLocation` 缓存策略落地（或注释修订）
- [x] T15 Claude 审阅收尾（helper 下沉 .cpp / blend guard 合并 / ScenePass::clear 解绑 / uniform warn / SpriteBatch 简化）
- [x] T12 （可选）统一 RenderPass 接口（`RenderPass`/`ReloadableRenderPass` 已接入 6 个 pass）
- [x] T13 （可选）顶点格式压缩评估与落地（`SpriteBatch::Vertex` 颜色改为 `RGBA8` 归一化，单顶点 32B -> 20B）
- [x] T14 新增生命周期/状态/Bloom 精度回归测试

## 验收标准
- Bloom 高亮区域不再因 `RGB8` 截断而变暗，视觉效果与 emissive 强度一致。
- SpriteBatch 单帧内不再出现冗余 vertex 重复上传。
- 所有 GL 对象释放发生在有效 context 生命周期内。
- headless 调用全渲染 API 稳定不崩溃。
- pass 间混合状态恢复不依赖隐式默认值。
- FBO/FullscreenQuad/DefaultTextures 重复样板显著减少，渲染模块更易扩展。

## 已确认决策
- Bloom 目标格式按 `GL_RGB16F` 实施（已确认）。

## 本轮验证记录
- `cmake --build build/debug -j8`（通过）
- `ctest --test-dir build/debug --output-on-failure -R "SpriteBatchSourceTest|RenderContextNoExceptionsTest|FactoryVisibilityTest"`（7/7 通过）
- `ctest --test-dir build/debug --output-on-failure -R "SpriteBatchSourceTest|RenderContextNoExceptionsTest"`（3/3 通过）
- `ctest --test-dir build/debug --output-on-failure -R "UIWorldAnchorTest|ScriptHostLifecycleTest|ParallelWaveSchedulerTest"`（17/17 通过）
- `ctest --test-dir build/debug --output-on-failure -R "GLRendererLifecycleTest|OpenGLPassStateTest|BloomPrecisionRegressionTest"`（5/5 通过）
- `ctest --test-dir build/debug --output-on-failure -R "RenderPassInterfaceTest|SpriteBatchSourceTest|GLRendererLifecycleTest|OpenGLPassStateTest|BloomPrecisionRegressionTest|RenderContextNoExceptionsTest|TextRendererNoExceptionsTest"`（11/11 通过）
