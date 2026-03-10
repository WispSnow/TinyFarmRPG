### Phase 10: RmlUi 采样策略切换（Pixel UI）

> **未完成项**：
> 1. 在可视运行环境中手动验证 `nearest` / `linear` 切换对图片与字体的实际观感
> 2. 确认 blur / box-shadow / backdrop-filter 等效果链路在运行时仍保持 linear 且无异常块状锯齿

**目标**：为 RmlUi 增加可切换的纹理采样策略，让像素风 UI 可以在 `nearest` / `linear` 之间切换；支持：

- 启动时从配置文件声明默认值
- 运行时在调试窗口中切换
- **作用范围仅限普通 RmlUi 图片纹理与字体字形 atlas**
- **滤镜、box-shadow、mask、post-process、SaveLayerAsTexture 等离屏/效果链路继续保持 linear**

> **背景结论**：
> - 当前仓库使用的 RmlUi GL3 backend 在 `GenerateTexture()` / 普通纹理创建路径中将过滤写死为 `GL_LINEAR`
> - 该路径同时覆盖普通图片与字体 atlas，因此 UI 图片和字形都会发糊
> - blur / backdrop-filter / post-process framebuffer / box-shadow 纹理并不应统一切到 nearest，否则视觉会明显变差

#### Step 10.1: 明确采样控制边界

先固定本阶段的控制边界，避免把“像素 UI 清晰化”误做成“整个 RmlUi 渲染器全部 nearest”：

- **纳入切换范围**
  - `LoadTexture()` 加载的普通 RmlUi 图片纹理
  - `GenerateTexture()` 生成的字体字形 atlas
  - 其他显式经 `RenderInterface::GenerateTexture()` 创建、用于常规内容采样的纹理
- **明确排除**
  - `SaveLayerAsTexture()` 保存的 layer 纹理
  - blur / backdrop-filter / mask / shader / box-shadow 等效果链路
  - backend 内部 post-process framebuffer、blit 与 filter pass

> 设计原则：本 phase 只控制“内容纹理采样”，不改变“效果纹理采样”。
>
> **边界论证**：
> - `RenderInterface_GL3::GenerateTexture()` 是虚函数，override 后会影响普通内容纹理与字体 atlas
> - `SaveLayerAsTexture()`、blur、box-shadow、post-process 等效果路径在 GL3 backend 内部直接使用静态 `Gfx::CreateTexture()` / framebuffer 纹理，不经过 `GenerateTexture()` 虚分派
> - 因此，只改写 `GenerateTexture()` 并不会误伤效果纹理，天然符合本 phase 的控制边界

#### Step 10.2: 在自定义 RmlUi backend 上补采样开关

**修改** `src/engine/ui/rmlui/render_interface_gl3_stb.h/cpp`

- 不直接修改 `external/RmlUi-6.2/Backends/RmlUi_Renderer_GL3.cpp`
- 在我们自己的 `RenderInterface_GL3_STB` 中新增共享枚举，例如：
  - `RmlUiTextureFilterMode::Nearest`
  - `RmlUiTextureFilterMode::Linear`
- 在 `RenderInterface_GL3_STB` 中：
  - override `GenerateTexture(...)`
  - override `ReleaseTexture(...)`
  - 新增 `setTextureFilterMode(...)` / `getTextureFilterMode()`
  - 跟踪所有由本接口创建且受策略控制的 texture handle
- **实现策略采用方案 B（推荐）**：
  - 先调用 `RenderInterface_GL3::GenerateTexture(...)`
  - 再对返回的 texture handle 执行 `glBindTexture + glTexParameteri`
  - 最后将 handle 记入跟踪集合
  - 不复制 vendor backend 的底层建纹理代码，避免未来升级时出现额外分叉
- `GenerateTexture(...)` 的具体行为：
  - `Nearest` -> 将 `GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_MAG_FILTER` 改为 `GL_NEAREST`
  - `Linear` -> 将 `GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_MAG_FILTER` 改为 `GL_LINEAR`
- **必须同步修正 `LoadTexture()`**
  - 当前 `LoadTexture()` 中使用了 `RenderInterface_GL3::GenerateTexture(...)` 限定名调用，会绕过虚分派
  - 改为 `GenerateTexture(...)` 或 `this->GenerateTexture(...)`
  - 这样 `LoadTexture()` 加载的普通图片纹理与字体 atlas 才会统一走 override 后的采样策略
- 当 mode 在运行时切换时：
  - 遍历已跟踪的 texture handle
  - 直接更新 `GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_MAG_FILTER`
  - **不要求重载文档，不要求重建字体 atlas**
- `ReleaseTexture(...)` 需先从跟踪集合中移除 handle，再转调基类 `ReleaseTexture(...)`

> 这样可以保证：已加载 UI 图片和字体在运行中的切换立即生效。

#### Step 10.3: 通过 RmlUILayer / GLRenderer 暴露运行时接口

**修改**：

- `src/engine/ui/rmlui/rml_ui_layer.h/cpp`
- `src/engine/render/opengl/gl_renderer.h/cpp`

新增一条从上到下的转发链：

- `GLRenderer::setRmlUiTextureFilterMode(...)`
- `GLRenderer::getRmlUiTextureFilterMode()`
- `RmlUILayer::setTextureFilterMode(...)`
- `RmlUILayer::getTextureFilterMode()`

并满足以下约束：

