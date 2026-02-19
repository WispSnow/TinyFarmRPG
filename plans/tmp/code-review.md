# FND-006 代码审核

## 审核信息
- 审核日期：`2026-02-18`
- 审核对象：FND-006 Lua + Sol2 宿主层实现（未提交工作区）
- 审核者：Claude Opus
- 编码要求依据：`for_agent/code-guide.md`
- 结论：**通过，需修正 2 个问题**

---

## 一、构建验证

| 配置 | CMake Configure | Build | 测试结果 |
|------|----------------|-------|----------|
| `ENABLE_SCRIPTING=OFF` | 成功 | 成功（0 error / 0 warning） | 193 passed, 1 failed*, 10 skipped / 194 total |
| `ENABLE_SCRIPTING=ON` | 成功（Lua 5.4.8 本地源码, sol2 手动 INTERFACE） | 成功（0 error / 0 warning） | 196 passed, 1 failed*, 10 skipped / 197 total |

*唯一失败项 `InputManagerTest.ImGuiForwarderReceivesQueuedEvents` 为既有问题（`ENABLE_DEBUG_UI=OFF` 配置下一直存在），与本次变更无关。

新增 3 个脚本测试全部通过，既有测试无回归。

---

## 二、计划审核建议落实情况

| 审核项 | 状态 |
|--------|------|
| B1 Sol2 版本标识 | 已落实（`docs/overview.md` 及 CMake 日志标注 `v3.5.0 (project version 4.0.0)`） |
| B2 Sol2 不走 `add_subdirectory` | 已落实（手动 INTERFACE target） |
| B3 Lua 排除 `lua.c`/`luac.c` | 已落实（同时排除 `onelua.c`） |
| S1 ScriptHost 构造签名 | 已落实（`registry&` + `dispatcher&`） |
| S2 软失败模式 | 已落实（`tryInitScriptHost` 不阻塞 `assembleServices`） |
| S3 步骤排序 | 已落实 |
| S4 FetchContent 回退 | 已落实（`FetchContent_Populate` 不走 `MakeAvailable`） |
| S5 测试资源路径 | 已落实（`PROJECT_SOURCE_DIR` 宏 + `tests/scripts/`） |
| A1 `TF_ENABLE_SCRIPTING` 宏 | 已落实（`target_compile_definitions(game PUBLIC TF_ENABLE_SCRIPTING)`） |
| A3 `system_bundle.cpp` include | 已落实 |

---

## 三、必须修正的问题

### P1. `execString` 方法完全冗余

**位置**：`src/game/script/script_host.h:19` / `script_host.cpp:68-70`

```cpp
// script_host.cpp:68-70
bool ScriptHost::execString(std::string_view script) {
    return exec(script);
}
```

`execString` 是 `exec` 的直接转发，增加了无意义的接口面。代码指南要求"保持代码精简"。

**建议**：删除 `execString`，统一使用 `exec`。测试中 3 处 `execString` 调用改为 `exec`。

---

### P2. `interact` 命令绑定中不必要的 optional 包装

**位置**：`src/game/script/script_bindings.cpp:157-159`

```cpp
command_api.set_function(
    "interact",
    [&registry, &dispatcher](std::uint32_t target_id,           // <-- 必填参数
                             sol::optional<std::uint32_t> player_id) -> bool {
        const entt::entity target = resolveTargetEntity(registry,
            sol::optional<std::uint32_t>{target_id});            // <-- 强制包装为 optional
```

`target_id` 是必填参数（`std::uint32_t`，非 `sol::optional`），但被手动包装为 `sol::optional` 传入 `resolveTargetEntity`。该函数在 `has_value()` 为 true 时验证实体有效性，否则回退到 player entity。对 `interact` 命令来说，target 不应回退到 player 自身。

**建议**：直接验证，避免语义歧义：

```cpp
const entt::entity target = static_cast<entt::entity>(target_id);
if (!registry.valid(target)) {
    spdlog::warn("ScriptHost: interact 目标实体无效: {}", target_id);
    return false;
}
```

---

## 四、建议改进（非阻塞）

### S1. `<filesystem>` 引入范围

**位置**：`src/game/runtime/game_runtime_assembler.cpp:61`

这是项目中首次引入 `<filesystem>`。当前仅用于 `tryInitScriptHost` 中检测 `bootstrap.lua` 是否存在——区分"脚本不存在"（info 级别）和"脚本执行失败"（error 级别）的语义差异，用法合理。作为项目唯一的 `<filesystem>` 依赖点，可考虑加注释说明引入理由。

