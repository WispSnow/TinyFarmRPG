# FND-007 计划审查报告

**审查人**: Claude Opus 4.6
**审查日期**: 2026-02-19
**计划文档**: `plans/foundation/FND-007.md`
**结论**: **有条件通过** — 整体方向正确，需修正若干技术细节后方可执行。

---

## 一、总体评价

FND-007 的目标精准地瞄准了 FND-006 遗留的三大安全缺口：标准库权限过宽、裸实体 ID 暴露、场景切换无脚本回收。计划结构清晰、步骤划分合理、DoD 明确可测。总体设计方向与现有代码架构兼容，是一份质量较高的开发计划。

---

## 二、与代码基线的一致性验证

### 2.1 基线描述 — 准确

| 计划中的描述 | 代码实际 | 一致？ |
|---|---|---|
| ScriptHost 开启 `base/math/table/string/package` | `script_host.cpp:20` 确认一致 | ✅ |
| script_bindings 以 `uint32_t -> entt::entity` 接收实体 | `resolveTargetEntity()` 确认一致 | ✅ |
| `resolveTargetEntity` 仅做 `registry.valid()` 检查 | 代码确认一致 | ✅ |
| game_runtime_assembler 启动时创建 ScriptHost | `tryInitScriptHost()` 确认一致 | ✅ |
| GameScene::clean() 未执行脚本宿主回收 | 代码仅做 debug UI 清理 + Scene::clean() | ✅ |
| Scene::clean() 会重置 registry_ | `registry_ = entt::registry{}` 确认一致 | ✅ |

**评价**: 基线分析全部准确，说明制定计划时对代码做了充分阅读。

### 2.2 基线描述 — 遗漏

| 遗漏项 | 影响 |
|---|---|
| `base` 库包含 `dofile()`/`loadfile()`/`load()`，可执行任意 Lua 代码 | 仅移除 `package` 不够，需同步封堵 `base` 中的危险函数 |
| `string.dump` 可序列化函数字节码 | 低风险但应纳入安全评估 |
| `tf.command` 使用 `dispatcher.trigger()`（同步）而 `tf.dialogue` 使用 `dispatcher.enqueue()`（异步） | 同步触发意味着命令处理器在脚本调用栈内执行，可能导致重入问题 |
| dispatcher 是应用全局的（来自 Context），不随 Scene::clean() 重置 | 已 enqueue 的脚本事件可能跨场景生存 |

---

## 三、技术问题与改进建议

### 🔴 P0 — 必须修正

#### 3.1 白名单收紧不够彻底

**计划说**: 移除 `package`，保留 `base/math/table/string`。

**问题**: `sol::lib::base` 默认暴露以下危险全局函数：
- `dofile(path)` — 从文件系统加载并执行任意 Lua 文件
- `loadfile(path)` — 从文件系统加载 Lua 文件为函数
- `load(chunk)` — 从字符串加载并编译任意 Lua 代码

即使移除了 `package`，脚本仍可通过 `dofile("/etc/passwd")` 等方式访问文件系统（虽然 `io` 未开启，但 `dofile` 走的是独立路径）。

**建议**: 在 `init()` 中，移除 `package` 后增加以下步骤：
```cpp
lua_["dofile"]  = sol::nil;
lua_["loadfile"] = sol::nil;
lua_["load"]    = sol::nil;
```
应将此操作纳入 **T2** 的范围，并在安全边界测试中增加对应用例。

#### 3.2 `scene_token` 与 EnTT entity version 的关系需明确

**计划说**: 句柄结构为 `entity + scene_token`。

**问题**: EnTT 的 `entt::entity` 本身已编码 version 信息（高位 bits），用于防止实体回收后的 ABA 问题。当前 `resolveTargetEntity` 接收 `uint32_t` 时其实已经包含了 version（因为 Sol2 会做完整的 32-bit 传递），但代码语义上未体现这一点。

需要明确 `scene_token` 的定位：
- 它解决的是**跨场景**失效（同一个 entity 值在新场景中可能合法但语义不同）
- EnTT 的 entity version 解决的是**同场景内**实体回收后的 ABA 问题

