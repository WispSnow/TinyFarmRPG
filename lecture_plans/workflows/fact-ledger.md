# TinyFarmRPG 课程事实账本

> 用途：记录会被多讲复用、容易随代码变化漂移、或涉及跨讲承诺的事实。每个阶段开始前先读本文件；阶段 2、阶段 3、阶段 4 如产生新事实，应更新本文件。

## 共享事实

| 事实项 | 当前口径 | 证据入口 | 影响讲次 | 最后校验 | 备注 |
| --- | --- | --- | --- | --- | --- |
| 上一期 TinyFarm 规模 | 接近 5 万行级别 | 对 `lecture_plans/ref/TinyFarm/src` 与 `lecture_plans/ref/TinyFarm/tests` 下 C++ 文件做 `wc -l`，结果为 48132 行 | L00 / L01 | 2026-05-30 | 统计口径为 C++ 源码与测试行数，含空行与注释 |
| TinyFarmRPG 技术栈版本 | C++20、RmlUi 6.2、Lua 5.4.8、Sol2 3.5.0、Effekseer 1.7.3.0、FreeType 2.14.1、HarfBuzz 12.1.0 等 | `docs/overview.md` 技术栈；`external/` 目录；`cmake/*Dependencies.cmake` | L00 / L03 / L07 / L22 / L23 | 2026-05-30 | L00 技术栈速览以 `docs/overview.md` 为主口径 |
| RmlUi 资源加载边界 | RmlUi 字体由 `GameApp::initRmlUi()` 直接调用 `Rml::LoadFontFace` 注册字体文件；ImGui 也加载 `assets/fonts/VonwaonBitmap-16px.ttf`。RmlUi 图片由 `RenderInterface_GL3_STB::LoadTexture()` 通过 RmlUi `FileInterface` 读文件并用 `stb_image` 解码，或通过 `generated://` 查询 `RmlGeneratedImageRegistry`；不共享 `ResourceManager / TextureManager` 的纹理缓存 | `src/engine/core/game_app.cpp`；`src/engine/render/opengl/imgui_layer.cpp`；`src/engine/ui/rmlui/render_interface_gl3_stb.cpp`；`src/engine/ui/rmlui/rml_generated_image_registry.cpp` | L03 / L14 / L22 | 2026-05-30 | L03 讲义应写成“共享字体文件 / stb 解码栈”，避免写成“共享 ResourceManager 缓存” |
| Scene 栈与 RmlUi 可见性调度 | `SceneManager::update()` / `fixedUpdate()` 只调用栈顶 Scene；`prepareUi()` / `render()` 遍历整栈，栈顶使用当前 interpolation alpha，底层覆盖场景使用 `1.0f` 保持冻结快照；`syncRmlActiveScene()` 从栈顶向下找第一个 `HideUnderlyingSceneUi`，并把该 Scene 及其上方 Scene 的 owner 传给 `RmlUiRuntime::setVisibleSceneOwners()` | `src/engine/scene/scene_manager.cpp`；`src/engine/ui/rmlui/rml_ui_runtime.cpp`；`tests/engine/scene/render_interpolation_pipeline_test.cpp` | L04 / L05 / L15 / L25 | 2026-05-30 | L04 讲义需避免写成 `prepareUi()` 仅栈顶；`GameMode` 是否驱动底层 scheduler 另在 L25 复核 |
| 输入上下文与战斗缓冲输入 | `Gameplay` 上下文允许移动/交互/背包/快捷栏等动作但不允许 `menu_*`；`Menu` 允许 `menu_*` 与 inventory/tab shortcuts；`Dialogue` / `Battle` 只允许 `menu_*`。`BattleInputRouter` 监听 `menu_up/down/left/right/confirm/cancel`，方向键负责 repeat，`menu_confirm` / `menu_cancel` 会在菜单状态非 `None` 时消费 150ms 内的 buffered press；直接成功处理的 confirm/cancel 会清空同窗口 buffer，避免同一次按键重复触发 | `src/engine/input/input_context_registry.cpp`；`src/game/scene/battle_input_router.cpp`；`tests/engine/input/input_context_test.cpp`；`tests/game/battle/battle_input_router_test.cpp` | L05 / L18 | 2026-05-30 | L05 讲义需同步上下文表、调试面板观察与 buffer 真实生产消费路径；L18 深讲战斗菜单时复用该口径 |
| Lua bootstrap 与读档状态时序 | `RuntimeServiceFactory` 只初始化 `ScriptHost` 并安装 `tf.*`；`GameScene::init()` 在读档 `SaveService::loadFromFile()` / 新游戏默认状态应用之后再执行 `scripts/bootstrap.lua`。暂停菜单在同一 `GameScene` 内读档成功后调用 `ScriptHost::reload()`，清掉旧回调和模块缓存并重新执行 bootstrap。因此 bootstrap 顶层读取到的是当前玩家、地图和已恢复的 `ScriptStateStore`。新游戏初始 300 金由 C++ 新游戏初始化写入 `PlayerWalletComponent`，不再由 Lua `tf.player.set_gold` 写入；`tf.player` 保留玩家查询和 `gold()` 读取 | `src/game/runtime/script_runtime_factory.cpp`；`src/game/scene/game_scene.cpp`；`src/game/scene/pause_menu_scene.cpp`；`src/game/script/tinyfarm_script_module.cpp`；`scripts/bootstrap.lua`；`tests/game/game_scene_runtime_assembly_test.cpp`；`tests/game/pause_menu_scene_async_save_ui_test.cpp`；`tests/game/script_module_require_test.cpp` | L06 / L07 / L21 | 2026-05-30 | L06 已完成并同步讲义、核心阅读文档和课程大纲；L07 已做 `tf.player` 示例的最小跨讲同步，完整绑定讲解待 L07 阶段审阅；L21 可复用 `script_state` 读档先于 bootstrap 的口径 |
| 上一期 TinyFarm `GameScene` system 数量 | 三十多个 system | `lecture_plans/ref/TinyFarm/src/game/scene/game_scene.h/.cpp` 中 system 字段与 `std::make_unique<...System>` 计数均为 33 | L01 | 2026-05-30 | 用于替代过时的“28 个 system”表述 |
| TinyFarmRPG `GameSystemBundle` system / bridge 数量 | 40+ system / bridge 实例 | `src/game/runtime/system_bundle.h` 中 `GameSystemBundle` 的 `std::unique_ptr` 字段计数为 45 | L01 / L25 | 2026-05-30 | 含 debug-only 字段和可选 `ScriptEventBridge` / `VfxBridgeSystem` |
| `RuntimeServiceFactory::assemble` 失败硬停点 | 当前有 13 处 `return false` | `src/game/runtime/runtime_service_factory.cpp` 中 `RuntimeServiceFactory::assemble` 函数体计数 | L01 | 2026-05-30 | 讲义正文不写死数量，只引导学生观察前置失败点 |
| `SaveService` 与背包事件边界 | `SaveService` 不订阅 `InventoryChanged`；保存时 capture 当前组件，读档 apply 后触发 `InventorySyncCommand` / `HotbarSyncCommand` / `HotbarActivateCommand` 做 UI 同步 | `rg "sink<game::defs::InventoryChanged|InventoryChanged" src/game`；`src/game/save/save_service.cpp` 中读档同步命令 | L02 / L21 | 2026-05-30 | L02 讲义避免写成“存档订阅 InventoryChanged” |

