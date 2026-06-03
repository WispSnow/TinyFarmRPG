# 2026-06-02 Web Release 第二轮 Gameplay WASM 迁移计划

## 元信息

- 目标分支：`web-release`
- 计划状态：`Phase 13 runtime package mechanism smoke passed under Codex CLI; Phase 14 pending`
- 前置基线：第一轮 Phase 0-8 已完成，Web POC walking skeleton 可重复构建、预览和 gate。
- 第二轮目标：从 tile smoke / DOM shell 走向浏览器内可玩的真实 gameplay demo。
- 首版策略：单线程、完整 `engine/game` 最小子集、真实地图渲染与移动、RmlUi 基础 UI、IDBFS 存档闭环。

## 审阅采纳

Claude 的审阅意见整体合理，尤其是 Phase 10 / Phase 11 顺序问题。当前 `TitleScene` 加载 `ui/rmlui/scenes/title.rml`，并通过 `RmlUiRuntime` 和 `RmlDocumentController` 绑定 `start/load/menu/exit` 事件。因此在 RmlUi 恢复前要求“点击标题页开始游戏”会把真实渲染管线 bring-up 和 RmlUi bring-up 两个风险源耦合。

本计划采纳以下修改：

- Phase 10 改为绕过标题页，使用临时 dev boot 直接进入 `home_exterior`，只验证真实 `GlRenderer`、`MapManager`、输入、移动和相机。
- Phase 11 再恢复 RmlUi，并把标题页、hotbar、暂停菜单、存档 UI 放在同一 UI 恢复阶段。
- Phase 9 不再把 feature gates 当作待建能力，而是把它们作为已有基础，重点改成 Emscripten 下编译、链接、依赖隔离和 source audit。
- 不新增 `src/web/web_game_main.cpp`。`src/main.cpp` 已是桌面/Web 共用的 SDL3 callback 入口，第二轮应避免制造双入口漂移。
- Phase 13 增加运行时分包加载机制决策，不只定义资源包边界。
- Phase 14 明确 COOP / COEP 只 gate pthreads 变体，不 gate 单线程 demo。

## 实现思路

第二轮不再扩展独立 `src/web` walking skeleton，而是把真实游戏运行时逐步纳入 Web 构建。推进方式采用“可链接子集优先”的路线：先让 `engine` / `game` 静态库在 Emscripten 下可编译链接，再用现有 SDL3 callbacks 启动真实 `GameApp`，随后分阶段恢复地图、RmlUi、音频、存档、资源分包和发布验证。

当前已有基础：

- `src/main.cpp` 已使用 `SDL_MAIN_USE_CALLBACKS`、`SDL_AppInit`、`SDL_AppIterate`、`SDL_AppEvent`、`SDL_AppQuit`。
- `GameApp` 已拆出 `init()`、`tickFrame()`、`handleSdlEvent()`、`shutdown()`。
- callbacks 模式已通过 `EventPumpMode::ExternalCallbacks` 避免每帧再次 `SDL_PollEvent`。
- Web 单线程策略已存在，`SystemScheduler::parallelThreadPool()` 在无线程配置下返回 `nullptr`。
- `MapManager` 已具备同步 preload 路线，可用于单线程 Web。
- IDBFS smoke、WebGL2 shader 边界和 release gate 已建立。
- `web-poc-assets` manifest 已稳定，当前首屏 `.data` 为 280 个文件。

核心改造点：

- 取消 `TF_BUILD_WEB` 下顶层 CMake 早退，把 Web target 从独立 skeleton 切换为真实 app target。
- 让依赖查找区分 native / wasm，避免 Web cross-build 链到 `prebuilt/` native 库。
- 将桌面 OpenGL、RmlUi GL3 loader、FreeType/HarfBuzz、MiniAudio、Lua、spdlog 等依赖逐个收敛到 wasm 可编译配置。
- 用 feature flag 保护未恢复能力：Debug UI、RmlUi、Effekseer、高级后处理、pthreads、工具目标默认关闭。