### S2. `ScriptingDependencies.cmake` 中 `CMP0169 OLD` 策略

**位置**：`cmake/ScriptingDependencies.cmake:9-11`

```cmake
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
```

为兼容 CMake 3.30+ 对 `FetchContent_Populate` 弃用警告的临时处理。当前可行，未来项目提升 CMake 最低版本后应迁移到 `FetchContent_MakeAvailable` + `EXCLUDE_FROM_ALL`。低优先级。

---

## 五、代码质量评分

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构设计 | 优 | ScriptHost 作为 service 而非 ECS system，职责清晰；独立 CMake 模块分离脚本依赖管理 |
| 错误处理 | 优 | 全链路 spdlog 日志，不向上抛异常，软失败不阻塞场景初始化 |
| 与现有模式一致性 | 优 | `#ifdef` guard 风格与 `TF_ENABLE_DEBUG_UI` 一致；构造签名遵循 registry+dispatcher 惯例 |
| CMake 工程 | 优 | 双来源策略完整（本地 > 下载），Lua 排除正确，Sol2 绕过版本冲突 |
| 测试覆盖 | 良 | 3 个新测试覆盖 smoke + command bridge；缺少错误路径测试（如加载不存在的文件、执行语法错误脚本） |
| 代码精简度 | 良 | `execString` 冗余；`interact` 绑定有不必要的间接；其余精简 |
| 绑定 API 设计 | 优 | 最小 API 面（time/player/command/dialogue），可选参数用 `sol::optional` 处理得当 |

---

## 六、文件级评审

### 新增文件

| 文件 | 行数 | 评审 |
|------|------|------|
| `cmake/ScriptingDependencies.cmake` | 148 | 结构清晰，双来源策略完整，函数分层合理 |
| `src/game/script/script_host.h` | 34 | 接口精简，`[[nodiscard]]` 正确使用。删除 `execString` 后更精简 |
| `src/game/script/script_host.cpp` | 102 | 错误处理统一收口到 `runResult`，`ensureReady` 保护未初始化调用 |
| `src/game/script/script_bindings.h` | 11 | 最小接口，仅暴露 `bindScriptAPI` |
| `src/game/script/script_bindings.cpp` | 219 | API 绑定完整，参数验证到位，匿名命名空间隔离内部函数 |
| `assets/scripts/bootstrap.lua` | 8 | 最小示例脚本，防御性检查合理 |
| `tests/game/script_host_smoke_test.cpp` | 67 | 覆盖内联执行和文件加载两个路径 |
| `tests/game/script_host_command_bridge_test.cpp` | 68 | 端到端验证 脚本 -> command -> domain effect 完整链路 |
| `tests/scripts/test_command.lua` | 6 | 最小测试辅助脚本 |

### 修改文件

| 文件 | 变更 | 评审 |
|------|------|------|
| `CMakeLists.txt` | +17 行：option, include, 条件链接与宏定义 | 正确，位置合理 |
| `cmake/Dependencies.cmake` | +1 行（尾部空行） | 无影响 |
| `src/CMakeLists.txt` | +7 行：条件 `target_sources` | 正确，与 `ENABLE_DEBUG_UI` 风格一致 |
| `src/game/runtime/system_bundle.h` | +9 行：forward declaration + 成员 | 正确，`#ifdef` guard 与 `TF_ENABLE_DEBUG_UI` 风格一致 |
| `src/game/runtime/system_bundle.cpp` | +3 行：条件 include | 正确，保证 `unique_ptr` 析构完整性 |
| `src/game/runtime/game_runtime_assembler.cpp` | +32 行：include + `tryInitScriptHost` + 调用点 | 软失败实现正确，bootstrap 加载语义清晰 |
| `tests/CMakeLists.txt` | +7 行：条件追加测试源文件 | 正确 |
| `docs/overview.md` | +5 行：技术栈更新 | 准确，CMake 版本修正为 3.13+ |

---

## 七、结论

实现质量整体优秀，计划审核中提出的所有修正建议均已落实。CMake 依赖管理、ScriptHost 架构、错误处理链路和测试覆盖都满足 FND-006 验收标准。

**必须修正**：P1（删除冗余 `execString`）、P2（`interact` 绑定避免不必要的 optional 包装）。

修正后即可提交。
