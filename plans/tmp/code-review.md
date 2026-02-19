# FND-007 代码审查报告

**审查人**: Claude Opus 4.6
**审查日期**: 2026-02-19
**计划文档**: `plans/foundation/FND-007.md`
**编码规范**: `for_agent/code-guide.md`
**构建验证**: `ENABLE_SCRIPTING=ON` 编译通过，212/212 测试通过
**结论**: **有条件通过** — 整体实现质量高，有 2 个必须修复的问题和若干改进建议。

---

## 一、变更概览

| 文件 | 类型 | 行数变化 |
|---|---|---|
| `src/game/script/script_entity_handle.h` | 新增 | +23 |
| `src/game/script/script_entity_handle.cpp` | 新增 | +15 |
| `src/game/script/script_host.h` | 修改 | +13 |
| `src/game/script/script_host.cpp` | 修改 | +115 |
| `src/game/script/script_bindings.h` | 修改 | +5/-4 |
| `src/game/script/script_bindings.cpp` | 修改 | +246/-125 |
| `src/game/scene/game_scene.cpp` | 修改 | +13 |
| `tests/game/script_host_security_boundary_test.cpp` | 新增 | ~110 |
| `tests/game/script_host_handle_lifecycle_test.cpp` | 新增 | ~65 |
| `tests/game/script_host_smoke_test.cpp` | 修改 | +5/-3 |
| `tests/scripts/test_command.lua` | 修改 | +3/-3 |
| `src/CMakeLists.txt` | 修改 | +1 |
| `tests/CMakeLists.txt` | 修改 | +2 |
| `docs/overview.md` | 修改 | +6 |
| `plans/foundation/FND-007.md` | 修改 | 待办清单标记完成 |

---

## 二、编码规范一致性检查

> 规范来源: `for_agent/code-guide.md`

### 2.1 现代 C++ 语法风格 (C++17/C++20)

| 检查项 | 结果 |
|---|---|
| `std::string_view` 用于不持有的字符串参数 | ✅ 全部使用正确 |
| `[[nodiscard]]` 标注有返回值的函数 | ✅ 所有 public/free 函数已标注 |
| `const` 正确性 | ✅ `makeHandle` / `validateHandle` / `isReady` / `sceneToken` 均为 const |
| `noexcept` 标注 | ✅ `isNullHandle` / `toRawEntity` / `makeHandle` / `isReady` / `sceneToken` |
| 避免裸 new/delete | ✅ 仅使用 `unique_ptr` 和栈对象 |
| 聚合初始化 / 列表初始化 | ✅ `ScriptEntityHandle` 使用默认成员初始化 |

### 2.2 保持代码精简，不做过度防御

| 检查项 | 结果 |
|---|---|
| 无冗余 try-catch | ✅ 仅 `init()` 有 catch（因 sol2 可能抛异常） |
| 无冗余空指针检查 | ✅ |
| 无冗余注释 | ⚠️ 见 3.3 |

### 2.3 不使用异常

| 检查项 | 结果 |
|---|---|
| 新增代码未抛异常 | ✅ 全部使用返回值 + 日志 |
| `init()` 中的 catch | ✅ 合理 — sol2 内部可能抛异常，catch 后转为 bool |

### 2.4 仅考虑最优方案，不考虑向后兼容

| 检查项 | 结果 |
|---|---|
| 旧的 `uint32_t` entity 路径已完全移除 | ✅ |
| 无兼容层、无废弃标记 | ✅ |

---

## 三、技术问题

### 🔴 P0 — 必须修复

#### 3.1 `rawget` 未封堵，只读代理表可被绕过

**位置**: `script_host.cpp:198` (`hardenLuaGlobals`)

当前封堵了 `rawset` 以防绕过 `__newindex`，但**未封堵 `rawget`**。虽然 `rawget` 不能写入，但它可以绕过只读代理表的 `__index` metatable 直接访问底层表。由于代理表本身是空的（实际数据在 `source` 表中，通过 `__index` 委托），`rawget(tf, "time")` 会返回 `nil`，这不是安全问题而是**语义一致性问题**。