```mermaid
flowchart TD
  A["Round 1 Web POC<br/>tile smoke / DOM shell"]
  A --> B["Phase 9<br/>真实 app wasm compile / link"]
  B --> C["Phase 10<br/>直进地图 bring-up"]
  C --> D["Phase 11<br/>RmlUi 与标题页"]
  D --> E["Phase 12<br/>音频与存档闭环"]
  E --> F["Phase 13<br/>资源分包机制"]
  F --> G["Phase 14<br/>跨浏览器发布候选"]
```

## 需要新增的文件

- `cmake/WebDependencies.cmake`
  - 管理 Emscripten 下 SDL3、Lua、RmlUi、FreeType/HarfBuzz、MiniAudio 等依赖策略。
- `cmake/WebRuntime.cmake`
  - 集中 Web link flags、异常策略、runtime feature flags、preload / package 设置。
- `src/engine/platform/web_audio_unlock.h`
- `src/engine/platform/web_audio_unlock.cpp`
  - 封装浏览器用户手势音频解锁状态，供 `AudioPlayer` 或启动 UI 使用。
- `src/engine/platform/web_filesystem_sync.h`
- `src/engine/platform/web_filesystem_sync.cpp`
  - 封装启动 sync、保存后 sync 和错误上报，避免 IDBFS 调用散落业务代码。
- `src/engine/render/opengl/web_gl_capabilities.h`
- `src/engine/render/opengl/web_gl_capabilities.cpp`
  - 将第一轮 DOM shell 的 WebGL feature probe 下沉到引擎层。
- `tools/web_release/package_web_assets.py`
  - 生成首屏包、地图包、音频包和压缩体积摘要。
- `tools/web_release/web_smoke.py`
  - 串联本地 server 与浏览器 smoke 的自动化入口。若 Browser 插件覆盖足够，可改为仅记录命令。
- `tests/engine/web_game_target_source_test.cpp`
  - 保护 Web target 不回退到 skeleton、不链接 pthreads、不启用桌面 GL / ImGui / SDL3_image 依赖。
- `plans/reports/2026-06-02-web-release-phase-9-report.md`
  - 后续每个 phase 延续报告记录。

明确不新增：

- 不新增 `src/web/web_game_main.cpp`。Web gameplay target 复用现有 `src/main.cpp` callback 入口。
- Phase 10 的直进地图 dev boot 优先通过 `game_entry.cpp` 中的编译期开关或小型 scene setup helper 完成；只有实现明显膨胀时再新增独立 helper 文件。

## Phase 9：真实 gameplay wasm target 编译与链接

目标：让 Web 构建编译并链接真实 `engine`、`game` 和现有 callback 入口。此阶段重点不是新增 feature gates，而是验证已有 gate 在 Emscripten 下足够干净，并消除 native-only 依赖。

实施步骤：

1. 调整顶层 CMake Web 路线。
   - 去掉 `TF_BUILD_WEB` 下 `add_subdirectory(src/web)` 后立即 `return()` 的结构。
   - 保留 `src/web` walking skeleton 为可选 smoke target，例如 `TF_BUILD_WEB_SKELETON=ON`。
   - 默认 Web target 改为真实 `${TARGET}`，链接 `game`。
2. 新增 Web 依赖模块。
   - `EMSCRIPTEN` 下不设置 native `CMAKE_PREFIX_PATH`。
   - SDL3 使用 Emscripten port 或已验证 wasm 来源。
   - 不链接 `SDL3_image::SDL3_image`，保留 stb_image 路径。
   - Lua、RmlUi、FreeType、HarfBuzz 逐个确认 wasm build source。
3. 固化 WebRuntime 设置。
   - 明确异常策略。首选 no-exception 构建；若第三方必须捕获异常，再单独评估 `-fwasm-exceptions`。
   - 集中 `-sMIN_WEBGL_VERSION=2`、`-sMAX_WEBGL_VERSION=2`、filesystem、memory、single-thread flags。
   - 单线程 gate 不允许 `-pthread`、`USE_PTHREADS`、`PTHREAD_POOL_SIZE`。
