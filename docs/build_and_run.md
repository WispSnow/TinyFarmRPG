# 构建与运行

> 用途：第一次拿到代码的学生需要的"如何把项目跑起来"。CMake / Ninja / 预设 / 依赖 / 资源复制 / 工具与测试，集中说明。
>
> 调试细节（ASan / TSan / LLDB / 给 AI 报告模板）见 [tutorial/debugging.md](tutorial/debugging.md)，本篇是更基础的入口。

## 一、快速上手（3 步）

```bash
# 1. 配置（首次或修改 CMakeLists 后）
cmake --preset debug

# 2. 构建
cmake --build --preset debug

# 3. 运行
./build/debug/TinyFarmRPG-Darwin     # macOS
./build/debug/TinyFarmRPG-Linux      # Linux
build\debug\TinyFarmRPG-Windows.exe  # Windows
```

可执行文件名形如 `TinyFarmRPG-<SystemName>`，由 `CMakeLists.txt:16` 的 `set(TARGET ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME})` 决定。

工作目录约定：直接从 `build/<preset>/` 启动，所有资源（`assets/` / `ui/` / `scripts/` / `config/`）已经在构建时被拷过去，开箱即用。

## 二、系统依赖

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| CMake | 3.21+ | `CMakePresets.json` 用 v6 schema，要求 3.21 |
| Ninja | 任意现代版本 | 推荐（构建最快）。未安装时用 `debug-fallback` / `release-fallback` 预设改走系统默认生成器 |
| C++ 编译器 | C++20 | macOS: AppleClang（Xcode 15+）；Linux: **GCC 13+** / Clang 17+（源码使用 `std::format`，libstdc++ 需 13 代）；Windows: MSVC（Visual Studio 2022），MinGW 未验证 |
| Python | 3 | 部分依赖构建脚本需要 |
| Git | 任意 | 拉取项目和子模块 |

依赖库本身（SDL3 / RmlUi / Lua / Effekseer 等）由 CMake 在配置阶段自行拉取或链接，多数情况下**不需要手动安装**。

> **首次 configure 需要联网**：`external/` 只内置了一部分依赖源码；SDL3 / SDL3_image / glm / nlohmann-json / spdlog 这五个依赖在本机没有安装时，会由 FetchContent 在配置阶段从 GitHub 在线克隆。离线环境可预先把对应源码放进 `external/`（目录命名见 `cmake/Dependencies.cmake` 中各依赖的 `LOCAL_PATH`）。

### Linux 额外系统包

主流发行版暂无 SDL3 官方包，SDL3 会从源码构建。SDL 的 CMake **只启用配置时能找到的后端**：缺少 X11 / Wayland 开发头时，SDL 会"成功"编译出一个没有视频后端的库，运行时才报 `No available video device`。因此 configure 之前先装齐（Ubuntu / Debian 示例）：

```bash
sudo apt install build-essential ninja-build cmake \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev \
    libwayland-dev libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev \
    libasound2-dev libpulse-dev
```

音频在运行时由 miniaudio 通过 dlopen 使用系统 ALSA / PulseAudio，无需额外构建依赖。无显示环境（SSH / CI）下测试会自动 `GTEST_SKIP`，需要真跑时用 xvfb 或 `SDL_VIDEODRIVER=dummy`。

### Windows 说明

- 仅支持 MSVC（Visual Studio 2022）工具链。预设使用 Ninja 生成器，需在 **x64 Native Tools Command Prompt**（或 VS 内置终端）里执行 preset 命令，让 `cl.exe` 在 PATH 上。
- 主游戏可执行是 GUI 子系统（不弹控制台，spdlog 输出不可见）；`tools/` 与 `tests/` 下的可执行保持控制台子系统，方便查看输出。
- 依赖的 DLL（如 harfbuzz）构建后会自动复制到 exe 旁（`setup_windows_dll_copy`）。

