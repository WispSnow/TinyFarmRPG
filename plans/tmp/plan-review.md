# FND-006 计划审核

## 审核信息
- 审核日期：`2026-02-18`
- 审核对象：`plans/foundation/FND-006.md`
- 审核者：Claude Opus
- 结论：**有条件通过** — 需修正 3 个问题，建议采纳 5 项改进

---

## 一、基线验证

| 检查项 | 计划描述 | 实际验证 | 结果 |
|--------|---------|----------|------|
| CMake 最低版本 | 未明确提及 | `cmake_minimum_required(VERSION 3.13.0)` | — |
| 现有选项 | `BUILD_SHARED_LIBS` + `ENABLE_DEBUG_UI`（第34行） | 实际还有 `BUILD_TOOLS`（L137）和 `BUILD_TESTING`（L147） | **偏差**（不影响执行） |
| `Dependencies.cmake` 入口行号 | L233 | `setup_project_dependencies()` 确在 L233 | 通过 |
| `system_bundle.h` 行号 | L36 | `GameRuntimeServices` 确在 L36 | 通过 |
| `external/lua-5.5.0` 无 CMakeLists | 是 | 是（仅 Makefile + 34 个 .c 文件，含 `lua.c`/`luac.c`） | 通过 |
| `external/sol2-3.5.0` 可提供头文件 | 是 | `include/sol/` 存在，CMakeLists.txt 导出 `sol2::sol2` INTERFACE target | 通过 |
| 回归基线 | 202/202 | 需要实际执行 ctest 确认 | 待验证 |

---

## 二、必须修正的问题（Blocker）

### B1. Sol2 版本标识与实际不符

**位置**：计划全文 + `plans/tmp/note.md`

**问题**：计划称 Sol2 版本为 `v3.5.0`，本地目录也命名为 `sol2-3.5.0`。但实际查看 `external/sol2-3.5.0/CMakeLists.txt:30`：

```cmake
project(sol2 VERSION 4.0.0 LANGUAGES CXX C)
```

这意味着本地实际是 Sol2 **4.0.0**（sol2 在 GitHub `v3.5.0` tag 发布的就是 4.0.0 工程版本）。默认 Lua 版本也是 `5.4.4`（L45），而非 `5.5.0`。

**影响**：不影响功能实现（Sol2 4.0 API 向下兼容 3.x），但以下两点需要注意：
1. `SOL2_LUA_VERSION` 默认值为 `"5.4.4"`，如果使用 `add_subdirectory` 方式接入 Sol2，需要 override 或确保不触发其内部 Lua 查找逻辑。
2. 文档和日志中应统一为 `sol2 v3.5.0 (project version 4.0.0)` 或直接用目录名 `sol2-3.5.0`，避免后续版本追溯混乱。

**建议修正**：在计划"风险与回滚"节补充此版本差异说明；实现时确保设置 `SOL2_BUILD_LUA FALSE` 以绕过内部 Lua 构建。

---

### B2. Sol2 CMake 最低版本冲突

**位置**：步骤 T4（Sol2 目标归一）

**问题**：`external/sol2-3.5.0/CMakeLists.txt:25` 声明：

```cmake
cmake_minimum_required(VERSION 3.26.0)
```

而项目顶层要求 `cmake_minimum_required(VERSION 3.13.0)`。若通过 `add_subdirectory` 接入，CMake 3.26 之前的版本会产生严重 policy 兼容问题；即使当前开发机 CMake >= 3.26 也可能导致 CI 或其他环境构建失败。

**影响**：直接调用 `add_subdirectory(external/sol2-3.5.0 ...)` 不可靠。

**建议修正**：**放弃对 Sol2 使用 `add_subdirectory`**，改为手动创建 INTERFACE target：

```cmake
# Sol2: header-only 接入，绕过其 CMakeLists.txt 的 3.26 版本要求
add_library(sol2_headers INTERFACE)
add_library(sol2::sol2 ALIAS sol2_headers)
target_include_directories(sol2_headers INTERFACE
    ${CMAKE_SOURCE_DIR}/external/sol2-3.5.0/include)
target_link_libraries(sol2_headers INTERFACE Lua::Lua)
```

计划步骤 T4 应将此策略写入，并在"实现思路"第 2 点更新为"Sol2 采用手动 INTERFACE target 接入，不调用 `add_subdirectory`"。

**备注**：FetchContent 回退路径同样面临此问题。若本地源码缺失而走 FetchContent 下载 Sol2，也不能对其执行 `add_subdirectory`。建议 FetchContent 下载后仍走手动 INTERFACE target 模式（仅取头文件目录）。

---

### B3. Lua 自定义构建需排除 `lua.c` 和 `luac.c`

**位置**：步骤 T3（Lua 目标归一）

**问题**：计划提到"Lua 采用项目内统一构建包装"，但未明确排除 `lua.c`（Lua 解释器入口）和 `luac.c`（编译器入口）。这两个文件各自包含 `main()` 函数，如果误编入静态库会导致链接冲突。