4. 审计 debug / desktop-only 源码。
   - 所有 ImGui include 和 `ImGui::` 调用必须只在 `ENABLE_DEBUG_UI` 源文件或宏保护内出现。
   - `TF_BUILD_WEB` 默认关闭 Debug UI、RmlUi debugger、Effekseer、tools、tests、learn。
   - `GL_FRAMEBUFFER_SRGB`、float framebuffer、Bloom、emissive pass 必须有 WebGL2 平台 gate。
5. 编译真实 app。
   - 先解决 compile errors，再解决 link errors。
   - 每个修复优先落在跨平台抽象或 feature gate，不新增长期 Web-only 业务分叉。
6. 保持第一轮 gate。
   - `validate_web_release.py` 增加真实 app target 检查。
   - 确认 `.html/.js/.wasm/.data` 仍可生成。

验收标准：

- `TF_BUILD_WEB=ON` 默认构建真实 gameplay wasm target。
- Web configure 不查找 native OpenGL、GLAD、SDL3_image prebuilt。
- wasm link 成功，产出 `.html/.js/.wasm/.data`。
- 单线程 build flags 干净，不含 pthreads 发布要求。
- native Debug 构建和完整 `ctest` 仍通过。

## Phase 10：直进地图与真实 GlRenderer bring-up

目标：在不依赖 RmlUi 的情况下，浏览器中直接进入 `home_exterior`，验证真实 `GlRenderer`、地图加载、输入、玩家移动和相机。这一阶段刻意绕开标题页和菜单。

实施步骤：

1. 增加 Web dev boot 模式。
   - 使用编译期开关或启动配置让 `game::createApp()` 注册直进地图 scene setup。
   - 直接创建 `GameScene` 并加载 `home_exterior`。
   - 保持 `src/main.cpp` 为唯一入口。
2. 临时关闭 RmlUi 依赖。
   - 增加或复用 `TF_WEB_ENABLE_RMLUI=OFF` 之类 feature gate。
   - `GameApp::initRmlUi()` 和依赖 `RmlUiRuntime` 的场景 UI 在此模式下走 no-op 或跳过。
   - 只验证引擎级地图、渲染、输入和移动。
3. 真实渲染管线 bring-up。
   - 运行完整 `GLRenderer` 的 scene / lighting / composite 基础路径。
   - WebGL2 下保护 `GL_FRAMEBUFFER_SRGB`，避免默认 framebuffer sRGB 在浏览器产生 GL error。
   - Bloom、emissive float pass 和 VFX pass 默认关闭或降级。
   - shader runtime rewrite 和 precision 注入必须覆盖真实 shader assets。
4. 恢复地图和移动。
   - `MapManager` 在单线程 Web 下走同步加载。
   - `home_exterior.tmj`、tileset、基础角色贴图必须来自 preload manifest。
   - SDL3 callbacks 下键盘、鼠标、gamepad 基础事件进入 `InputManager`。
5. 建立浏览器 smoke。
   - 断言 canvas 非空且帧持续变化。
   - 通过引擎日志、debug overlay 或测试 hook 断言 map id、玩家坐标或移动前后位置。
   - 不依赖 RmlUi 文本或标题页按钮。

验收标准：

- 浏览器中直接进入 `home_exterior`。
- 玩家可移动，画面无 WebGL error flood。
- 真实 `GLRenderer` 基础 pass 可在 WebGL2 下运行。
- 刷新后仍能重新进入 dev map boot。
- 失败时 release gate 输出明确的渲染、资源或输入诊断。

## Phase 11：RmlUi 基础 UI 与标题页恢复

目标：恢复真实 RmlUi runtime，并将 Phase 10 的 direct boot 切回标题页入口。标题页、hotbar、暂停菜单和存档选择在同一阶段恢复。

实施步骤：

1. 适配 RmlUi GL3 backend。
   - `RMLUI_GL3_CUSTOM_LOADER` 在 Web 下改用 `engine/platform/gl_platform.h` 或 GLES3 入口。
   - 避免包含 desktop-only `glad/glad.h`。
2. 适配字体路径。
   - FreeType / HarfBuzz 在 wasm 下可编译。
   - 默认字体和 fallback 字体来自 preload manifest。