但更关键的是：**`rawset` 配合 `rawget` 可以绕过代理模式**。当前已封堵 `rawset`，但应同步封堵 `rawget` 以保持语义一致且消除困惑：

```cpp
lua_["rawget"] = sol::lua_nil;
```

**严重程度**: 中（不是安全漏洞，但是设计遗漏）。
**修复成本**: 1 行。

#### 3.2 `tf.player.id()` 残留 — 仍暴露裸 entity ID

**位置**: `script_bindings.cpp:128-129`

```cpp
player_impl.set_function("id", [&registry]() -> std::uint32_t {
    return static_cast<std::uint32_t>(game::system::helpers::getPlayerEntity(registry));
});
```

FND-007 的核心目标之一是"旧版直接传 `uint32_t entity id` 路径移除"。新增了 `tf.player.handle()` 来替代，但 `tf.player.id()` 仍然暴露裸 entity ID。虽然当前所有 command/dialogue API 已不再接受 `uint32_t`，所以脚本拿到 `id()` 也无法直接用于命令调用，但这仍然是一个设计上的矛盾：

- 暴露了不应暴露的内部实现细节
- 与"统一走句柄"的设计哲学冲突
- 未来新增 API 时可能有人误用此 ID

**建议**: 移除 `tf.player.id()`，或将其重命名为 `tf.player.debug_id()` 并标注仅用于日志/调试。如果保留，应在安全边界测试中增加说明性注释。

**严重程度**: 高（与计划目标直接矛盾）。

---

### 🟡 P1 — 强烈建议修复

#### 3.3 `resolveCommandTargetEntity` 与 `resolveDialogueTargetEntity` 重复度高

**位置**: `script_bindings.cpp:48-76`

两个 resolve 函数的逻辑几乎相同，唯一区别是 command 版在找不到玩家时返回 `false`，dialogue 版返回 `true`（允许 target 为 null）。这可以合并为单个函数加一个 `bool allow_null_default` 参数：

```cpp
[[nodiscard]] bool resolveTargetEntity(game::script::ScriptHost& host,
                                       entt::registry& registry,
                                       const sol::optional<game::script::ScriptEntityHandle>& raw_target,
                                       std::string_view api_name,
                                       entt::entity& out_target,
                                       bool require_default_player = true) {
    if (raw_target.has_value()) {
        return host.validateHandle(raw_target.value(), out_target, api_name);
    }
    out_target = game::system::helpers::getPlayerEntity(registry);
    if (out_target == entt::null && require_default_player) {
        spdlog::warn("ScriptHost: {} 失败，未找到默认玩家实体", api_name);
        return false;
    }
    return true;
}
```

**严重程度**: 低（不影响功能，但违反 DRY 原则）。

#### 3.4 `g_next_scene_token` 使用 `std::atomic` 但当前架构是单线程

**位置**: `script_host.cpp:22`

```cpp
std::atomic<std::uint64_t> g_next_scene_token{1};
```

当前游戏是单线程架构，`ScriptHost` 的创建和销毁都在主线程。使用 `atomic` 是过度防御。虽然 `memory_order_relaxed` 开销极低，但按照编码规范"保持代码精简，不做过度防御"的要求，应改为普通静态变量：

```cpp
std::uint64_t g_next_scene_token = 1;
```

如果项目未来确实需要多线程脚本宿主，可以在那时再加 atomic。

**严重程度**: 低（违反"不做过度防御"原则，但无功能影响）。

#### 3.5 `ScriptEntityHandle` 的 `scene_token` 在 Lua 侧暴露为 `readonly_property`

**位置**: `script_bindings.cpp:93-94`

```cpp
"scene_token",
sol::readonly_property([](const ScriptEntityHandle& handle) { return handle.scene_token; }),
```

将 `scene_token` 暴露给 Lua 脚本侧没有明确的使用场景。脚本不应关心也不应看到 scene token 的具体值。暴露它可能导致：
- 脚本开发者对其产生依赖或做出错误假设
- 增加 API 表面积

**建议**: 仅保留 `entity_id`（用于日志/调试）和 `is_valid`，移除 `scene_token` 的 Lua 侧暴露。C++ 侧校验不受影响。

---

### 🟢 P2 — 可选改进