```mermaid
flowchart LR
    USER["开发者机器"] --> CMAKE["cmake --preset debug"]
    CMAKE --> CFG["读 CMakePresets.json<br/>读 CMakeLists.txt"]
    CFG --> DEPS["cmake/*.cmake<br/>拉取/链接依赖"]
    DEPS --> NINJA["生成 build/debug/build.ninja"]
    NINJA --> BUILD["cmake --build --preset debug"]
    BUILD --> EXE["build/debug/TinyFarmRPG-...<br/>+ 拷贝 assets / ui / scripts / config"]
```

## 三、CMake 预设

由 `CMakePresets.json` 提供，全部使用 Ninja，构建目录统一在 `build/<presetName>/`。

| 预设 | `CMAKE_BUILD_TYPE` | 额外开关 | 用途 |
|------|--------------------|----------|------|
| `debug` | Debug | — | 日常开发调试 |
| `debug-asan` | Debug | `-fsanitize=address`（macOS / Linux） | 怀疑内存错误（越界 / use-after-free） |
| `debug-tsan` | Debug | `ENABLE_TSAN=ON`（macOS / Linux） | 怀疑多线程竞争 |
| `release` | Release | — | 性能测试 |
| `relwithdebinfo` | RelWithDebInfo | — | 生产构建的崩溃定位 |
| `debug-fallback` | Debug | 不指定生成器 | **没装 Ninja 时的备用**：Windows 走 Visual Studio，Linux/macOS 走 Unix Makefiles |
| `release-fallback` | Release | 不指定生成器 | 同上的 Release 版 |

> `debug-asan` 和 `debug-tsan` 在 Windows 上不可用（`condition` 跳过）。Windows 用 `debug` 配合 Visual Studio 调试器。

**备用预设（fallback）说明**：`*-fallback` 预设不指定生成器，由 CMake 按平台选择默认生成器。在 Windows 上是 Visual Studio（多配置生成器），产物会多一层配置子目录：可执行文件在 `build/debug-fallback/Debug/TinyFarmRPG-Windows.exe`，资源也随之复制到该目录，直接从那里运行即可；也可以用生成的 `.sln` 在 VS 里打开调试。在 Linux/macOS 上是 Unix Makefiles，目录结构与 Ninja 预设相同。构建命令一致：

```bash
cmake --preset debug-fallback
cmake --build --preset debug-fallback     # 默认并行 8，可用 -j N 覆盖
ctest --preset debug-fallback             # 跑测试
```

每个 preset 对应一个独立 `build/` 目录，预设之间互不干扰。**第一次切换预设后**也无需 clean，直接 `cmake --preset <name>` + `cmake --build --preset <name>` 即可。

## 四、CMake 选项一览

`CMakeLists.txt` 顶部定义的可控选项（默认值在括号内）：

| 选项 | 默认 | 含义 |
|------|------|------|
| `BUILD_SHARED_LIBS` | OFF | 依赖库默认静态链接。改成 ON 会让大部分库变成动态库 |
| `ENABLE_DEBUG_UI` | ON | ImGui 调试面板（定义 `TF_ENABLE_DEBUG_UI` 宏） |
| `ENABLE_RMLUI_DEBUGGER` | ON | RmlUi 内置 inspector |
| `ENABLE_TSAN` | OFF | ThreadSanitizer（`debug-tsan` 预设自动打开） |
| `BUILD_TOOLS` | ON | 编译 `tools/` 下的调试工具（需要 `ENABLE_DEBUG_UI=ON`） |
| `BUILD_RMLUI_TESTER` | ON | 单独控制 `rmlui_tester` |
| `BUILD_LEARN` | ON | 编译 `learn/` 下的实验目标（与课程子教程对应） |
| `BUILD_TESTING` | ON | 编译 `tests/` 下的 GoogleTest |

显式覆盖示例：

```bash
cmake --preset debug -DENABLE_DEBUG_UI=OFF -DBUILD_LEARN=OFF
```