3. 恢复标题页。
   - `TitleScene` 可加载 `ui/rmlui/scenes/title.rml`。
   - `start/load/menu/exit` data event 绑定可用。
   - 点击 `Start` 可进入现有新游戏流程。
4. 恢复游戏内基础 UI。
   - hotbar 可显示。
   - 暂停菜单可打开、关闭并回到游戏。
   - 存档选择 UI 可加载，保存功能可先延后到 Phase 12。
5. 缩小 DOM shell。
   - DOM shell 仅保留发布 debug overlay 或紧急 fallback。

验收标准：

- 浏览器中能从标题页点击进入游戏。
- 游戏中 hotbar 可显示。
- 暂停菜单可打开、关闭，并返回游戏。
- RmlUi 资源缺失会在 console / log 中给出文件路径。

## Phase 12：音频、存档与玩法闭环

目标：恢复可感知玩法闭环：用户点击后音频可播放，存档 UI 可保存，刷新后可加载。

实施步骤：

1. 浏览器音频解锁服务。
   - 用户手势后初始化或 resume 音频设备。
   - 确认 MiniAudio Emscripten Web Audio 后端在无 `-pthread` 下初始化成功。
   - `AudioPlayer` 初始化失败时不阻塞游戏主路径。
2. 恢复音效和 BGM。
   - 先恢复 `pop.mp3` 和标题/地图 BGM。
   - 音频加载错误必须可诊断。
3. 存档写入 persistent root。
   - 默认配置仍从 readonly package 读取。
   - 存档和用户设置写入 `/persistent`。
4. 保存后 sync。
   - 保存完成后显式 IDBFS sync。
   - 刷新页面后通过存档 UI 读取同一 slot。
5. 浏览器 smoke。
   - 新游戏进入地图。
   - 打开暂停菜单保存。
   - 刷新页面。
   - 加载 slot 并回到地图。

验收标准：

- 首次用户点击后音频状态为 Ready。
- MiniAudio Web Audio 后端在单线程构建下可用。
- 保存 slot 文件写入 persistent root。
- 刷新后存档 UI 能读取并加载 slot。
- 保存和 sync 失败有用户可见提示或 debug overlay 记录。

## Phase 13：资源分包与运行时加载机制

目标：把单个 20.8 MiB `.data` POC 包拆成可发布的分包路线，并先明确运行时加载机制。

实施步骤：

1. 技术决策。
   - 在独立 Emscripten data package 与自定义同步 XHR + C++ filesystem 写入之间做一次小型 spike。
   - 选择标准是加载时序可控、错误可诊断、资源路径仍能被 C++ filesystem 读取。
   - 包就绪必须有明确 gate，地图或音频加载前先等待对应 package ready。
2. 定义包边界。
   - `boot`: html/js/wasm、config、标题页、基础字体。
   - `shared-ui`: RmlUi theme、hotbar、pause、save UI。
   - `home-map`: home exterior/interior 地图、tileset、角色基础贴图。
   - `audio-core`: pop、标题 BGM、地图 BGM。
3. 生成包 manifest。
   - `package_web_assets.py` 从 `web-poc-preload.args` 和资源引用关系生成分包清单。
   - 输出原始大小、gzip、brotli 估算或实际文件。
4. 实现懒加载。
   - 首屏只加载 boot 包。
   - 进入地图前加载 `home-map`。
   - 音频可在用户点击后加载或延迟播放。
   - 可选将已下载包缓存到 IDBFS，减少二次进入下载等待。
5. 加载指标和 gate。
   - 记录首屏 Running 时间。
   - 记录标题页可交互时间。
   - 记录首次进入地图时间。
   - 校验 boot 包小于当前单包。

验收标准：

- boot 包显著小于当前 20.8 MiB 单包。
- 浏览器 smoke 能按需加载 `home-map`。
- release report 记录原始 / gzip / brotli 体积和加载耗时。
- 资源缺包时错误可定位到 package 和 asset path。

## Phase 14：跨浏览器发布候选

目标：形成可交付的 Web demo 发布候选，覆盖 Chromium 和 Safari。

实施步骤：