**建议**: 在 `script_entity_handle.h` 的头文件注释中明确区分两层校验语义：
1. `scene_token` — 跨场景代际校验
2. `entt::entity`（含内置 version）— 同场景内实体有效性校验

### 🟡 P1 — 强烈建议

#### 3.3 dispatcher 跨场景事件泄漏

**计划说**: `GameScene::clean()` 执行 `script_host->shutdown()`。

**问题**: 即使 ScriptHost 被 shutdown，如果在 shutdown 之前脚本通过 `tf.dialogue.show()` enqueue 了事件（使用 `dispatcher.enqueue()`），这些事件仍然存在于 dispatcher 的队列中。`Scene::clean()` 不会清空 dispatcher（它是 Context 级别的）。这些残留事件在新场景中被 `update()` 时会被 flush，可能引发对不存在实体的操作。

**建议**: 在 `GameScene::clean()` 中，`script_host->shutdown()` 之后、`Scene::clean()` 之前，考虑对脚本相关的事件类型做一次 `dispatcher.clear<EventType>()`，或在事件处理侧增加 scene token 校验。这不属于 FND-007 的核心范围，但应作为已知风险记录在文档中。

#### 3.4 缺少指令计数保护（lua_sethook）

**问题**: 当前和计划中都没有对 Lua 执行的指令数做限制。一个包含 `while true do end` 的脚本会永久阻塞游戏主线程。

**建议**: 虽然计划"非目标"中提到不引入协程调度器，但 `lua_sethook` 设置指令上限是非常轻量的安全措施（约 5 行代码），建议纳入 T2 或 T3 的范围：
```cpp
lua_sethook(lua_.lua_state(), [](lua_State* L, lua_Debug*) {
    luaL_error(L, "script exceeded instruction limit");
}, LUA_MASKCOUNT, 1000000);  // 约 100 万条指令
```

#### 3.5 `tf` 命名空间的只读保护实现方式

**计划说**: "固定导出只读顶层命名空间 `tf`，禁止脚本改写宿主 API 表结构。"

**建议**: Sol2 中实现只读 table 有几种方式，推荐使用 metatable 的 `__newindex` 拦截：
```lua
-- 概念：对 tf 表设置 __newindex 为 error 函数
setmetatable(tf, { __newindex = function() error("tf namespace is read-only") end })
```
需要注意递归保护（`tf.command`、`tf.player` 等子表也需要冻结）。建议在 T2 实现时确保子表也被冻结，并在安全边界测试中验证。

### 🟢 P2 — 建议考虑

#### 3.6 `shutdown()` 的幂等性与 `ready_` 状态机

计划提到 `shutdown()` 需要幂等保护，这很好。建议明确 ScriptHost 的状态机：
```
[Uninitialized] --init()--> [Ready] --shutdown()--> [Shutdown]
                                                        |
                                                   init() 不可再调用
```
如果需要支持场景切换后重新初始化脚本（新场景需要新的脚本上下文），应考虑 `reinit()` 或在 `GameScene` 侧创建全新的 ScriptHost 实例。当前计划中提到 `shutdown + reset`，`reset` 的语义需要明确——是 `unique_ptr::reset()` 还是 ScriptHost 自身的 reset 方法。

#### 3.7 测试文件命名与现有模式

现有测试文件命名为 `script_host_smoke_test.cpp` 和 `script_host_command_bridge_test.cpp`，FND-007 新增 `script_host_security_boundary_test.cpp` 和 `script_host_handle_lifecycle_test.cpp`，命名风格一致，无问题。

#### 3.8 EnTT entity 的 `uint32_t` 表示

在当前代码中，`tf.player.id()` 返回的是 `static_cast<std::uint32_t>(player_entity)`，Sol2 传回 Lua 后是一个 number。需要注意 Lua 5.4 引入了整数子类型，`uint32_t` 在 Sol2 中的传递是准确的。但在新的句柄设计中，建议使用 Sol2 的 usertype 而非 plain table 来表示 `ScriptEntityHandle`，这样可以利用 Sol2 的类型检查来防止脚本伪造句柄。

---

## 四、步骤排序评估