另外，顶层 `CMakeLists.txt` 会为 EnTT 全局定义两项构建前提：`ENTT_ID_TYPE=std::uint64_t` 把 ID 类型强制设为 64 位，降低 catalog / cue / resource 哈希冲突风险；`ENTT_USE_ATOMIC` 让 EnTT 在多线程访问时使用 atomic 内部共享状态，配合 SystemScheduler 的并行岛使用。

## 五、构建产物结构

```
build/debug/
├── TinyFarmRPG-<System>              # 可执行文件（即 ${TARGET}）
├── assets/                           # 由 BuildHelpers.setup_asset_copy 复制
├── ui/                               # 由 setup_ui_copy 复制
├── scripts/                          # 由 setup_script_copy 复制
├── config/                           # 由 setup_config_copy 复制
├── tools/                            # 调试工具（若 BUILD_TOOLS=ON）
├── tests/                            # GoogleTest 可执行文件（若 BUILD_TESTING=ON）
└── learn/                            # 学习实验目标（若 BUILD_LEARN=ON）
```

资源 / UI / 脚本 / 配置都是**构建后复制**到 build 目录，因此修改 `assets/data/*.json` 或 `scripts/*.lua` 等内容文件**通常**需要重新 `cmake --build` 才能在已编译的二进制里看到（看 `cmake/BuildHelpers.cmake` 中的 dependency 规则；常见做法是再跑一次构建命令，Ninja 会增量复制）。

Windows 上还有 `setup_windows_dll_copy`（`cmake/BuildHelpers.cmake`）负责把动态库 DLL 复制到可执行文件旁。

## 六、CMake 模块布局

`CMakeLists.txt` 把各部分拆到 `cmake/*.cmake`：

```mermaid
flowchart TD
    ROOT["CMakeLists.txt"] --> CS["cmake/CompilerSettings.cmake<br/>C++20 / 警告 / sanitizer 接入"]
    ROOT --> RP["cmake/RuntimePath.cmake<br/>RPATH 配置"]
    ROOT --> D1["cmake/Dependencies.cmake<br/>SDL3 / EnTT / spdlog / json ..."]
    ROOT --> D2["cmake/ScriptingDependencies.cmake<br/>Lua / Sol2"]
    ROOT --> D3["cmake/EffekseerDependencies.cmake<br/>VFX 后端"]
    ROOT --> D4["cmake/RmlUiDependencies.cmake<br/>生产 UI"]
    ROOT --> IM["cmake/ImGui.cmake<br/>调试 UI"]
    ROOT --> GL["cmake/OpenGL.cmake"]
    ROOT --> BH["cmake/BuildHelpers.cmake<br/>资源复制 / DLL 复制"]
    ROOT --> PI["cmake/ProjectInfo.cmake<br/>构建末尾打印汇总"]
```

工程目标三层（`CMakeLists.txt:80-153`）：

- `engine` 静态库：通用引擎层
- `game` 静态库：依赖 engine，包含 TinyFarmRPG 玩法
- `${TARGET}` 可执行文件：依赖 game，加 `src/main.cpp` 入口

子目录：

- `src/`：游戏源码（`add_subdirectory(src)`）
- `tools/`：调试工具（如 `BUILD_TOOLS && ENABLE_DEBUG_UI`）
- `tests/`：GoogleTest（如 `BUILD_TESTING`）
- `learn/`：实验目标（如 `BUILD_LEARN`）

## 七、运行可执行文件

构建完成后直接运行：

```bash
./build/debug/TinyFarmRPG-Darwin       # macOS
./build/debug/TinyFarmRPG-Linux        # Linux
.\build\debug\TinyFarmRPG-Windows.exe  # Windows
```

默认窗口大小 / 输入映射 / 渲染参数等在 `config/` 下的 JSON：

| 文件 | 用途 |
|------|------|
| `config/window.json` | 窗口大小、标题、全屏策略 |
| `config/input.json` | 键鼠 / 手柄 action 映射 |
| `config/render.json` | 分辨率、视口、letterbox |
| `config/audio.json` | 音量、空间声参数 |
| `config/text.json` | 字体、字号 |