1. Chromium 自动 smoke。
   - 构建、gate、本地 server、浏览器自动流程串成单命令。
   - 覆盖标题页、地图、移动、菜单、保存、刷新加载。
2. Safari 手工 smoke。
   - 记录 Safari 版本、WebGL2 状态、音频策略、IndexedDB 行为。
   - 如 Safari 有限制，建立专门兼容任务。
3. 发布产物检查。
   - MIME、cache header、gzip/brotli。
   - COOP / COEP 只作为 pthreads 变体 gate，不作为单线程 demo gate。
   - 单线程和未来 pthreads 产物分开。
4. 禁用项记录。
   - Effekseer、Bloom、高级后处理、完整战斗、全地图资源是否恢复。
   - 每个禁用项给出恢复路线和风险。
5. 文档和交付说明。
   - 更新 plans report。
   - 增加 Web demo 运行说明。

验收标准：

- 从 clean checkout 执行固定命令生成 Web demo。
- Chromium 完整 smoke 通过。
- Safari 手工 smoke 通过或有明确阻塞记录。
- 单线程 demo 不依赖 COOP / COEP。
- 产物体积、加载耗时、禁用项和恢复路线全部记录。

执行记录（2026-06-02）：

- 已新增真实 gameplay Web 构建路径，`TF_BUILD_WEB=ON` 默认复用 `src/main.cpp` SDL3 callback 入口并链接 `engine` / `game`。
- 已新增 `cmake/WebDependencies.cmake` 与 `cmake/WebRuntime.cmake`，隔离 Web 依赖、link flags、no-exception 策略和 pthread gate。
- 已确认 Web 构建不链接 native OpenGL、GLAD、SDL3_image；Debug UI、Effekseer、runtime threads 在 Web 默认关闭。
- 已为 WebGL2 平台 gate 掉默认 framebuffer sRGB、桌面 `glClearDepth`、以及需要 float color framebuffer 的 HDR Bloom / Emissive 默认路径。
- 已生成 `build/web-gameplay-phase9/TinyFarmRPG-Web.html/.js/.wasm/.data`；尚未做浏览器内 direct map smoke，留到 Phase 10。

执行记录（2026-06-03，Phase 10）：

- 已新增可选 `TF_WEB_DIRECT_MAP_BOOT`，用于绕过 `TitleScene` 并由 `game_entry.cpp` 直接创建 `GameScene` 进入 `home_exterior`；`src/main.cpp` 仍是唯一 SDL3 callback 入口。
- 已在 direct boot 下关闭 `GameApp` 的 RmlUi runtime 初始化，并让 `GameScene` 跳过 HUD、inventory menu、pause menu 等 RmlUi 依赖路径。
- 已修复 `BlueprintManager` 在 `JSON_NOEXCEPTION` / Emscripten 下的 abort：蓝图解析改用 `engine::utils::json_helpers` 显式成员查找，不再依赖 `json.value(json_pointer, fallback)` 的异常回退。
- 已让 `MapLoadingSettings::forCurrentPlatform()` 在 Phase 10 的 `TF_WEB_DIRECT_MAP_BOOT` 下把 `preload_mode` 降为 `off`，只加载当前 home map，避免 POC manifest 尚未包含 `town.tmj` 时的全图预加载警告。
- 浏览器 smoke 已确认真实 `GameApp` 在 wasm 中启动、`home_exterior` 加载成功、真实 `GlRenderer` 画面非空，刷新后可重复进入地图；截图保存在 `build/web-gameplay-phase9/tinyfarm-web-phase10-preload-off.png`。
- 输入 smoke 已确认短按 `W/A/S/D` 不触发 fatal；由于当前 Browser 控制环境无法稳定模拟“按住”键盘事件，玩家位移仍需后续通过运行时坐标 hook 或专用浏览器自动化补充机器验收。
- 仍可见音频解码 / 缺失音效资源日志，按计划留到 Phase 12 的 MiniAudio Web Audio 与音频包恢复阶段处理。

执行记录（2026-06-03，Phase 11）：

