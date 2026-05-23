# Lua 扩展前框架体检与重构计划

## 背景

当前项目已经完成 Lua/Sol2、分层外观、Effekseer、任务/商店/队伍/装备/回合制战斗等基础闭环。下一阶段计划把更多剧本式玩法从 C++ 迁到 Lua，包括对话脚本、任务推进、招募对白、商店预设、地图事件和战斗事件钩子。

这份计划综合两轮代码框架 review 的结论，目标是在大规模扩展 Lua API 前，先收紧框架边界，避免后续脚本层变成绕过领域规则的第二套玩法系统。

## 总体判断

当前框架值得保留的主干：

- `src/engine` / `src/game` 分层基本清晰，没有发现 engine 反向依赖 game。
- ECS + `SystemScheduler` + `entt::dispatcher` + domain service 的主链路是合适的。
- 数据驱动目录已经覆盖物品、RPG、任务、商店、外观、VFX、音频等核心内容。
- 测试密度高，已有脚本、战斗、调度、存档、UI 架构回归测试。
- `engine::script::ScriptHost` 已具备基础沙箱、指令上限、句柄代际校验和 `protected_function` 错误收口。

需要在 Lua 扩展前处理的核心风险：

- Lua 目前主要是“脚本主动调用 C++”，缺少 `tf.event` / `tf.callbacks` 这类 C++ 事件驱动 Lua 回调的双向通道。
- `commands.h` / `events.h` 承载过多领域类型，脚本绑定继续扩展会放大编译依赖和概念耦合。
- `GameRuntimeAssembler`、`SystemScheduler`、`BattleScene` 等文件已经承担过多职责，新增脚本扩展会进一步推高编辑成本。
- 战斗公式使用独立 Lua VM，但没有复用 `ScriptHost` 的沙箱策略。
- 如果 Lua 承载剧本变量，必须提前明确脚本状态的生命周期和存档策略。

## 设计原则

- Lua 不直接写 ECS 细节；写入仍走 domain service 或 dispatcher command。
- 脚本 API 返回稳定的 typed result，不把底层失败原因藏在 bool 后面。
- `ScriptHost` 仍建议随 `GameScene` 生命周期存在；长期剧本变量必须显式进入 SaveService，而不是依赖 Lua VM 常驻内存。
- 禁止为了脚本方便重新打开 `io/os/package/loadfile/dofile/rawset` 等能力。
- 可以不考虑旧存档兼容，但要保持新的存档 schema 可迁移、可测试。
- 优先拆会阻塞 Lua 扩展的边界；大场景文件可作为并行线逐步拆。

## Phase 0：脚本安全与基础工具收口

目标：先补齐 Lua 基础设施的一致性，避免后面 API 越扩越散。

待办：

- [ ] 抽出共享 Lua 沙箱配置，例如 `engine::script::LuaSandboxPolicy`。
- [ ] `ScriptHost` 和 `BattleFormulaEvaluator` 共用沙箱策略：禁用危险全局、禁用字节码导出、设置 instruction limit。
- [ ] 为 `BattleFormulaEvaluator` 增加无限循环/危险全局访问回归测试。
- [ ] 增加脚本 reload 的调试入口：优先接入现有 Debug UI 面板，调用 `ScriptHost::reload()`。
- [ ] 抽 `ScriptTestEnv` 测试 fixture，统一封装 `entt::registry`、`entt::dispatcher`、常用 catalog、`ScriptHost` 和 `expectLuaPasses()` 辅助。
- [ ] 将现有脚本 smoke / command bridge 测试迁到 `ScriptTestEnv`，后续 `tf.event`、`tf.state`、`tf.script.require` 测试沿用同一套 fixture。
- [ ] 梳理 `scripts/bootstrap.lua` 的运行日志策略，避免长期保留只有 `print` 的占位脚本。

验收：

- `tests/engine/script/*` 和 `tests/game/battle/battle_formula_evaluator_test.cpp` 覆盖共享沙箱行为。
- 公式 Lua VM 无法访问 `load/dofile/loadfile/rawset/rawget/io/os/package`。
- `BattleFormulaEvaluator` 应用共享沙箱后，仍保持每次求值前刷新 `a` / `b` 表的语义。
- 脚本测试新增能力不再重复手写 registry/dispatcher/ScriptHost boilerplate。
- Debug UI 可以触发脚本重载，失败时只记录错误，不影响主流程。

## Phase 1：命令/事件/领域边界整理

