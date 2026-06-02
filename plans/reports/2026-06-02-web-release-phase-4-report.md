# 2026-06-02 Web Release Phase 4 实施记录

## 结果概览

- 新增/扩展 `engine/platform/gl_platform.h`，native 继续使用 GLAD + OpenGL 3.3，Emscripten 使用 GLES3/WebGL2 头文件。
- 将 engine 内直接引用 `<glad/glad.h>` 的项目源文件收口到平台头，降低后续完整 engine wasm 编译风险。
- `RenderContext` 在 Web 编译下默认请求 GLES 3.0 / WebGL2，并跳过桌面专属的 context flags 与 default framebuffer sRGB 属性。
- `ShaderProgram` 增加 Web shader 源码适配：`#version 330 core` 可转换为 `#version 300 es`，并补充 precision 声明。
- Web walking skeleton 从彩色 quad 升级为 textured tile smoke：读取 `home_exterior.tmj`、外部 `.tsj` tileset 和 atlas PNG，使用 WebGL2 绘制真实地图 tiles。
- Web POC 继续关闭 default framebuffer sRGB、Bloom 和高级 VFX，避免在 WebGL2 首次落地时引入 float render target 与 gamma 变量。

## 修改文件

- `src/engine/platform/gl_platform.h`
- `src/engine/render/opengl/renderer_init_params.h`
- `src/engine/render/opengl/render_context.cpp`
- `src/engine/render/opengl/shader_program.cpp`
- `src/engine/render/opengl/shader_program.h`
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/engine/resource/texture_loader.cpp`
- `src/engine/render/opengl/*` 中直接 GLAD include 的平台头替换
- `src/engine/resource/*` / `src/engine/ui/rmlui/*` / `src/engine/utils/defs.h` 中直接 GLAD include 的平台头替换
- `src/web/CMakeLists.txt`
- `src/web/web_main.cpp`
- `plans/2026-06-02-web-release-wasm-migration-plan.md`

## WebGL2 Tile Smoke

`src/web/web_main.cpp` 现在会：

- 创建 SDL3 + GLES 3.0 context。
- 编译 `#version 300 es` tile shader。
- 读取 preload 中的 `/assets/maps/home_exterior.tmj`。
- 解析 tile layers、external tileset firstgid / source、atlas image metadata。
- 使用 stb_image 解码 atlas PNG 并上传为 `GL_RGBA8` 纹理。
- 按 layer 顺序生成 tile quads，并按 texture 分批绘制。

浏览器日志：

```text
TinyFarmRPG WebGL vendor: WebKit
TinyFarmRPG WebGL renderer: WebKit WebGL
TinyFarmRPG GL version: OpenGL ES 3.0 (WebGL 2.0 ...)
tile smoke: layers=7 tilesets=11 batches=28 vertices=6966
```

截图验证产物：

```text
build/web-release/webgl-tile-smoke-converted.png
```

像素 smoke：

```text
size=(1280, 720)
sampled_unique_colors=11818
central_non_background_samples=56953 / 76006
```

## 构建产物

| 文件 | 大小 |
|---|---:|
| `build/web-release/TinyFarmRPG-Web.html` | 20 KiB |
| `build/web-release/TinyFarmRPG-Web.js` | 244 KiB |
| `build/web-release/TinyFarmRPG-Web.wasm` | 832 KiB |
| `build/web-release/TinyFarmRPG-Web.data` | 21 MiB |

## 验证

已通过：

```bash
cmake --build build/debug -- -j$(sysctl -n hw.ncpu)
ctest --test-dir build/debug --output-on-failure -j$(sysctl -n hw.ncpu)
source "$HOME/.local/emsdk/emsdk_env.sh"
emcmake cmake -S . -B build/web-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DTF_BUILD_WEB=ON -DBUILD_TOOLS=OFF -DBUILD_TESTING=OFF -DBUILD_LEARN=OFF
cmake --build build/web-release
```

CTest 结果：

- 1052 个测试全部通过。
- 11 个测试由测试套件跳过。

浏览器验证：

- `http://127.0.0.1:8787/TinyFarmRPG-Web.html`
- canvas 尺寸为 960x540。
- WebGL2 context 可用。
- 画面非空，显示 `home_exterior` 地图 tile。

## WebGL2 风险记录

- Web POC 暂用 `GL_RGBA8` atlas 纹理，避免 sRGB decode / default framebuffer sRGB 的组合差异影响首个 tile smoke。
- Native `TextureLoader` 仍在非 Web 平台使用 `GL_SRGB8_ALPHA8`；Web 路径通过 `gl_platform.h` 降级为 `GL_RGBA8`。
- `GL_FRAMEBUFFER_SRGB` 已 feature-gate；WebGL2 POC 不启用。后续需要在 sRGB texture、canvas/default framebuffer 和 shader 手动 gamma 之间做正式决策。
- Bloom 的 `GL_RGB16F` 与 Emissive 的 `GL_RGBA16F` 暂不进入 Web POC；完整恢复要先确认 WebGL2 float color buffer 扩展行为。
- 当前 tile smoke 只支持 atlas-backed external tilesets；Tiled image collection tileset 会被跳过，完整 loader 接入后再统一处理。

## 仍未覆盖

- 完整 engine/game 渲染管线尚未作为 Web target 链接运行。
- RmlUi、音频、存档、IDBFS 和完整 gameplay loop 仍等待后续阶段。
- Web tile smoke 使用窄 Tiled 解析器，只用于 Phase 4 验证真实 atlas / UV / blend / VAO / VBO 路径，不替代 engine loader。
