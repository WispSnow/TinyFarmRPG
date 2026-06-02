# 2026-06-02 Web Release Phase 2 实施记录

## 结果概览

- 新增 `TF_BUILD_WEB=ON` WebAssembly 构建入口。
- Web 配置使用 Emscripten toolchain，绕开桌面 `OpenGL::GL`、GLAD、`SDL3_image`、engine/game/tools/learn/tests。
- 新增 SDL3 + WebGL2 walking skeleton：创建 canvas、非阻塞 browser main loop、清屏并绘制一个基础 quad。
- 使用 Phase 1 的 `manifests/assets/web-poc-preload.args` 作为唯一 Web POC 资源清单，不全量打包 `assets`。
- 生成浏览器可加载的 `.html/.js/.wasm/.data` 四件套。

## 新增/修改文件

- `CMakeLists.txt`
- `cmake/WebPreload.cmake`
- `cmake/scripts/StageWebPreload.cmake`
- `src/engine/platform/gl_platform.h`
- `src/web/CMakeLists.txt`
- `src/web/web_main.cpp`

## Web 构建命令

```bash
source "$HOME/.local/emsdk/emsdk_env.sh"
emcmake cmake -S . -B build/web-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTF_BUILD_WEB=ON \
  -DBUILD_TOOLS=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_LEARN=OFF
cmake --build build/web-release
```

## 资源打包策略

`cmake/WebPreload.cmake` 读取 `manifests/assets/web-poc-preload.args`，构建时先把精选资源复制到：

```text
build/web-release/src/web/TinyFarmRPG-Web-preload-root
```

随后只传一个 Emscripten preload 参数：

```text
--preload-file=<preload-root>@/
```

这样避免 274 个 `--preload-file` 参数在 CMake / Ninja / Emscripten response file 链路中被错误转义，也保留了原始虚拟文件路径。

## 构建产物

| 文件 | 大小 |
|---|---:|
| `build/web-release/TinyFarmRPG-Web.html` | 19 KiB |
| `build/web-release/TinyFarmRPG-Web.js` | 239 KiB |
| `build/web-release/TinyFarmRPG-Web.wasm` | 697 KiB |
| `build/web-release/TinyFarmRPG-Web.data` | 21 MiB |

## 浏览器验证

本地静态服务器：

```bash
cd build/web-release
python3 -m http.server 8787
```

验证页面：

```text
http://127.0.0.1:8787/TinyFarmRPG-Web.html
```

已确认：

- 页面标题为 `TinyFarmRPG Web Walking Skeleton`。
- canvas 尺寸为 960x540。
- WebGL2 上下文可用，输出区显示 `OpenGL ES 3.0 (WebGL 2.0 ...)`。
- 画面非空，中央基础 quad 成功绘制。
- 资源 preload smoke 通过：
  - `/assets/data/resource_mapping.json`
  - `/config/window.json`
  - `/ui/rmlui/scenes/title.rml`
  - `/assets/maps/home_exterior.tmj`

## 回归验证

已通过：

- `cmake --build build/debug -- -j$(sysctl -n hw.ncpu)`
- `ctest --test-dir build/debug --output-on-failure -j$(sysctl -n hw.ncpu)`

CTest 结果：

- 1052 个测试全部通过。
- 11 个测试由测试套件跳过，主要是文档/集成类 UI 检查。

## 仍未覆盖

- 还没有接入完整 engine/game 主循环。
- 还没有迁移桌面 OpenGL shader 到 GLES3/WebGL2 变体。
- 还没有接入 RmlUi、音频、存档或 IDBFS。
- SDL3 Emscripten port 仍提示 experimental；当前作为 Phase 2 风险记录保留。