目标：给 Lua 绑定层提供稳定 C++ 入口，减少直接触碰零散 system 的需要。

待办：

- [x] 拆分 `src/game/defs/commands.h`：
  - `commands_inventory.h`
  - `commands_equipment.h`
  - `commands_interaction.h`
  - `commands_quest.h`
  - `commands_recruit.h`
  - `commands_battle.h`
  - `commands_appearance.h`
  - 保留聚合头 `commands.h` 作为内部过渡入口。
- [x] 拆分 `src/game/defs/events.h`，至少把 dialogue、inventory、equipment、quest、battle 事件分域。
- [x] 新建 `game::script::ScriptGameApi` 或类似 facade，Lua 绑定优先依赖 facade，不直接散落调用 registry/dispatcher/domain service。
- [ ] 对脚本需要主动调用的玩法按需补 domain/service 入口，而不是预先为空壳抽象：
  - 如果只是单个事件或命令转发，facade 直接发 command/event，例如 `DialogueShowEvent`、`DialogueHideEvent`、`RecruitPartyMemberCommand`。
  - 如果逻辑涉及多步原子写入、共享校验或跨多个调用点复用，再升级为 domain service。
  - Dialogue 先沿用现有 show/hide 事件；只有出现 script-driven dialogue session、分支选择、推进状态写回等需求时再抽 `DialogueDomainService`。
  - Recruitment 先沿用 `RecruitPartyMemberCommand` + `PartyRecruitmentSystem`；只有招募资格判定被 UI、脚本、系统多处复用时再抽 `RecruitmentDomainService`。
  - Farm/WorldInteraction 先通过 command/facade 暴露高层意图；只有需要把 per-tile 空间查询和多步写入稳定复用时再抽 service。
  - Battle 入口先保留 `EnterBattleCommand`，但放入 `commands_battle.h` 并提供脚本 facade。

验收：

- Lua 绑定文件不需要 include 大量无关领域头。
- 每个脚本可写操作都有对应 facade 入口；facade 内部要么发明确 command/event，要么调用真正有领域价值的 service。
- Inventory/Equipment/Quest/Shop 现有行为不变，相关测试继续通过。

## Phase 2：Lua 双向事件通道

目标：让 Lua 能承载真正的剧本流，而不只是被 C++ 暴露函数调用。

建议 API：

```lua
tf.event.on("interact", function(evt)
    -- evt.player, evt.target, evt.target_name, evt.map_id ...
end)

tf.callbacks.on_battle_end(function(evt)
    -- evt.outcome, evt.troop_id, evt.rewards ...
end)
```

待办：

- [x] 在 C++ 侧实现 typed event 到 Lua payload table 的统一中转层。
- [x] 先盘点 C++ 侧事件源：复用已存在的 `DayChangedEvent` / `HourChangedEvent` / `TimeOfDayChangedEvent` / `InventoryChanged` / `BattleEndedEvent`，并补齐首批缺失事件源：`BattleStartedEvent`、`ItemUsedEvent`、`QuestAcceptedEvent`、`QuestCompletedEvent`；`MapEnteredEvent` 待地图切换完成事件边界明确后接入。
- [x] 首批事件建议只做高价值、低风险集合：
  - `interact`
  - `dialogue_closed`
  - `quest_accepted`
  - `quest_completed`
  - `battle_started`
  - `battle_ended`
  - `day_changed`
  - `time_of_day_changed`
  - `map_changed`（待 `MapEnteredEvent`/地图切换完成事件明确后接入）
  - `inventory_changed`
  - `item_used`
- [x] 回调调用必须走 `sol::protected_function`，错误进入日志，不中断主循环。
- [x] 回调注册与 `ScriptHost::shutdown()` 绑定生命周期，场景销毁时全部失效。
- [x] 明确回调内可执行命令的时机：先 enqueue 到脚本命令队列，在安全阶段统一 drain，避免 dispatcher 重入。
- [x] 在 `SystemScheduler` 中先局部加入 `SchedulerStage::ScriptCommands`，推荐放在 `State` 之后、`Movement` 之前；Phase 5 声明式调度重构时再迁入 `StageDecl`。
- [x] 制定回调级指令预算策略：每次调用 Lua 回调前重置/配置 instruction budget，避免单个回调长跑拖死 VM。
- [x] 增加无限循环回调 stress 测试，验证回调被中断后不会崩溃，也不会留下半写脚本命令队列。
- [x] 增加事件回调端到端测试：Lua 注册回调，C++ 触发事件，Lua 通过 `tf.command` 或 `tf.state` 产生可验证结果。

