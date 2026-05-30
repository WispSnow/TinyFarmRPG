# TinyFarmRPG 课程审阅进度

> 用途：跨会话追踪每一讲的阶段状态、用户确认、验证范围和残留 backlog。L00-L01 已按旧的一次性流程完成；L02 起按四阶段流程推进。

| 讲次 | 文件 | 当前阶段 | 状态 | 最近处理 | 代码状态 | 文本状态 | 验证 | Backlog |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| L00 | `lecture_plans/lectures/00-开篇.md` | 完成 | 旧流程完成 | 2026-05-30 | 无代码改动 | 已校准项目规模、技术栈版本、源码入口和自测提示 | 路径检查通过；源码入口存在；Mermaid 人工检查（`mmdc` 未安装） | 无 |
| L01 | `lecture_plans/lectures/01-游戏架构设计.md` | 完成 | 旧流程完成 | 2026-05-30 | 无代码改动 | 已校准 TinyFarm 规模、system 数量、装配顺序、ScriptHost/domain service 时序、ServiceLookup 口径和练习说明 | 路径检查通过；关键源码片段已核对；Mermaid 人工检查（`mmdc` 未安装）；`git diff --check` 通过 | 无 |
| L02 | `lecture_plans/lectures/02-领域服务与命令事件边界.md` | 完成 | 已完成 | 2026-05-30 | 已收紧 `InventoryDomainService::addItem()` catalog preflight；move/sort 写入收敛到 domain；`InventorySystem` 薄壳化 | 已校准 command / event 示例、`tf.command.add_item`、`InventoryChanged` 订阅者、SaveService 边界、`moveItem / sortInventory` 与排序 remap；同步更新 `docs/game/domain-services.md` | `ninja -C build/debug game_tests` 通过；L02 相关 76 项 CTest 通过；Markdown 相对链接检查通过；源码入口存在；已知过时表述 `rg` 检查通过；Mermaid 人工结构检查通过（`mmdc` 未安装）；`git diff --check` 通过 | 复合事务异常提交回滚策略已记入事实账本，后续 L10 / L11 / L13 复查 |
| L03 | `lecture_plans/lectures/03-RmlUi接入.md` | 完成 | 已完成 | 2026-05-30 | 已新增 `RmlUiRuntime::reloadDocument()`，失败保留旧文档；`reloadLastDocument()` 改走安全 reload；RmlUi Debug Panel 已支持对调试文档 Reload 并更新 document 指针；新增源码回归测试锁定 reload 顺序和 Debug Panel 入口；用户手动测试成功 | 已校准调试文档 Reload 与生产 `RmlDocumentController` 生命周期边界；资源段改为共享字体文件 / `stb_image` 解码栈、不共享 `ResourceManager / TextureManager` 纹理缓存；动态图片前缀修正为 `generated://`；补充鼠标优先输入口径、RmlUi Debug Panel 观察入口、阅读清单、源码入口、自测题和最小练习；同步更新 `docs/engine/ui_framework.md` 调试 reload 说明；未新增 Mermaid 图 | `ninja -C build/debug game_tests engine_tests` 通过（no work to do）；RmlUi / 输入路由 / UI 布局相关 CTest 53/53 通过（2 项布局集成因环境跳过）；Markdown 相对链接检查通过；源码入口、阅读清单、练习路径和 `tools/rmlui_tester` 存在；过时表述 `rg` 扫描通过；Mermaid 人工结构检查通过（`mmdc` 未安装）；RML/RCSS 坑点 `rg` 扫描通过；`git diff --check` 通过 | L14/L22 复用 `generated://` 与字体加载边界 |
| L04 | `lecture_plans/lectures/04-HUD与覆盖式场景.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L05 | `lecture_plans/lectures/05-输入上下文与菜单导航.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L06 | `lecture_plans/lectures/06-Lua内容层总览.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L07 | `lecture_plans/lectures/07-ScriptHost与Sol2绑定.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L08 | `lecture_plans/lectures/08-脚本事件桥与Tiled接入.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L09 | `lecture_plans/lectures/09-数据目录与RPG-Catalog.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L10 | `lecture_plans/lectures/10-任务系统.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L11 | `lecture_plans/lectures/11-商店系统.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L12 | `lecture_plans/lectures/12-队伍与招募.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L13 | `lecture_plans/lectures/13-装备成长与休息.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L14 | `lecture_plans/lectures/14-分层角色外观与头像.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L15 | `lecture_plans/lectures/15-探索与战斗的过渡.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L16 | `lecture_plans/lectures/16-回合制战斗领域核心.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L17 | `lecture_plans/lectures/17-战斗动作解析.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L18 | `lecture_plans/lectures/18-战斗Action生成.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L19 | `lecture_plans/lectures/19-战斗表现与动画导演.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L20 | `lecture_plans/lectures/20-战斗结算与探索态写回.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L21 | `lecture_plans/lectures/21-存档系统与Schema迁移.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L22 | `lecture_plans/lectures/22-本地化与用户设置.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L23 | `lecture_plans/lectures/23-Effekseer与VFX管线.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L24 | `lecture_plans/lectures/24-异步地图预加载与主线程命令队列.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L25 | `lecture_plans/lectures/25-SystemScheduler与并行岛.md` | 阶段 1 | 未开始 |  |  |  |  |  |
| L26 | `lecture_plans/lectures/26-调试测试与课程收尾.md` | 阶段 1 | 未开始 |  |  |  |  |  |