- Web 默认入口已从 direct map boot 切回 `TitleScene`；`TF_WEB_DIRECT_MAP_BOOT` 保留为手动诊断选项，显式开启时仍跳过 RmlUi runtime。
- `GameApp` 在默认 Web 路径恢复 RmlUi runtime；RmlUi GL3 backend、FreeType、HarfBuzz 已在 `build/web-gameplay-phase11` 下完成 Emscripten 编译和链接。
- `web-poc-assets` / `web-poc-preload.args` 已加入 `appearance_customize`、`pause_menu`、`save_slot_select` 三组 RmlUi scene，首屏包从 274 个文件增至 280 个文件，`.data` 仍为 20.8 MiB。
- Web 当前单包阶段把 `MapLoadingSettings::forCurrentPlatform()` 的 `preload_mode` 统一降为 `off`，避免标题页进入游戏后尝试加载后置地图包资源。
- `tools/web_release/validate_web_release.py` 已兼容真实 gameplay target 的 preload stage 路径，并把 Phase 11 必需 UI 资源加入 release gate。
- 已生成 `build/web-gameplay-phase11/TinyFarmRPG-Web.html/.js/.wasm/.data`；release gate 通过，产物为 `.wasm` 7.1 MiB、`.data` 20.8 MiB。
- 本地 HTTP server 可正常提供 `.html/.js/.wasm/.data`，preload stage 中已确认存在 title、appearance、pause、save slot RmlUi 文件。
- Codex CLI 环境下已通过本地 `chrome-headless-shell` + CDP 完成浏览器交互 smoke。此前阻断来自桌面 App / MCP 浏览器自动化环境限制，不是 wasm 或游戏代码阻塞。
- 标题页 RmlUi 首屏渲染通过，截图：`build/web-gameplay-phase11/phase11-cdp-title.png`。
- 标题页 `Start` 点击通过，进入 `AppearanceCustomizeScene`，截图：`build/web-gameplay-phase11/phase11-cdp-after-start.png`、`build/web-gameplay-phase11/phase11-cdp-appearance-wide.png`。
- `AppearanceCustomizeScene` 的 `Confirm` 点击通过，进入 `home_exterior`，HUD 与 hotbar 可见，截图：`build/web-gameplay-phase11/phase11-cdp-after-confirm.png`。
- 暂停菜单打开和 `Resume` 返回游戏通过，截图：`build/web-gameplay-phase11/phase11-cdp-pause-menu.png`、`build/web-gameplay-phase11/phase11-cdp-after-resume.png`。
- 浏览器日志仍可见音频资源缺失、AudioContext 用户手势策略和少量 POC 子集资源警告，按计划留到 Phase 12 音频 / 存档闭环与 Phase 13 资源分包处理。

执行记录（2026-06-03，Phase 12）：

- `AudioPlayer` 已接入 WebAudio 用户手势解锁：Emscripten 下初始化 miniaudio 时使用 `noAutoStart`，首次键盘或鼠标事件后调用 `ma_engine_start()`，桌面路径保持初始化后 ready。
- `AudioPlayer` 在 WebAudio 初始化失败时不阻塞游戏主路径，进入静音模式；实际播放时会返回 false 并输出诊断。
- 已确认 MiniAudio Web Audio 后端在当前单线程、无 `-pthread` 的 Web 构建下可初始化，并能在用户手势后启动。
- `GameApp` 启动时执行 IDBFS initial sync，把浏览器持久化内容同步到 `/persistent`。
- `SaveService` 的直接保存与无线程 async 保存路径已在写入成功后执行 `syncPersistentStorageToBrowser()`；async 保存完成事件等待 sync 回调后再发布，避免 UI 提前显示成功但刷新丢档。
- `ResourceManager` 在 Web 下对未进入当前 preload 包的音频只注册资源路径、不尝试解码，旧的启动期 `AudioLoader: 无法解码音频文件` 噪音已降级为可定位的缺包 warning。
- `validate_web_release.py` 已把 Phase 12 最小音频闭环资源纳入 preload gate：`assets/audio/pop.mp3`、`assets/audio/01_spring_journey.ogg`、`assets/audio/02_spring_fairy_tale.ogg`、`config/audio.json`。
- Web 构建与 release gate 通过：`build/web-gameplay-phase11/TinyFarmRPG-Web.html/.js/.wasm/.data`，当前 `.wasm` 7.1 MiB、`.data` 20.8 MiB。
- Codex CLI 下 Playwright 可正常控制本地 wasm 页面；已完成完整 Phase 12 浏览器 smoke：标题页 `Start`、appearance `Confirm`、进入 `home_exterior`、暂停菜单保存 slot0、刷新页面、标题页 `Load`、加载 slot0 回到地图。
- 浏览器 smoke 结果：slot0 在刷新前、刷新后、加载后均存在，大小 11355 bytes；日志中 `AudioContext was not allowed to start` 为 0、旧音频解码失败为 0、音频设备启动 2 次、IDBFS mounted 2 次、`home_exterior` 加载 3 次、存档加载 1 次。
- 关键截图保存在 `build/web-gameplay-phase11/phase12-playwright-title.png`、`phase12-playwright-after-save.png`、`phase12-playwright-load-slots.png`、`phase12-playwright-after-load.png`。
- 仍可见未进入当前 preload 包的完整音频资源 warning，这是 Phase 13 `audio-core` / 资源分包需要收敛的范围，不再阻塞 Phase 12 gameplay 闭环。

