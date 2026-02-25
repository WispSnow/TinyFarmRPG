# FND-006R Lua 宿主下沉到引擎层重构计划

## 元信息
- 任务ID：`FND-006R`
- 任务标题：`Lua/Sol2 脚本支持下沉到 engine 层，game 层仅保留扩展绑定`
- 优先级：`P0`
- 状态：`Done`
- 负责人：`TBD`
- 完成时间：`2026-02-24`
- 依赖任务：`FND-006`（已完成）、`FND-007`（已完成）
- 对应上层计划：`./2026-02-16-foundation-backlog.md`

## 目标
- 将“脚本运行时内核能力”（Lua VM 生命周期、安全收敛、句柄校验、执行保护）从 `src/game` 下沉到 `src/engine`。
- 保持 game 层只负责 TinyFarm 业务扩展（`tf.time/tf.player/tf.command/tf.dialogue` 等绑定），不再承载通用脚本宿主实现。
- 在不考虑向后兼容的前提下，明确脚本系统分层边界，降低未来新玩法/新场景接入脚本时的重复成本。

## 完成情况（2026-02-24）
- 已完成 `Engine Script Core + Game Script Module` 分层：`ScriptHost/ScriptEntityHandle` 下沉到 `src/engine/script`，`game` 仅保留 `tinyfarm_script_module` 扩展绑定。
- 已完成接口与依赖收敛：`ScriptModuleInstaller` 固定签名落地，`ScriptHost` 构造仅持有 `registry`，`dispatcher` 改为 `init(...)` 入参。
- 已完成构建分层：Lua/Sol2 改为 `engine` 目标 `PUBLIC` 链接，`engine` 与 `game` 同步定义 `TF_ENABLE_SCRIPTING`。
- 已完成测试分层：宿主安全/生命周期测试迁至 `tests/engine/script/*`，game 层保留桥接与 smoke 测试。
- 已完成审阅修正：`createReadOnlyProxy` 声明/定义签名统一；脚本测试 installer helper 抽到 `tests/game/script_test_utils.h`。
- 已完成脚本定向回归：`ctest --test-dir build/debug --output-on-failure -R "ScriptHost|script_host"`（`10/10` 通过）
- 已完成 debug 全量回归：`ctest --test-dir build/debug --output-on-failure`（`288/288` 通过）
- 已完成 noscript 全量回归：`ctest --test-dir build/noscript --output-on-failure`（`278/278` 通过）

## 当前实现分析
### 1) 运行时核心能力位于 game 层
- `ScriptHost` 生命周期与安全逻辑在 `src/game/script/script_host.cpp`，包含：
- 标准库白名单开启与危险全局封堵（`dofile/loadfile/load/rawset/rawget`）。
- 指令上限保护（`lua_sethook`）。
- 句柄代际校验（`scene_token + registry.valid`）。
- 这部分本质是“脚本宿主内核能力”，并不属于 TinyFarm 玩法语义。

### 2) 通用宿主与业务绑定耦合紧密
- `src/game/script/script_bindings.cpp` 同时承担：
- 脚本表结构控制（只读代理表）。
- TinyFarm 业务 API 绑定（`game::data::GameTime`、`game::defs::*Command`、`Dialogue*Event`、`PlayerTag`）。
- 结果是宿主层与玩法层边界不清晰，扩展新脚本域（例如非游戏逻辑工具脚本）会被迫依赖 game 模块。

### 3) 装配与清理路径耦合在 GameScene
- 脚本宿主初始化在 `src/game/runtime/game_runtime_assembler.cpp` 的 `tryInitScriptHost(...)`，并硬编码 bootstrap 路径 `scripts/bootstrap.lua`。
- 脚本上下文回收与脚本相关队列清理在 `src/game/scene/game_scene.cpp::clean()`。
- 这导致“脚本功能开关/生命周期”由具体场景强持有，不利于抽象为可复用引擎能力。

### 4) 构建分层与目标链接不理想
- 顶层 CMake 将 Lua/Sol2 仅链接给 `game` 目标（`CMakeLists.txt`）。
- 脚本源文件也仅编译进 `game`（`src/CMakeLists.txt`）。
- 这使 engine 层无法声明或复用脚本宿主能力，形成反向分层。

### 5) 测试分层混杂
- 当前脚本测试均位于 `tests/game/*`，其中一部分其实是宿主通用契约测试（安全边界、句柄生命周期、指令上限）。
- 缺少 engine 级脚本宿主契约测试入口，后续重构容易把通用能力变更误伤为“游戏逻辑问题”。

## 实现思路（最优方案）
### 方案结论
- 采用“**Engine Script Core + Game Script Module**”双层结构：
- `engine` 层只保留脚本运行时内核与扩展点。
- `game` 层通过模块注册方式注入 TinyFarm 的 `tf.*` 业务 API。