#### 3.6 `shutdown()` 中 `lua_ = sol::state{}` 的语义

**位置**: `script_host.cpp:65`

```cpp
void ScriptHost::shutdown() {
    lua_ = sol::state{};
    // ...
}
```

move-assign 一个默认构造的 `sol::state` 可以正常工作（先关闭旧 state，再持有新的空 state），但语义上不如 `lua_.~state(); new (&lua_) sol::state();` 或直接调用 sol2 的 close 能力清晰。不过当前写法功能正确，且 sol2 的 move assignment 有良好保证，可以保留。

#### 3.7 `configureInstructionLimit` 在 `shutdown()` 后不需要清理

`shutdown()` 通过 `lua_ = sol::state{}` 替换了整个 Lua state，所以 hook 随旧 state 一起销毁。新 state 没有 hook。这是正确的。但如果未来 `shutdown()` 改为只清理部分状态而不替换整个 state，hook 需要显式移除。当前无需修改。

#### 3.8 `SCRIPT_INSTRUCTION_LIMIT = 200000` 的选值

**位置**: `script_host.cpp:21`

200K 条指令对于一个同步脚本调用来说是合理的上限。作为参考：
- 一个简单的 `for i=1,1000 do end` 循环大约消耗 ~4000 条指令
- 200K 足以执行复杂的数据查询和命令链
- 死循环会在约 0.1 秒内被拦截

选值合理，无需调整。

#### 3.9 测试中 `seedPlayer` 辅助函数在两个测试文件中重复定义

**位置**:
- `script_host_security_boundary_test.cpp:13-18`
- `script_host_handle_lifecycle_test.cpp:13-18`

两处定义完全相同。可以提取到共享的测试辅助头文件中。但考虑到测试文件的独立性原则和仅有 2 处重复，当前可以接受。

---

## 四、架构评估

### 4.1 只读代理表模式

`createReadOnlyProxy()` 的实现正确且优雅：
- 使用 `__index` 委托到真实实现表
- 使用 `__newindex` 拦截写入并报错
- 使用 `__metatable = "locked"` 防止 `getmetatable()` 返回真实 metatable

递归保护（`tf` 及其子表 `tf.time`/`tf.player`/`tf.command`/`tf.dialogue` 都各自是独立的代理）正确实现。

### 4.2 句柄校验通道

`ScriptHost::validateHandle()` 的校验链完整且优先级正确：
1. `ready_` 状态检查
2. null 句柄检查
3. `scene_token` 匹配检查
4. `registry.valid()` 检查

失败时仅 warn 日志 + 返回 false，不抛异常、不崩溃。符合计划要求。

### 4.3 Sol2 usertype 注册

`ScriptEntityHandle` 注册为 `sol::no_constructor` 的 usertype，这比 plain table 更安全：
- 脚本无法直接构造句柄（只能通过 `tf.player.handle()` 获取）
- Sol2 的类型检查会拒绝非 `ScriptEntityHandle` 类型的参数
- 属性为 `readonly_property`，脚本无法篡改

### 4.4 场景生命周期回收

`GameScene::clean()` 的执行顺序正确：
1. `script_host->shutdown()` — 关闭 Lua state，失效化 token
2. `script_host.reset()` — 释放 ScriptHost 对象
3. `dispatcher.clear<DialogueShowEvent/HideEvent>()` — 清理残留事件
4. debug UI cleanup
5. `Scene::clean()` — 重置 registry

先 shutdown 再 reset 确保了 Lua state 中的引用不会在 registry 重置后被访问。dispatcher 事件清理解决了计划审查中提出的跨场景事件泄漏问题。

### 4.5 `hardenLuaGlobals()` 安全措施

| 封堵目标 | 已处理？ |
|---|---|
| `package` 库（`require` / `loadlib`） | ✅ 未加载 |
| `io` 库 | ✅ 未加载 |
| `os` 库 | ✅ 未加载 |
| `debug` 库 | ✅ 未加载 |
| `dofile` | ✅ 设为 nil |
| `loadfile` | ✅ 设为 nil |
| `load` | ✅ 设为 nil |
| `rawset` | ✅ 设为 nil |
| `rawget` | ❌ 未封堵（见 3.1） |
| `string.dump` | ✅ 设为 nil |
| 指令计数限制 | ✅ `lua_sethook` 200K |