## Phase 13 执行记录

- 已选择运行时分包机制：自定义 `.tfpack` 包 + 浏览器同步 XHR 读取 + C++ filesystem 写入 MEMFS。选择原因是 C++ 地图/资源读取路径仍保持同步，package ready gate 可以在 `GameScene` / `MapManager` 进入真实资源读取前完成。
- spike 中验证过 Emscripten Fetch 的同步模式在当前主线程路径下返回空句柄；同步 XHR 不能使用 `responseType=arraybuffer`，因此 loader 使用 `overrideMimeType("text/plain; charset=x-user-defined")` 读取二进制字符串并复制进 wasm heap。
- `tools/web_release/package_web_assets.py` 已从 `manifests/assets/web-poc-preload.args` 生成 `boot`、`shared-ui`、`home-map`、`audio-core` 分包计划和 `.tfpack` artifact。
- 当前体积：`boot` 28 files / 2.8 MiB；`shared-ui` 166 files / 13.3 MiB，gzip 8.0 MiB；`home-map` 81 files / 687.1 KiB，gzip 279.6 KiB；`audio-core` 5 files / 4.1 MiB，gzip 4.0 MiB。
- `cmake/WebRuntime.cmake` 已在 Web target post-build 生成 package index 和 `.tfpack`，`validate_web_release.py` 已校验 package strategy、包边界、artifact 存在性、路径覆盖和 boot 小于当前 20.8 MiB 单包。
- `GameScene` 进入真实 gameplay 前加载 `home-map.tfpack`，`MapManager::loadMap` 也有地图级兜底 gate，资源缺包时日志能定位到 package 和 map。
- 浏览器 smoke 已通过：`web-packages/home-map.tfpack` 返回 200，日志显示 `WebAssetPackage: package 'home-map' loaded (81 files).`，随后 `assets/maps/home_exterior.tmj` 加载成功并进入地图。
- 关键截图保存在 `build/web-gameplay-phase11/phase13-runtime-package-smoke.png`。
- 当前仍保留完整 `.data` 预载包；Phase 13 先验证运行时加载机制和分包 gate，不把 boot-only `.data` cutover 混入同一风险面。后续发布收敛应增加 boot-only 构建选项，并把 `shared-ui` / `audio-core` 的加载时序也接入真实 gate。
- 同步 XHR 会阻塞主线程，适合作为当前同步引擎路径下的 Phase 13 bridge；若发布版要展示 loading UI 或加载大包，应改为异步 package loader + scene transition/loading screen。

## 第二轮待办清单

