# 2026-06-02 Web Release Phase 3 实施记录

## 结果概览

- `GameApp::run()` 已拆成可复用生命周期：`init()`、`tickFrame()`、`shutdown()`。
- 主游戏入口改为 SDL3 main callbacks，由 `SDL_AppInit` / `SDL_AppEvent` / `SDL_AppIterate` / `SDL_AppQuit` 驱动。
- `InputManager` 新增单事件入口 `processSdlEvent()`，callbacks 模式下由 `SDL_AppEvent` 喂入事件，避免和 `SDL_PollEvent` 双重消费。
- 桌面 while-loop driver 仍通过 `GameApp::run()` 保留，供 tools / learn / 过渡调用使用。
- Phase 2 的 wasm walking skeleton 仍保持独立最小目标，后续 Phase 4 再接入完整渲染路径。

## 修改文件

- `src/main.cpp`
- `src/game/game_entry.h`
- `src/game/game_entry.cpp`
- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`
- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `plans/2026-06-02-web-release-wasm-migration-plan.md`

## 生命周期拆分

`GameApp` 现在公开以下入口：

- `init()`：初始化 SDL、窗口、渲染、资源、UI、场景和调度器。
- `tickFrame()`：执行单帧事件采样、时间推进、调度、UI 更新、渲染和 present。
- `shutdown()`：断开事件订阅、清理 UI/renderer/window 并调用 `SDL_Quit()`。
- `handleSdlEvent()`：callbacks driver 直接传入 SDL 事件。

`tickFrame()` 支持两种事件泵模式：

| 模式 | 用途 |
|---|---|
| `EventPumpMode::Poll` | 保持原桌面 while-loop 行为，由 `InputManager::sampleInputEvents()` 内部 poll |
| `EventPumpMode::ExternalCallbacks` | SDL3 callbacks 模式，不再主动 poll，只处理 `SDL_AppEvent` 已交付事件 |

## SDL3 callbacks 入口

`src/main.cpp` 启用 `SDL_MAIN_USE_CALLBACKS`：

- `SDL_AppInit` 创建 heap 生命周期的 `AppState` 和 `GameApp`。
- `SDL_AppEvent` 将 SDL 事件传给 `GameApp::handleSdlEvent()`。
- `SDL_AppIterate` 每次只执行一帧 `tickFrame(EventPumpMode::ExternalCallbacks)` 并立即返回。
- `SDL_AppQuit` 调用 `shutdown()` 后释放 `appstate`。

这样桌面端先吃到和 Web 端一致的 callback 生命周期，提前暴露主循环反转问题。

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

桌面入口 smoke：

- `build/debug/TinyFarmRPG-Darwin` 可启动。
- 运行 5 秒后仍处于主循环。
- 发送终止信号后正常退出，退出码为 `0`。

## 仍未覆盖

- 完整 engine/game 尚未作为 Web target 链接运行；当前 Web 目标仍是 Phase 2 walking skeleton。
- WebGL2 / GLES3 shader、RmlUi、音频和存档仍等待后续阶段接入。
- callbacks 模式已避免主动 poll，但仍假设 SDL 事件由主线程交付；若后续引入跨线程 `SDL_PushEvent`，需要单独复查事件边界。