**影响**：若使用 `file(GLOB ...)` 收集 `external/lua-5.5.0/src/*.c` 并直接构建，将产生多 main 符号链接错误。

**建议修正**：在步骤 T3 明确列出排除项：

```cmake
file(GLOB LUA_SOURCES ${CMAKE_SOURCE_DIR}/external/lua-5.5.0/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_SOURCE_DIR}/external/lua-5.5.0/src/lua.c
    ${CMAKE_SOURCE_DIR}/external/lua-5.5.0/src/luac.c)

add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC
    ${CMAKE_SOURCE_DIR}/external/lua-5.5.0/src)
add_library(Lua::Lua ALIAS lua_static)
```

---

## 三、建议改进（Non-Blocker）

### S1. ScriptHost 构造签名应匹配现有模式

**位置**：步骤 T6（新增 `script_host.h/.cpp`）

**问题**：计划未明确 `ScriptHost` 的构造参数。查看项目中所有系统和服务的模式（如 `DialogueSystem`、`InventorySystem`、`SaveService`），构造时传入 `entt::registry&` 和 `entt::dispatcher&` 是标准做法。

**建议**：明确 `ScriptHost` 构造签名为：

```cpp
ScriptHost(entt::registry& registry, entt::dispatcher& dispatcher);
```

并在 `assembleServices` 中传入 `params.context.getDispatcher()`。这也意味着 `ServiceBuildParams` 当前不直接持有 `dispatcher`，但可通过 `params.context.getDispatcher()` 间接获取，与 `assembleSystems` 中的做法一致。

---

### S2. 建议将 ScriptHost 初始化定义为软失败

**位置**：步骤 T8（runtime 装配接入）

**问题**：计划"错误处理"节提到"脚本报错只记录日志，不导致主循环崩溃"，但未明确初始化失败时是否阻塞整个 `assembleServices`。查看 `assembleServices` 当前模式——所有步骤失败都 `return false`，导致场景初始化失败。

**建议**：ScriptHost 初始化应为**软失败**——失败时仅 warn 并将 `script_host` 置为 nullptr，不阻塞场景启动：

```cpp
#ifdef TF_ENABLE_SCRIPTING
services.script_host = std::make_unique<game::script::ScriptHost>(
    params.registry, params.context.getDispatcher());
if (!services.script_host->init()) {
    spdlog::warn("ScriptHost 初始化失败，脚本功能将禁用");
    services.script_host.reset();
    // 注意：不 return false
}
#endif
```

在步骤 T8 中明确此行为。

---

### S3. 步骤排序优化：T5 应后移至 T7 之后

**位置**：步骤 T5（链接脚本依赖）

**问题**：T5 要求在 `src/CMakeLists.txt` 中将脚本源文件添加到 `game` target 并链接依赖。但此时 `script_host.h/.cpp`（T6）和 `script_bindings.h/.cpp`（T7）尚未创建。若按顺序执行，T5 会导致构建失败（引用不存在的源文件）。

**建议**：将 T5 调整为在 T7 之后执行，或合并入 T6/T7：

```
T3 -> T4 -> T6 -> T7 -> T5（链接+添加源文件） -> T8 -> ...
```

这样在添加 CMake 源文件引用时，文件已经存在，可立即验证编译。

---

### S4. FetchContent 回退路径中的 Lua 构建方案需补充

**位置**：步骤 T3 的"本地缺失自动下载"分支

**问题**：计划要求"本地缺失时自动 FetchContent 下载"。但 Lua 上游 `https://github.com/lua/lua` 同样没有 `CMakeLists.txt`（只有 Makefile）。因此 FetchContent 下载后仍然无法使用 `find_or_fetch_dependency` 宏（该宏内部走 `add_subdirectory`）。

**建议**：Lua 的 FetchContent 路径需要自定义处理——先用 `FetchContent_Declare` + `FetchContent_Populate`（不调用 `MakeAvailable`），然后用与本地路径相同的手动 `add_library` 方式构建。在步骤 T3 中补充此分支逻辑：

```cmake
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/external/lua-5.5.0)
    FetchContent_Declare(lua
        GIT_REPOSITORY https://github.com/lua/lua
        GIT_TAG v5.5.0
        GIT_SHALLOW TRUE)
    FetchContent_Populate(lua)
    set(LUA_SOURCE_DIR ${lua_SOURCE_DIR})
else()
    set(LUA_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/lua-5.5.0/src)
endif()
# 然后统一用 file(GLOB) + add_library 构建
```

---

### S5. 测试文件需要脚本资源路径机制

**位置**：步骤 T10/T11（新增测试）

**问题**：`script_host_smoke_test.cpp` 需要加载 Lua 脚本文件。但测试运行时 CWD 可能不在项目根目录。项目现有测试使用 `PROJECT_SOURCE_DIR` 宏解决此问题（见 `tests/CMakeLists.txt:66`/`tests/CMakeLists.txt:125`）。