验收：

- `bootstrap.lua` 能注册至少一个交互或日期变化回调。
- 回调内抛 Lua 错误不会崩溃，不会留下半写 ECS 状态。
- 回调内无限循环会被单回调预算中断，并且后续脚本回调仍可继续执行。
- 事件 payload 不暴露裸 `entt::entity`，只暴露 `ScriptEntityHandle` 或稳定字符串/id。

## Phase 3：Lua 模块加载与 bootstrap 组合根

目标：让 `scripts/bootstrap.lua` 成为真正的脚本组合根，而不是单文件堆逻辑。

建议目录：

```text
scripts/
├── bootstrap.lua
├── lib/
│   ├── event.lua
│   ├── quest.lua
│   ├── dialogue.lua
│   └── state.lua
├── maps/
│   └── home_exterior.lua
├── quests/
│   └── first_delivery.lua
└── npcs/
    └── lyria.lua
```

待办：

- [x] 增加白名单式 `tf.script.require("module.name")`，只允许加载 `scripts/` 下 `.lua`。
- [x] 不打开 Lua `package`；模块解析由 C++ 控制路径、缓存和错误日志。
- [x] 明确脚本生命周期：`bootstrap.lua` 在每次 `GameScene` 初始化时执行；读档会销毁旧 `GameScene`、创建新 `GameScene`、重新加载 bootstrap 并重新注册回调。
- [x] 编写脚本约定：模块顶层只做注册和幂等初始化，不假设自己是 app 生命周期内只执行一次。
- [x] `bootstrap.lua` 示例化：
  - 加载 `lib/event.lua`
  - 加载 `quests/*.lua` 或显式 require 几个样例模块
  - 注册一个 NPC 交互样例
  - 注册一个 `day_changed` 或 `battle_ended` 样例
- [x] 测试脚本 fixture 增加多文件加载与失败路径。

验收：

- 缺失模块、语法错误、运行时错误都有清晰日志。
- 同一模块重复 require 不重复注册回调，或有明确的重载清理策略。
- 脚本热重载后旧回调不会重复触发。
- 读档或重建 `GameScene` 后，脚本回调会重新注册，但不会依赖旧 Lua VM 中的临时全局变量。

## Phase 4：脚本状态与存档

目标：决定并落实剧本变量的生命周期，避免 Lua state 重建导致隐式丢状态。

推荐方案：`ScriptHost` 仍随 `GameScene` 生命周期存在，脚本状态通过 `tf.state` 显式接入 SaveService。

建议 API：

```lua
tf.state.get("quest.first_delivery.stage", 0)
tf.state.get_int("quest.first_delivery.stage", 0)
tf.state.get_string("npc.lyria.mood", "neutral")
tf.state.set("quest.first_delivery.stage", 2)
tf.state.add("npc.lyria.gift_count", 1)
```

待办：

- [x] 新增 `ScriptStateComponent` 或 SaveData 中的 `script_state` 字段，值只允许 JSON 兼容基元：null / bool / number / string。
- [x] Lua `number` 不在存档层区分 int/float；脚本侧通过类型化访问器表达期望类型，例如 `get_int`、`get_number`、`get_bool`、`get_string`。
- [x] `tf.state` 只允许简单可序列化值，不允许保存 function/table/entity handle。
- [x] SaveService 写入和读取 `script_state`。
- [x] 增加 schema 迁移测试和 roundtrip 测试。
- [x] 明确命名规范：推荐 `domain.object.field`，例如 `quest.first_delivery.stage`。

验收：

- 新游戏、读档、地图切换、战斗返回后脚本状态一致。
- GameScene 重建后，Lua 模块重新加载，但剧本变量从存档恢复。
- 无法序列化的 Lua 值会被拒绝并记录错误。
- 类型化访问器对缺失值、类型不匹配值使用默认值，并记录可诊断信息。

## Phase 5：组合根与调度器整理

目标：降低运行时装配和系统调度的维护成本，为脚本阶段插入点留出位置。

待办：

- [x] 拆 `GameRuntimeAssembler`：
  - `ContentCatalogLoader`
  - `AssetPreloadRegistrar`
  - `RuntimeServiceFactory`
  - `SystemFactory`
  - `ScriptRuntimeFactory`
- [x] 把硬编码资源路径集中到 `GameContentManifest` 或 `GameRuntimeConfig`。
- [x] `SystemScheduler` 改为声明式 `StageDecl`：
  - stage 名称
  - mode mask
  - run 函数
  - ro/rw resources
  - gate 条件
  - 是否 worker eligible