---

## 五、测试覆盖评估

### 5.1 新增测试

| 测试名 | 覆盖场景 | 评估 |
|---|---|---|
| `OnlyWhitelistedApiIsExposed` | io/os/package/rawset 为 nil | ✅ |
| `DofileLoadfileLoadAreBlocked` | dofile/loadfile/load 为 nil | ✅ |
| `TfNamespaceIsReadOnly` | 写入 tf 和 tf.time 被拦截 | ✅ |
| `InvalidHandleIsSafelyRejected` | 销毁实体后句柄失效 | ✅ |
| `InfiniteLoopIsAbortedByInstructionLimit` | 死循环被中断 + 后续执行正常 | ✅ |
| `StaleHandleAfterShutdownIsRejected` | shutdown 后句柄失效 | ✅ |
| `SceneSwitchInvalidatesOldHandles` | 跨 ScriptHost 实例句柄失效 | ✅ |

### 5.2 更新测试

| 测试名 | 变更 | 评估 |
|---|---|---|
| `LoadAndRunInlineScriptWithoutCrash` | 改用 `tf.player.handle()` + `tf.command.interact(handle)` | ✅ |
| `ScriptCanEmitCommandAndProduceDomainEffect` | 无变更（因 `issue_add_item` 不传 target 时走默认玩家路径） | ✅ |

### 5.3 缺失测试

| 场景 | 严重程度 |
|---|---|
| `rawget` 被封堵的验证 | 低（取决于 3.1 是否修复） |
| 脚本传入非 `ScriptEntityHandle` 类型参数（如数字）被 Sol2 拒绝 | 低（Sol2 usertype 保证） |
| `tf.command` / `tf.dialogue` 子表的只读性（当前仅测 `tf` 和 `tf.time`） | 低（同一 `createReadOnlyProxy` 逻辑） |

---

## 六、与计划对照

| 计划待办 | 实现状态 | 评估 |
|---|---|---|
| T1 句柄契约 | ✅ 含两层校验注释 | |
| T1.1 测试骨架 | ✅ | |
| T2 白名单初始化 | ✅ 移除 package，只读代理 | |
| T2.1 危险函数封堵 | ⚠️ 缺 `rawget` | |
| T2.2 指令上限 | ✅ 200K + hook | |
| T3 validateHandle + shutdown | ✅ 幂等 | |
| T4 bindings 句柄化 | ⚠️ `tf.player.id()` 残留 | |
| T5 GameScene 回收 | ✅ shutdown + reset + 事件清理 | |
| T5.1 清理脚本队列事件 | ✅ DialogueShow/Hide clear | |
| T6 安全边界测试 | ✅ | |
| T6.1 DofileLoadfile + ReadOnly | ✅ | |
| T6.2 InfiniteLoop | ✅ | |
| T7 句柄生命周期测试 | ✅ | |
| T8 更新现有测试 | ✅ | |
| T9 test_command.lua | ✅ | |
| T10 CMakeLists | ✅ | |
| T11 ctest 全量回归 | ✅ 212/212 | |
| T12 docs/overview.md | ✅ | |

---

## 七、结论

### 必须修复（合并前）

1. **移除 `tf.player.id()`**（或降级为 debug 用途）— 与计划核心目标"移除裸 uint32_t 实体入口"直接矛盾。
2. **封堵 `rawget`** — 与已封堵 `rawset` 保持一致，消除设计遗漏。

### 强烈建议

3. 合并两个 `resolve*TargetEntity` 为单一函数（DRY）。
4. `g_next_scene_token` 去掉 `std::atomic`（当前单线程架构无需原子操作）。
5. 移除 `ScriptEntityHandle` Lua 侧的 `scene_token` 暴露。

### 总体评价

实现质量 **8.5/10**。代码风格一致、架构决策合理、测试覆盖充分。安全收口（白名单、只读代理、usertype 句柄、指令限制、场景回收）全部到位。主要扣分点是 `tf.player.id()` 残留和 `rawget` 遗漏。修复这两个问题后即可提交。