修改 config 后通常**重启游戏**即可生效（构建时会复制到 build 目录，运行时直接读 build 内副本）。

## 八、调试工具与测试

工具目标在 `tools/`（`BUILD_TOOLS=ON` 时编译）：

- `tools/battle_tester/` — 不开 GameScene 直接搭一场战斗调公式 / AI
- `tools/rmlui_tester/` — 单独跑 RmlUi 文档，验证布局 / 数据绑定
- `tools/visual_tester/` — 视觉回归
- `tools/scheduler_dot_dump/` — 把 SystemScheduler 的并行岛打成 DOT 图
- `tools/rpg_importer/` — RPG Maker 风格数据导入

详见 [testing/tools.md](testing/tools.md)。

测试用 GoogleTest（`tests/` 下按 `engine/game/shared/data/scripts` 分层）：

```bash
cmake --build --preset debug --target test         # 构建测试
ctest --preset debug                                # 跑全部测试
ctest --preset debug -R "Battle"                    # 跑名字含 "Battle" 的测试
```

或者直接跑单个测试二进制（在 `build/debug/tests/<dir>/<name>`）。

## 九、常见错误

| 现象 | 原因 / 处理 |
|------|-------------|
| `Could not find Ninja` / `CMAKE_MAKE_PROGRAM is not set` | 装 Ninja（macOS `brew install ninja`，Linux `apt install ninja-build`，Windows `winget install Ninja-build.Ninja`），**或改用 `debug-fallback` 预设**（无需 Ninja） |
| `cmake too old` | 升级 CMake 到 3.21+ |
| `assets/data/xxx.json not found` | 直接从 `build/<preset>/` 启动可执行文件，不是从仓库根目录 |
| 修改了 JSON / Lua 看不到效果 | `cmake --build --preset <name>` 重新触发资源复制（也可手动 cp 进 build 目录验证） |
| ASan / TSan 在 Windows 编译失败 | 预设条件已自动跳过，确认你用的是 `debug-asan` / `debug-tsan` 而非默认 `debug` |
| 重新切换分支后构建怪异 | 删 `build/<preset>/` 后重 configure；不要混合不同分支的 build 目录 |
| Linux 运行时报 `No available video device` | configure 时缺 X11 / Wayland 开发包，SDL3 编成了无视频后端的库。安装上文 Linux 系统包后删 `build/<preset>/` 重新 configure |
| Linux / GCC 报找不到 `<format>` / `std::format` | GCC 需 13+。`g++ --version` 确认；必要时 `apt install g++-13` 并 `CXX=g++-13 cmake --preset debug` |
| 首次 configure 卡在克隆依赖 | FetchContent 在线拉取 SDL3 等依赖，确认能访问 GitHub；或把源码预置到 `external/` |

## 十、推荐工作流

- **日常迭代**：`debug` 预设
- **要试性能影响**：`relwithdebinfo`（保留符号但优化）
- **遇到崩溃**：先用 `debug-asan` 跑一次，多数情况下能定位到行号
- **怀疑数据竞争**：`debug-tsan`
- **CI / 教师批改**：`release` + `ctest --preset release`

更细的崩溃定位（LLDB / sanitizer 输出格式 / 给 AI 报错模板）见 [调试与崩溃定位](tutorial/debugging.md)。

## 相关文档

- [启动到第一帧](engine/entry_to_first_frame.md) — 可执行文件里第一帧之前发生了什么
- [项目总览](overview.md) — 顶层目录结构与技术栈
- [调试与验证工具](testing/tools.md) — `tools/` 下各工具用法
- [调试与崩溃定位](tutorial/debugging.md) — ASan / TSan / LLDB 的详细用法
- [运行时装配](game/runtime-assembly.md) — 可执行文件启动后如何装配 catalog / service / system