**建议**：
1. 测试中用 `PROJECT_SOURCE_DIR "/assets/scripts/bootstrap.lua"` 拼接路径，或在 `tests/data/` 下放置测试专用的小型 Lua 脚本（推荐后者，避免测试依赖运行时资产）。
2. 对于 `ScriptHostSmokeTest.LoadAndRunFileWithoutCrash`，可以使用内联字符串执行（`host.exec("local x = 1 + 1")`）避免文件依赖。
3. 对于 `ScriptHostCommandBridgeTest`，需一个小型 Lua 脚本触发 command，建议放在 `tests/data/scripts/test_command.lua`。

---

## 四、架构层面的补充观察

### A1. `#ifdef TF_ENABLE_SCRIPTING` 的范围控制

计划中使用 `ENABLE_SCRIPTING` 作为 CMake option，但需要同时定义对应的 C++ 预处理宏。参考 `ENABLE_DEBUG_UI` 的实现（`CMakeLists.txt:66`）：

```cmake
if(ENABLE_DEBUG_UI)
    target_compile_definitions(engine PUBLIC TF_ENABLE_DEBUG_UI)
endif()
```

脚本功能对应应为：

```cmake
if(ENABLE_SCRIPTING)
    target_compile_definitions(game PUBLIC TF_ENABLE_SCRIPTING)
endif()
```

注意放在 `game` 而非 `engine` 上，因为脚本是 game 层概念。这一点计划中未明确提及，实现时需注意。

### A2. `game_scene.cpp` 的已有源码检查测试

`tests/game/game_scene_runtime_assembly_test.cpp` 通过字符串搜索源码来验证架构约束（如检查 `GameRuntimeAssembler::assembleServices` 字符串存在）。若在 `game_scene.cpp` 中添加 `#ifdef TF_ENABLE_SCRIPTING` 代码块，应检查此测试是否需要更新。

### A3. `system_bundle.cpp` 的 include 锚定

`system_bundle.cpp` 用 `= default` 实现析构函数，目的是确保 `unique_ptr` 指向的类型在析构点完整可见（避免 incomplete type deletion）。添加 `ScriptHost` 的 `unique_ptr` 后，需在 `system_bundle.cpp` 中添加：

```cpp
#ifdef TF_ENABLE_SCRIPTING
#include "game/script/script_host.h"
#endif
```

---

## 五、待办清单评审

| 编号 | 描述 | 评审 |
|------|------|------|
| T1 | 新增 `ENABLE_SCRIPTING` 开关 | 通过。注意同时添加 `TF_ENABLE_SCRIPTING` 预处理宏定义。 |
| T2 | `Dependencies.cmake` 增加入口 | 通过。但 Lua/Sol2 都不能直接复用 `find_or_fetch_dependency` 宏，需自定义逻辑。 |
| T3 | Lua 目标归一 | **需修正**：排除 `lua.c`/`luac.c`；补充 FetchContent 回退路径。见 B3/S4。 |
| T4 | Sol2 目标归一 | **需修正**：不走 `add_subdirectory`，改用手动 INTERFACE target。见 B2。 |
| T5 | 链接脚本依赖 | **建议调整顺序**：后移至 T7 之后。见 S3。 |
| T6 | 新增 `script_host.h/.cpp` | 通过。建议明确构造签名。见 S1。 |
| T7 | 新增 `script_bindings.h/.cpp` | 通过。 |
| T8 | runtime 装配接入 | 通过。建议软失败。见 S2。 |
| T9 | 新增 `bootstrap.lua` | 通过。 |
| T10 | 新增 smoke test | 通过。注意资源路径。见 S5。 |
| T11 | 新增 command bridge test | 通过。注意资源路径。见 S5。 |
| T12 | 双配置回归 | 通过。 |
| T13 | 更新文档 | 通过。 |

---

## 六、建议的修正后步骤执行顺序

```
T1  新增 ENABLE_SCRIPTING 开关 + TF_ENABLE_SCRIPTING 定义
T2  Dependencies.cmake 增加 Lua/Sol2 自定义依赖逻辑
T3  Lua 目标归一（排除 lua.c/luac.c，支持 FetchContent 回退）
T4  Sol2 手动 INTERFACE target（不走 add_subdirectory）
T6  新增 script_host.h/.cpp（构造签名：registry + dispatcher）
T7  新增 script_bindings.h/.cpp
T5  src/CMakeLists.txt 添加源文件 + 链接依赖（此时源文件已存在）
T8  runtime 装配接入（软失败模式）
T9  新增 bootstrap.lua
T10 新增 smoke test
T11 新增 command bridge test
T12 双配置回归
T13 更新文档
```

---

## 七、结论

FND-006 计划整体设计合理，目标明确，范围得当。**3 个 Blocker（B1-B3）需在实现前修正**，尤其是 B2（Sol2 CMake 版本冲突）和 B3（Lua 构建排除项）直接影响构建能否成功。5 个改进建议（S1-S5）可在实现过程中采纳。

建议执行策略：先修正计划中的 B1-B3，然后按修正后的步骤顺序逐步实现，每完成 T4 后立即做一次 `ENABLE_SCRIPTING=ON` 的构建验证以尽早暴露依赖接入问题。