- [x] 新增 `cmake/WebDependencies.cmake`，隔离 wasm 依赖来源。
- [x] 新增 `cmake/WebRuntime.cmake`，集中 Web link flags、异常策略和 runtime feature gates。
- [x] 保留 walking skeleton 为可选 smoke target，默认 Web target 改为真实 gameplay wasm。
- [x] 不新增 Web gameplay main，复用 `src/main.cpp` SDL3 callbacks 入口。
- [x] 让 `engine` 在 `EMSCRIPTEN` 下编译，不依赖 native OpenGL / GLAD / SDL3_image。
- [x] 让 `game` 在 `EMSCRIPTEN` 下编译，禁用 Debug UI / Effekseer / pthreads。
- [x] 审计 ImGui include 和调用，确认全部在 Debug UI gate 后。
- [x] 审计 `GL_FRAMEBUFFER_SRGB`、float framebuffer、Bloom、emissive pass 的 WebGL2 gate。
- [x] 浏览器启动真实 `GameApp`，通过 direct map boot 进入 `home_exterior`。
- [ ] 键盘输入和玩家移动在浏览器中可用。当前已完成短按 smoke 且无 fatal，仍缺按住移动的坐标级自动验收。
- [x] 真实 `GlRenderer` 基础 pass 在 WebGL2 下运行且无 GL error flood。
- [x] RmlUi runtime、标题页、hotbar、暂停菜单和存档 UI 资源完成 Web 编译、链接与 preload gate。
- [x] 标题页按钮可点击并进入真实游戏流程。
- [x] 浏览器 UI smoke 已确认标题页、appearance、hotbar、暂停菜单打开与 `Resume` 返回游戏。
- [x] 音频用户手势解锁接入真实 `AudioPlayer` 路径。
- [x] 确认 MiniAudio Web Audio 后端在无 `-pthread` 下可初始化。
- [x] 保存 slot 写入 `/persistent` 并在刷新后读取。
- [x] Phase 12 浏览器 smoke 覆盖标题页、新游戏、菜单保存、刷新、加载 slot 并回到地图。
- [x] 决定运行时资源包加载机制。
- [x] 拆分 boot / shared-ui / home-map / audio-core 资源包。
- [x] release gate 覆盖真实 gameplay target 和分包 manifest。
- [x] 浏览器 smoke 验证 `home-map` 运行时加载并进入 `home_exterior`。
- [ ] Chromium 自动 smoke 覆盖标题页、地图、移动、菜单、保存、刷新加载。
- [ ] Safari 手工 smoke 记录并形成发布候选报告。

## 风险与应对

| 风险 | 影响 | 应对 |
|------|------|------|
| RmlUi / FreeType / HarfBuzz wasm 编译阻塞 | 标题页和菜单无法恢复 | Phase 10 先 direct map boot，不把 UI 和渲染风险耦合 |
| 真实 GlRenderer 首次跑 WebGL2 | 地图渲染异常或黑屏 | Phase 10 单独 bring-up，先关 Bloom / emissive / VFX |
| `GL_FRAMEBUFFER_SRGB` 或 float FBO 在 WebGL2 报错 | console error flood 或黑屏 | 平台 gate 加 source test，浏览器 smoke 查 console |
| native prebuilt 被 Web 构建误用 | configure 或 link 失败 | `WebDependencies.cmake` 明确隔离依赖查找路径 |
| ImGui 源文件泄漏到 Web target | compile 或 link 失败 | Phase 9 做 source audit，Debug UI 默认关闭 |
| MiniAudio 浏览器策略复杂 | 音频初始化失败 | 用户手势解锁服务先行，验证无 pthreads Web Audio 后端 |
| 单包资源启动慢 | 首屏等待过长 | Phase 13 先决定 runtime package loader，再拆 boot / map 包 |
| IDBFS sync 时序不稳定 | 刷新后存档丢失 | 保存后显式 sync，并在 smoke 中验证刷新加载 |
| 真实 app target 改动面大 | native 回归风险 | 每个 phase 必跑 native build 和完整 ctest |

## 当前无待澄清问题

计划按“真实 gameplay wasm 最小可玩路径”推进。Phase 13 已完成运行时资源分包机制、package manifest / artifact gate 和 `home-map` 浏览器加载 smoke。下一步进入 Phase 14，应优先把 Chromium smoke 串成固定命令，并补齐 Safari 手工 smoke、boot-only 构建选项、移动坐标级验收和发布候选报告。