## 代码片段锚点

| 讲次 | 片段主题 | 源文件 | 符号 / 范围 | 片段策略 | 最后校验 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| L00 | 极薄入口调用 `game::run()` | `src/main.cpp` | `main()` | 完整 | 2026-05-30 | L00 源码入口表提到该调用 |
| L00 | 启动逻辑入口 | `src/game/game_entry.cpp` | `game::run()` / `setupInitialScene()` | 节选 | 2026-05-30 | L00 只要求建立入口直觉，不展开 `GameApp` |
| L01 | `GameScene::init()` 委托装配 | `src/game/scene/game_scene.cpp` | `GameScene::init()` | 节选 | 2026-05-30 | 讲义只保留 `assembleServices` / `assembleSystems` 核心片段 |
| L01 | 内容路径集中常量 | `src/game/runtime/game_content_manifest.h` | `GameContentManifest` | 节选 | 2026-05-30 | 讲义省略部分字段，并用“等等”说明 |
| L01 | catalog 指针注入 | `src/game/runtime/runtime_service_factory.cpp` | `injectCatalogPointers()` | 完整 | 2026-05-30 | 当前注入 `RpgCatalog*` / `QuestCatalog*` / `ShopCatalog*` |
| L01 | 本地化服务查找 helper | `src/game/runtime/service_lookup.h` | `findLocalizationService()` | 核心形态 | 2026-05-30 | 讲义省略 Doxygen 与命名空间 |
| L02 | 背包领域服务统一写入 | `src/game/domain/inventory_domain_service.cpp` | `InventoryDomainService::addItem()` / `removeItem()` / `moveItem()` / `sortInventory()` | 节选 | 2026-05-30 | `addItem()` 先校验 `ItemCatalog`；move/sort 写入已从 `InventorySystem` 收敛到 domain |
| L02 | 背包 system 薄壳转发 | `src/game/system/inventory_system.cpp` | `onAddItem()` / `onRemoveItem()` / `onMoveItem()` / `onSort()` | 核心形态 | 2026-05-30 | system 只处理 command 到 domain service 的转发；`InventorySyncCommand` 仍只发 full sync 事件 |
| L02 | 背包排序快捷栏 remap | `src/game/defs/events_inventory.h` / `src/game/system/hotbar_system.cpp` | `InventoryChanged::slot_remap_old_to_new` / `HotbarSystem::onInventoryChanged()` | 节选 | 2026-05-30 | 排序事件由 domain 发 full sync + old-to-new 映射，HotbarSystem 订阅后更新快捷栏映射 |
| L03 | RmlUi runtime 文档生命周期 | `src/engine/ui/rmlui/rml_ui_runtime.cpp` | `loadDocument()` / `reloadDocument()` / `unloadDocument()` / `unloadDocumentsByOwner()` | 节选 | 2026-05-30 | `reloadDocument()` 先加载替换文档，成功后继承 owner / 可见性 / 输入模式 / 字号 class，再关闭旧文档；失败时保留旧文档 |
| L03 | RmlUi Debug Panel 文档调试入口 | `src/engine/debug/panels/rmlui_debug_panel.cpp` | `drawFileBrowser()` / `drawDebugDocuments()` / `loadDocument()` / `reloadDebugDocument()` | 核心形态 | 2026-05-30 | Debug Panel 可加载、隐藏、Reload、卸载 `ui/rmlui` 下的调试文档；Reload 使用 runtime 的安全 reload 接口并更新面板持有的 document 指针 |
| L04 | Scene 栈调度与 RmlUi owner 可见性 | `src/engine/scene/scene_manager.cpp` | `update()` / `fixedUpdate()` / `prepareUi()` / `render()` / `syncRmlActiveScene()` | 核心形态 | 2026-05-30 | `prepareUi()` / `render()` 遍历全栈；底层 Scene 用 `1.0f` alpha；可见 owner 列表由第一个 `HideUnderlyingSceneUi` 决定 |
| L04 | GameScene HUD 组合入口 | `src/game/scene/game_scene.cpp` / `src/game/ui/game_scene_ui_controller.cpp` | `GameScene::initUI()` / `GameSceneUiController::init()` | 核心形态 | 2026-05-30 | `GameScene` 直接持有 `GameSceneUiController`、`GameOverlay`、`GameInputPromptOverlay`；`GameSceneUiController` 持有 hotbar、dialogue、floating notice、tooltip、time clock、screen fade |
| L04 | 覆盖式 Scene 输入与可见性策略 | `src/game/scene/*_scene.cpp` | `init()` / `clean()` / `uiCoverage()` | 核心形态 | 2026-05-30 | Pause / Inventory / SaveSlot / Rest 使用默认 Overlay；Shop / QuestOffer / RecruitOffer / DialogueChoice / AppearanceCustomize / Battle 返回 `HideUnderlyingSceneUi`；相关源码测试已锁定 |
| L05 | 输入上下文白名单与战斗菜单输入路由 | `src/engine/input/input_context_registry.cpp` / `src/game/scene/battle_input_router.cpp` | `buildInputContextDefinitions()` / `BattleInputRouter::update()` / `dispatchBufferedMenuAction()` | 核心形态 | 2026-05-30 | `BattleInputRouter` 的 confirm/cancel buffer 窗口为 150ms；buffer 只在菜单状态非 `None` 时回放，成功处理后清空同窗口 buffered press |
| L06 | Lua bootstrap 执行时机 | `src/game/runtime/script_runtime_factory.cpp` / `src/game/scene/game_scene.cpp` / `src/game/scene/pause_menu_scene.cpp` | `ScriptRuntimeFactory::tryInitScriptHost()` / `tryLoadBootstrapScript()` / `GameScene::init()` / `PauseMenuScene::onLoadClicked()` | 核心形态 | 2026-05-30 | Host 初始化与 bootstrap 执行已拆分；读档 apply 和新游戏默认钱包写入先于 `tryLoadBootstrapScript()`；暂停菜单读档成功后 `ScriptHost::reload()` |
| L06 | `tf.state` 与 `lib.once` 持久状态 | `src/game/script/script_state.h` / `src/game/script/tinyfarm_script_module.cpp` / `scripts/lib/state.lua` / `scripts/lib/once.lua` | `ScriptStateStore` / `tf.state` 注册块 / `once.run()` | 核心形态 | 2026-05-30 | `ScriptStateStore` 保存 `nullptr_t / bool / double / string`；`once.run()` 先 mark 再执行 fn，保持 at-most-once 语义 |
| L06 | Lua 内容层玩家金币边界 | `src/game/script/tinyfarm_script_module.cpp` / `src/game/scene/game_scene.cpp` / `scripts/bootstrap.lua` | `tf.player` 注册块 / `initializeNewGameWallet()` / bootstrap 顶层 require 列表 | 核心形态 | 2026-05-30 | `tf.player` 不再暴露 `set_gold/add_gold`；bootstrap 不直接改钱包；新游戏初始 300 金由 C++ 写入 `PlayerWalletComponent` |