### 分层边界
1. `engine::script`（内核层）
- 负责 Lua VM 生命周期、安全策略、执行保护、句柄与校验、脚本加载执行、模块注册机制。
- 不直接依赖 `game/*` 头文件和任何玩法事件/命令。

2. `game::script`（扩展层）
- 负责 `tf.*` 业务 API 绑定与 TinyFarm 语义转换（命令/事件/默认玩家解析）。
- 只通过 engine 提供的扩展接口接入，不反向改写 engine 内核逻辑。

### 已决策事项
- 脚本宿主维持 **Scene 级实例**（与 `Scene` 持有 `registry` 的 ECS 契约一致），本次不提升到 `engine::core::Context` 全局服务。

### ScriptModuleInstaller 接口草案（冻结）
```cpp
// engine/script/script_module.h
namespace engine::script {
class ScriptHost;
using ScriptModuleInstaller = std::function<
    void(sol::state&, ScriptHost&, entt::registry&, entt::dispatcher&)
>;
} // namespace engine::script
```

### 关键设计点
1. 引擎层抽象脚本模块扩展点（函数式）
- 使用上面的 `ScriptModuleInstaller` 固定签名，`ScriptHost::init(...)` 时统一安装。
- Host 仅感知安装器列表，不感知 TinyFarm 业务细节。

2. Host 依赖收敛策略
- `ScriptHost` 构造仅持有 `entt::registry&`（用于 `makeHandle/validateHandle`）。
- `entt::dispatcher&` 不作为 Host 成员，改为 `init(...)` 安装模块阶段显式传入。
- 目标：减少 Host 长生命周期依赖面，避免把业务事件总线耦合进宿主内核。

3. Host 与绑定解耦（并限定抽取边界）
- 将只读命名空间装配工具放入 engine 可复用位置。
- **首批迁移到 `engine/script_binding_utils` 的仅有**：
- `createReadOnlyProxy(sol::state&, sol::table, std::string_view)`
- **留在 game 扩展模块的仍是**：
- `resolveTargetEntity(...)`、`sanitizeChannel(...)`、默认玩家解析与所有 `game::defs::*` 命令/事件转换。

4. 构建目标下沉
- `ENABLE_SCRIPTING` 开启时，Lua/Sol2 改为链接到 `engine`（`PUBLIC`），`game` 通过 `engine` 继承编译/链接要求。
- 脚本核心源码编译进 `engine`；TinyFarm 扩展源码编译进 `game`。

5. 测试分层重排
- 宿主内核契约测试迁移到 `tests/engine/script/*`。
- TinyFarm 扩展绑定与 command/event 桥接测试保留在 `tests/game/*`。

## 需要新增的文件
- `src/engine/script/script_host.h`
- `src/engine/script/script_host.cpp`
- `src/engine/script/script_entity_handle.h`
- `src/engine/script/script_entity_handle.cpp`
- `src/engine/script/script_module.h`（模块注册接口/类型别名）
- `src/engine/script/script_binding_utils.h`
- `src/engine/script/script_binding_utils.cpp`
- `src/game/script/tinyfarm_script_module.h`
- `src/game/script/tinyfarm_script_module.cpp`
- `tests/engine/script/script_host_security_test.cpp`
- `tests/engine/script/script_host_lifecycle_test.cpp`

## 预计改动文件
- `CMakeLists.txt`
- `cmake/ScriptingDependencies.cmake`（通常无需修改；T10.1 时显式确认）
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/game/script/script_host.h`（迁移后删除或转发）
- `src/game/script/script_host.cpp`（迁移后删除）
- `src/game/script/script_entity_handle.h`（迁移后删除或转发）
- `src/game/script/script_entity_handle.cpp`（迁移后删除）
- `src/game/script/script_bindings.h`（迁移为 tinyfarm 模块后删除或重命名）
- `src/game/script/script_bindings.cpp`（迁移为 tinyfarm 模块后删除或重命名）
- `src/game/runtime/system_bundle.h`
- `src/game/runtime/system_bundle.cpp`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/game/scene/game_scene.cpp`
- `tests/game/script_host_smoke_test.cpp`
- `tests/game/script_host_command_bridge_test.cpp`
- `tests/game/script_host_security_boundary_test.cpp`
- `tests/game/script_host_handle_lifecycle_test.cpp`
- `docs/overview.md`

## 实现步骤
1. 迁移脚本宿主核心到 `engine::script`。  
说明：将 `ScriptHost`、`ScriptEntityHandle` 与安全/生命周期逻辑整体下沉到 `src/engine/script`，保持行为一致；同步迁移 `g_next_scene_token`（避免重复定义或代际逻辑丢失）。

2. 增加脚本模块安装抽象。  
说明：新增 `script_module.h` 并落地固定签名 `ScriptModuleInstaller`，让 T3/T5 有统一契约。

3. 收敛 Host 依赖面。  
说明：`ScriptHost` 构造参数改为仅 `registry`；`dispatcher` 改为 `init(...)` 阶段输入，用于安装模块时传递给业务绑定。