- [x] 显式处理 `Battle` / `PauseOverlay` profile：目前声明为暂停型 profile，仅保留 `RemoveEntity` 清理 stage。
- [x] 将 Phase 2 中临时硬编码的 `ScriptCommands` 阶段迁入声明式 `StageDecl`。

验收：

- profile 展示、tick 顺序、dot dump 来自同一份声明。
- 新增一个 stage 不需要同时修改多个 switch/profile/dump 函数。
- 现有 SystemScheduler 测试继续通过，并新增脚本阶段顺序测试。

## Phase 6：大文件拆分并行线

目标：不阻塞 Lua 扩展主线，但逐步降低核心场景维护成本。

优先级：

1. `battle_scene.cpp` / `battle_scene.h`
   - [x] 先拆出战斗状态/菜单状态与 RmlUi ViewModel 类型定义：`battle_scene_state.h`、`battle_scene_view_models.h`，保持 `BattleScene` 对外行为不变。
   - [x] 拆出 RmlUi ViewModel 结构体注册：`battle_scene_data_bindings.h/.cpp`，`BattleScene` 只保留数据实例绑定。
   - [x] 拆状态机：`BattleFlowController` 负责流程状态推进，`BattleScene` 通过 delegate 保留场景动作与表现层编排。
   - [x] 拆输入路由：`BattleInputRouter` 负责动作订阅和方向输入映射，`BattleScene` 只暴露菜单状态、游标移动、确认/取消。
   - [x] 拆 ViewModel 构建：`BattleViewModelBuilder` 负责行动顺序、队伍 HUD、战斗日志和 Victory 奖励/升级条目生成。
   - [x] 拆数据绑定/菜单模型：`BattleMenuModel` 负责菜单 RmlUi 绑定字段、命令列表、游标、dirty 标记与 enabled 刷新。
   - 保留 `BattleScene` 作为场景生命周期和胶水层。
2. `shop_menu_scene.cpp`
   - [x] 保留 scene 生命周期，抽 `ShopTradeListBuilder` 和 `ShopMenuTransactionPresenter`，Scene 继续负责 UI 生命周期、输入与交易协调。
3. `inventory_tab_content.cpp`
   - [x] 抽 `InventorySlotDragController` 和 `InventoryActionMenuModel`，TabContent 保留事件入口、RmlUi 定位与命令 dispatch。
4. `input_manager.cpp`
   - 拆 SDL event ingestion、binding persistence、context filter、rumble/glyph 辅助。
5. `rpg_catalog.cpp`
   - 按 classes/actors/skills/states/equipment/enemies/troops 拆 parser 文件。

验收：

- 每次拆分保持行为测试通过。
- 不在拆分过程中重写玩法规则。
- Scene 文件只保留生命周期、输入入口和少量协调逻辑。

## 推荐推进顺序

1. Phase 0：共享 Lua 沙箱 + 公式 VM 收口。
2. Phase 1：拆命令/事件头，补脚本可调用的 domain/facade。
3. Phase 2：实现 `tf.event` / `tf.callbacks` 双向通道。
4. Phase 3：搭起 `bootstrap.lua` + Lua lib + 模块加载。
5. Phase 4：实现 `tf.state` 和 SaveService roundtrip。
6. Phase 5：拆 RuntimeAssembler / 声明式 Scheduler。
7. Phase 6：并行拆 BattleScene 等大文件。

## 首个建议任务切片

为了降低第一步风险，建议从这个小切片开始：

- [ ] 新建共享 Lua 沙箱 helper。
- [ ] `ScriptHost` 改用 helper，保持现有测试通过。
- [ ] `BattleFormulaEvaluator` 改用 helper，并补危险全局/无限循环测试；同时确认公式求值测试仍覆盖 `a` / `b` 表刷新语义。
- [ ] 新增一个 Debug UI reload 脚本按钮或命令入口。
- [ ] 在 `bootstrap.lua` 中保留最小结构：

```lua
-- TinyFarm script bootstrap
-- 入口由 GameRuntimeAssembler 在 GameScene 初始化时加载。
-- 后续阶段会经由 tf.script.require 加载 lib/event、quests、npcs 等模块。
-- 注意：每次 GameScene 重建（包括读档）都会重新执行本文件。

print("[tf] bootstrap loaded")
```

这个切片不改变玩法规则，但能先统一脚本安全边界，并为后续 Lua 扩展打下可测基础。