- `GLRenderer` 持有当前 RmlUi 采样模式的运行时状态
- `GLRenderer::setRmlUiTextureFilterMode(...)` 在 `rmlui_layer_ == nullptr` 时只更新缓存值，不报错
- `GLRenderer::setRmlUiTextureFilterMode(...)` 在 `rmlui_layer_` 已存在时立即把 mode 应用到 `RmlUILayer`
- `GLRenderer::initRmlUiLayer()` 创建完 layer 后，必须把当前缓存 mode 立即应用到 `RmlUILayer`
- headless / `rmlui_layer_ == nullptr` 时安全退化，不得崩溃
- 后续重新初始化 `RmlUILayer` 时，应继续沿用当前 mode，而不是悄悄回退到 linear

> 目的：让配置初始化、调试面板切换、未来脚本或 debug 命令都只依赖 `GLRenderer` 这一层。

#### Step 10.4: 补配置文件声明

**修改**：

- `config/window.json`
- `src/engine/core/config.h/cpp`

在 `graphics` 下新增显式配置项，例如：

```json
"graphics": {
    "vsync": true,
    "debug_ui": true,
    "rmlui_texture_filter": "nearest"
}
```

约束如下：

- 合法值仅允许：`"nearest"` / `"linear"`
- 默认推荐值：`"nearest"`
  - 当前项目以像素风为主，优先保证 UI 图片与字形清晰
- 非法值回退到 `nearest`，并输出 warning 日志
- `Config::toJson()` / `fromJson()` 必须双向支持该字段

> **本 phase 不要求调试面板改动自动回写配置文件**。  
> 配置文件负责“启动默认值”，调试面板负责“当前会话临时切换”。若后续需要“一键保存到配置”，单独开后续任务。

#### Step 10.5: 启动链接入配置默认值

**修改** `src/engine/core/game_app.cpp`

- 插入点固定为 `GameApp::initGLRenderer()`：
  - `GLRenderer::create(...)` 成功返回后
  - 由 `GameApp` 读取 `config_->rmlui_texture_filter_*`
  - 立即调用 `gl_renderer_->setRmlUiTextureFilterMode(...)`
- 不把 `Config` 依赖反向注入 `GLRenderer`
- `setVSyncEnabled()` / `setDebugUIEnabled()` 保持现有调用方式；RmlUi texture filter 与它们并列作为 renderer 初始化后的外部注入项
- 保证首次加载任意 RmlUi 文档前，默认采样策略已经生效

> 关键点：配置默认值必须早于首个场景的 RmlUi 文档加载，否则启动帧仍可能先以 linear 创建字体 atlas。

#### Step 10.6: 调试窗口支持运行时切换

**修改**：

- `src/engine/debug/panels/rmlui_debug_panel.h/cpp`

在 `RmlUi Debug` 面板中增加一个小节，例如 `Texture Filter`：

- `Nearest (Pixel Art)`
- `Linear (Smooth)`

行为要求：

- 切换后立即调用 `GLRenderer::setRmlUiTextureFilterMode(...)`
- UI 图片与字体 atlas 应立刻更新采样，不要求 reload 文档
- 文案中明确说明：
  - 该切换影响 RmlUi 图片与字体
  - 滤镜 / 离屏效果仍保持 linear
  - 当前调试切换仅影响本次运行，不自动保存到配置

#### Step 10.7: 测试与验证补齐

至少补齐以下验证：

- **配置测试**
  - `Config` 能正确解析 `"nearest"` / `"linear"`
  - `toJson()` 可正确回写该字段
  - 非法字符串能回退到默认值并保持行为稳定
- **源码/接口测试**
  - 验证 `GameApp::initGLRenderer()` 在首个场景加载前应用 RmlUi texture filter
  - 验证 `GLRenderer -> RmlUILayer -> RenderInterface_GL3_STB` 转发链存在
  - 验证 `RenderInterface_GL3_STB::LoadTexture()` 通过 `this->GenerateTexture(...)`（或等价非限定调用）走虚分派，而不是继续写死 `RenderInterface_GL3::GenerateTexture(...)`
  - 验证 `RenderInterface_GL3_STB::GenerateTexture()` 使用当前 mode 设置 `GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_MAG_FILTER`
  - 验证 `SaveLayerAsTexture()` / post-process / blur 路径未被本 phase 改成 nearest
- **手动验证**
  1. 用 `rmlui_tester` 或游戏运行时打开含位图字体与 icon 的文档
  2. 在 `nearest` 下确认像素边缘清晰、字形不发糊
  3. 切到 `linear` 后确认图片/文字恢复平滑
  4. 确认 blur / box-shadow / backdrop-filter 没有出现 nearest 带来的块状锯齿

#### Step 10.8: 文档与风险说明

- 在 Phase 10 完成后，回写 `plans/rmlui-migration/README.md`
- 若实现中发现某类 callback texture 也走 `GenerateTexture()` 且视觉上更适合 linear：
  - 不要临时回退成“全局 linear”
  - 优先为 `RenderInterface_GL3_STB` 增加受控分类或白名单，再单独记录到本 phase 文档

**主要风险**：

- 若简单改成“所有 RmlUi 纹理 nearest”，会破坏 blur / box-shadow / backdrop-filter 视觉质量
- 若只影响新建纹理、不回刷已加载 texture handle，则调试面板切换看起来“无效”
- 若配置应用时机晚于首帧文档加载，字体 atlas 可能已经按错误模式创建

**完成判定**：

1. `graphics.rmlui_texture_filter` 可声明启动默认值
2. `RmlUi Debug` 面板可在运行时切换 nearest / linear
3. 图片与字体都随切换立即生效
4. blur / box-shadow / backdrop-filter / layer texture 仍保持 linear
5. 构建与相关测试通过