## 跨讲承诺

| 来源讲次 | 承诺 / 引用 | 目标讲次 | 兑现锚点 | 状态 | 备注 |
| --- | --- | --- | --- | --- | --- |
| L00 | 下一讲展开 `GameRuntimeAssembler`、`RuntimeServiceFactory`、`SystemFactory` 声明式装配 | L01 | L01 关键链路与“装配模式”小节 | 已兑现 | L01 已覆盖三个角色与 `GameSystemBundle` |
| L00 | RmlUi 基础为前置必修，主线 L03 只讲项目接入 | L03 | L03 开头说明只回答“为什么要换、接入点在哪里”；阅读清单外链 `learn/lectures/rmlui/syllabus.md`；正文不展开 RML/RCSS 语法基础 | 已兑现 | L03 聚焦 runtime / controller / data bridge / render interface / 资源与 Debug Panel 接入 |
| L00 | RmlUi L07-L15 与 L04 / L18 等讲次穿插关联 | L04 / L18 | L04 阅读清单关联 RmlUi L09 spritesheet；L18 待复核 | 部分兑现 | 与大纲先修分布保持一致 |
| L00 | 多线程子教程在 L21 / L24 / L25 指明具体章节 | L21 / L24 / L25 | 待填写 | 待查 | 后续工程化讲次修订时确认 |
| L01 | domain service 的 preflight / 一致写入 / 反馈事件详深留到 L02 与 L10 | L02 / L10 | L02 “preflight → 一致写入 → event”小节；L10 待任务交付深讲 | 部分兑现 | L02 已建立领域服务共同模式；L10 继续以任务交付展开 |
| L01 | Lua 内容层与 C++ 绑定详深留到 L06-L08 | L06 / L07 / L08 | L06 已完成：bootstrap 在读档 / 新游戏默认状态应用后执行；Lua 不再通过 `tf.player.set_gold` 直接写钱包；讲义已校准 `tf.*` 能力地图与 `tf.state` / `lib.once` 规约。L07/L08 待填写 | 部分兑现 | 后续确认 script 层绑定数量、事件桥 payload 与 Tiled 字段口径一致 |
| L01 | Blueprint / EntityFactory 的脚本化字段扩展留到 L08 | L08 | 待填写 | 待查 | 包含 `scripted_interaction=true` 等字段 |
| L05 | 战斗菜单多层状态、cursor memory、repeat 与 buffered confirm/cancel 的深讲留到 L18 | L18 | 待填写 | 待查 | L05 只建立为什么不用 RmlUi 原生导航和 InputBuffer 的上游背景 |

