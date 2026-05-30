# TinyFarmRPG 课程事实账本

> 用途：记录会被多讲复用、容易随代码变化漂移、或涉及跨讲承诺的事实。每个阶段开始前先读本文件；阶段 2、阶段 3、阶段 4 如产生新事实，应更新本文件。

## 共享事实

| 事实项 | 当前口径 | 证据入口 | 影响讲次 | 最后校验 | 备注 |
| --- | --- | --- | --- | --- | --- |
| 上一期 TinyFarm 规模 | 接近 5 万行级别 | 对 `lecture_plans/ref/TinyFarm/src` 与 `lecture_plans/ref/TinyFarm/tests` 下 C++ 文件做 `wc -l`，结果为 48132 行 | L00 / L01 | 2026-05-30 | 统计口径为 C++ 源码与测试行数，含空行与注释 |
| TinyFarmRPG 技术栈版本 | C++20、RmlUi 6.2、Lua 5.4.8、Sol2 3.5.0、Effekseer 1.7.3.0、FreeType 2.14.1、HarfBuzz 12.1.0 等 | `docs/overview.md` 技术栈；`external/` 目录；`cmake/*Dependencies.cmake` | L00 / L03 / L07 / L22 / L23 | 2026-05-30 | L00 技术栈速览以 `docs/overview.md` 为主口径 |
| 上一期 TinyFarm `GameScene` system 数量 | 三十多个 system | `lecture_plans/ref/TinyFarm/src/game/scene/game_scene.h/.cpp` 中 system 字段与 `std::make_unique<...System>` 计数均为 33 | L01 | 2026-05-30 | 用于替代过时的“28 个 system”表述 |
| TinyFarmRPG `GameSystemBundle` system / bridge 数量 | 40+ system / bridge 实例 | `src/game/runtime/system_bundle.h` 中 `GameSystemBundle` 的 `std::unique_ptr` 字段计数为 45 | L01 / L25 | 2026-05-30 | 含 debug-only 字段和可选 `ScriptEventBridge` / `VfxBridgeSystem` |
| `RuntimeServiceFactory::assemble` 失败硬停点 | 当前有 13 处 `return false` | `src/game/runtime/runtime_service_factory.cpp` 中 `RuntimeServiceFactory::assemble` 函数体计数 | L01 | 2026-05-30 | 讲义正文不写死数量，只引导学生观察前置失败点 |

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

## 跨讲承诺

| 来源讲次 | 承诺 / 引用 | 目标讲次 | 兑现锚点 | 状态 | 备注 |
| --- | --- | --- | --- | --- | --- |
| L00 | 下一讲展开 `GameRuntimeAssembler`、`RuntimeServiceFactory`、`SystemFactory` 声明式装配 | L01 | L01 关键链路与“装配模式”小节 | 已兑现 | L01 已覆盖三个角色与 `GameSystemBundle` |
| L00 | RmlUi 基础为前置必修，主线 L03 只讲项目接入 | L03 | 待填写 | 待查 | L03 修订时确认不重复基础语法 |
| L00 | RmlUi L07-L15 与 L04 / L18 等讲次穿插关联 | L04 / L18 | 待填写 | 待查 | 与大纲先修分布保持一致 |
| L00 | 多线程子教程在 L21 / L24 / L25 指明具体章节 | L21 / L24 / L25 | 待填写 | 待查 | 后续工程化讲次修订时确认 |
| L01 | domain service 的 preflight / 原子写入 / 反馈事件详深留到 L02 与 L10 | L02 / L10 | 待填写 | 待查 | L02 建立模式，L10 以任务交付深讲 |
| L01 | Lua 内容层与 C++ 绑定详深留到 L06-L08 | L06 / L07 / L08 | 待填写 | 待查 | 后续确认 script 层边界一致 |
| L01 | Blueprint / EntityFactory 的脚本化字段扩展留到 L08 | L08 | 待填写 | 待查 | 包含 `scripted_interaction=true` 等字段 |

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