| 步骤 | 描述 | 评估 |
|---|---|---|
| 1 | 定义 ScriptEntityHandle | ✅ 正确，先契约后实现 |
| 2 | 重构 ScriptHost 安全初始化 | ✅ 正确，基础设施先行 |
| 3 | 重构 script_bindings API | ✅ 正确，依赖步骤 1-2 |
| 4 | 接入场景生命周期回收 | ✅ 正确，依赖步骤 2-3 |
| 5 | 补充测试并回归 | ⚠️ 建议调整为测试先行（TDD） |
| 6 | 更新文档 | ✅ 正确，最后收尾 |

**关于步骤 5（测试先行）**: 计划中"测试先行锁定安全契约"的描述很好，但步骤编排上测试放在了第 5 步（实现之后）。建议将安全契约测试的**骨架**前移到步骤 1 之后，先写出失败的测试用例（红色阶段），再逐步实现使其通过。这与 T6-T7 的待办清单有出入，建议调整为：

1. T1: 定义句柄契约 + 安全测试骨架（红）
2. T2-T3: ScriptHost 安全初始化 + 校验入口（绿）
3. T4: bindings 重构（绿）
4. T5: 场景生命周期接入（绿）
5. T6-T9: 完善测试 + 回归

---

## 五、待办清单完整性

| 待办 | 覆盖了？ | 备注 |
|---|---|---|
| T1 句柄契约 | ✅ | |
| T2 白名单初始化 | ⚠️ | 需补充 `dofile/loadfile/load` 封堵 |
| T3 validateHandle + shutdown | ✅ | 需明确 shutdown 后的状态机 |
| T4 bindings 句柄化 | ✅ | |
| T5 GameScene 回收 | ✅ | 需注意 dispatcher 事件残留 |
| T6 安全边界测试 | ⚠️ | 需补充 `dofile/loadfile/load` 被封堵的测试 |
| T7 句柄生命周期测试 | ✅ | |
| T8 更新现有测试 | ✅ | |
| T9 更新 test_command.lua | ✅ | |
| T10 CMakeLists | ✅ | |
| T11 ctest 回归 | ✅ | |
| T12 文档更新 | ✅ | |

**建议新增待办**:
- **T2.1**: 封堵 `base` 库中的 `dofile`/`loadfile`/`load` 全局函数
- **T6.1**: 新增测试 `SecurityBoundary.DofileLoadfileLoadAreBlocked`
- (可选) **T2.2**: 添加 `lua_sethook` 指令计数保护

---

## 六、风险评估补充

计划中的风险分析已覆盖 API 破坏性、调试受限、生命周期遗漏三个主要风险。补充以下：

| 风险 | 等级 | 缓解 |
|---|---|---|
| `base` 库危险函数未封堵，白名单形同虚设 | 高 | 见 3.1 |
| dispatcher 残留事件跨场景生存 | 中 | 见 3.3，记录为已知限制 |
| 脚本无限循环阻塞主线程 | 中 | 见 3.4，考虑加入 lua_sethook |
| Sol2 usertype 与 plain table 选型影响句柄防伪造能力 | 低 | 见 3.8，建议用 usertype |

---

## 七、结论与建议

### 通过条件

1. **必须**: 将 `dofile`/`loadfile`/`load` 封堵纳入 T2 范围（否则移除 `package` 的安全效果大打折扣）
2. **必须**: 明确 `scene_token` 与 EnTT entity version 的两层校验语义
3. **强烈建议**: 在文档中记录 dispatcher 事件跨场景残留的已知限制
4. **建议**: 考虑 `lua_sethook` 指令计数保护（5 行代码，安全收益高）
5. **建议**: ScriptEntityHandle 使用 Sol2 usertype 而非 plain Lua table

### 总体评价

计划质量 **8/10**。方向正确、结构清晰、任务分解合理。主要扣分点是白名单收紧不够彻底（`base` 库危险函数遗漏）和缺少指令计数保护。修正上述问题后即可执行。

**预估工时**: 计划标注 1.5 天，考虑到需要补充的安全措施，实际可能需要 **1.5-2 天**，仍在合理范围内。