## 术语表

| 术语 | 课程统一解释 | 首次重点讲解 | 备注 |
| --- | --- | --- | --- |
| 待填写 | 待填写 | 待填写 | 待填写 |

## 全局一致性待查

| 项目 | 发现讲次 | 影响范围 | 处理状态 | 备注 |
| --- | --- | --- | --- | --- |
| `RuntimeServiceFactory::assemble()` 行数表述不一致 | L01 | L01 正文第 152 行写“约 60 行”，第 250 行写“约 80 行” | 已处理 | L01 已移除易漂移行数 |
| `28+ ECS systems` 数字疑似过时 | L01 | L01 图示和职责表；当前 `GameSystemBundle` 字段计数为 45 | 已处理 | L01 已改为“40+ system / bridge 实例” |
| `4 万行` 项目规模表述需统一 | L00 / L01 | 开篇与架构课中的规模叙述 | 已处理 | L00 / L01 均已按上一期 TinyFarm 统计结果改为“接近 5 万行级别” |
| 复合事务异常提交回滚策略需后续复查 | L02 | L10 / L11 / L13 中任务交付、商店交易、装备穿脱的 preview / commit 讲解 | 待处理 | L02 阶段 2 保留风险；后续深讲复合交易时复核失败路径与回滚口径 |