4. 抽取绑定工具到引擎层。  
说明：仅抽取 `createReadOnlyProxy(...)`，其余业务解析逻辑留在 game 模块，避免过度抽象。

5. 迁移 TinyFarm 扩展模块。  
说明：新增 `tinyfarm_script_module`，复用现有 `script_bindings.cpp` 主体逻辑（以迁移+签名适配为主，不重写业务语义）。

6. 改造 runtime 装配流程。  
说明：`GameRuntimeAssembler` 创建 `engine::script::ScriptHost`，注册 TinyFarm 模块后再 `init()`，bootstrap 保持软失败策略。

7. 收敛场景清理职责。  
说明：`GameScene::clean()` 继续负责 Scene 级 shutdown；将“脚本事件队列清理”显式标注为 TinyFarm 模块配套策略。

8. 调整 CMake 分层。  
说明：`engine` 在脚本开关开启时链接 Lua/Sol2 并编译脚本核心源码；`game` 仅编译扩展模块源码；同步确认 `cmake/ScriptingDependencies.cmake` 是否无需变更。

9. 重排测试归属并补齐回归。  
说明：将通用契约测试迁至 `tests/engine/script`，保留并更新 game 桥接测试；在 `tests/CMakeLists.txt` 显式注册新增 engine script 测试源。

10. 执行脚本开关 ON 回归。  
说明：执行 `ctest --test-dir build --output-on-failure -j4`，重点验证宿主安全边界与命令桥接链路无回退。

11. 更新文档（低优先级，不阻塞主线实现）。  
说明：修订 `docs/overview.md` 的目录职责与脚本分层描述，可在主代码稳定后、合入前完成。

## 待办清单（用于追踪）
- [x] T1 下沉 `ScriptHost` 到 `src/engine/script` 并迁移 `g_next_scene_token`
- [x] T2 下沉 `ScriptEntityHandle` 到 `src/engine/script`
- [x] T3 新增 `script_module.h` 并固定 `ScriptModuleInstaller` 签名
- [x] T4 `ScriptHost` 构造改为仅持有 `registry`，`dispatcher` 改为 `init(...)` 输入
- [x] T5 抽取 `createReadOnlyProxy(...)` 到 `engine/script_binding_utils`
- [x] T6 新增 `game/script/tinyfarm_script_module.*` 并完成 `tf.*` 绑定迁移（迁移+适配）
- [x] T7 `GameRuntimeServices` 改为持有 `engine::script::ScriptHost`
- [x] T8 `GameRuntimeAssembler` 改为“创建 Host -> 注册 TinyFarm 模块 -> init -> bootstrap”
- [x] T9 `GameScene::clean()` 更新为新类型并保留脚本事件清理
- [x] T10 `CMakeLists.txt`/`src/CMakeLists.txt` 调整脚本源码归属与链接层级到 `engine`
- [x] T10.1 确认 `cmake/ScriptingDependencies.cmake` 是否无需改动
- [x] T11 拆分并更新测试：`tests/engine/script/*` + `tests/game/*`，并在 `tests/CMakeLists.txt` 注册
- [x] T12 执行 `ctest --test-dir build --output-on-failure -j4`（脚本开关 ON）
- [x] T13 更新 `docs/overview.md` 分层文档（低优先级，合入前完成）

## 验收标准（DoD）
- `src/engine` 内存在可独立复用的脚本宿主核心实现，且不依赖 `game/*`。
- `src/game` 仅保留 TinyFarm 业务扩展模块，不再承载通用宿主逻辑。
- `ENABLE_SCRIPTING=ON` 时功能不回退：脚本加载、错误软失败、句柄校验、安全边界、指令上限均保持。
- 现有脚本链路保持可用（至少覆盖 `test_command.lua` 与 `scripts/bootstrap.lua`）。
- 脚本测试分层清晰：宿主契约在 engine 测试，业务桥接在 game 测试。

## 风险与缓解
- 风险：迁移过程中 include 路径和命名空间改动面大，易出现编译级连锁错误。  
缓解：先迁移类型定义与实现，再逐步迁移调用点，最后统一改测试；使用 `rg -n "game/script/script_host|game/script/script_entity_handle|game/script/script_bindings" src tests` 做全局扫尾。

- 风险：模块注册抽象设计不当会引入过度复杂性。  
缓解：第一版只实现最小扩展接口（单层 installer 列表），避免提前引入多级生命周期框架。

- 风险：CMake 目标切换后，测试目标可能遗漏链接依赖。  
缓解：先确保 `engine` 对 Lua/Sol2 使用 `PUBLIC` 链接，再逐个修复 `game_tests` 编译。

## 结论
- 该方案已实施完成：`Scene 级 ScriptHost + Engine Core / Game Module` 分层落地，测试回归通过，无阻塞项。
