# RmlUi 迁移计划索引

本目录是 RmlUi 迁移计划的拆分版。

- 上层：本索引（状态看板 + 全局约束）
- 次层：每个阶段独立计划（执行某一阶段时，只读取对应 `phase-xx.md`）

状态标记：

- `[x]` 已完成
- `[~]` 部分完成
- `[ ]` 未开始

## 当前进度（2026-03-10）

| Phase | 状态 | 文档 |
|------|------|------|
| Phase 0: 应用层基础设施 | `[x]` | [`phase-00.md`](./phase-00.md) |
| Phase 1: 静态 HUD（TimeClockUI） | `[x]` | [`phase-01.md`](./phase-01.md) |
| Phase 2: 全屏 Overlay（ScreenFade） | `[x]` | [`phase-02.md`](./phase-02.md) |
| Phase 3: 菜单场景 | `[x]` | [`phase-03.md`](./phase-03.md) |
| Phase 4: 对话气泡 + Tooltip | `[x]` | [`phase-04.md`](./phase-04.md) |
| Phase 5: 战斗 UI | `[x]` | [`phase-05.md`](./phase-05.md) |
| Phase 6: 快捷栏（HotbarUI） | `[x]` | [`phase-06.md`](./phase-06.md) |
| Phase 7: 物品栏（InventoryUI） | `[x]` | [`phase-07.md`](./phase-07.md) |
| Phase 8: 补齐残留迁移 | `[ ]` | [`phase-08.md`](./phase-08.md) |
| Phase 9: 清理旧 UI 框架 | `[ ]` | [`phase-09.md`](./phase-09.md) |


- 2026-03-06: Phase 3 已完成 TitleScene / PauseMenuScene / RestDialogScene / SaveSlotSelectScene 的 RmlUi 迁移。
- 2026-03-07: Phase 4 已完成 DialogueBubbleView / ItemTooltipUI 的 RmlUi 迁移，保留旧 API，内部改为 RmlUi wrapper。
- 2026-03-07: Phase 5 已完成 BattleScene 的 RmlUi 迁移，保留现有战斗状态机与 6 个动作按钮行为。
- 2026-03-08: Phase 6 已完成 HotbarUI 的 RmlUi 迁移，保留左键激活 / 右键使用 / Tooltip / 内部拖拽换位 / 拖出解绑；`inventory ↔ hotbar` 跨 UI 拖拽继续延期到 Phase 7。
- 2026-03-08: Phase 6 收尾已完成：`HotbarUI` 改用独立 `DataTypeRegister`，数量文本仅在 `count > 1` 时显示，相关旧版 UIElement 布局测试已改为 RmlUi 文档级测试（当前 headless 测试环境下自动跳过）。
- 2026-03-09: Phase 7 已完成 InventoryUI 的 RmlUi wrapper 迁移，保留分页 / 右键使用 / Tooltip / inventory 内部拖拽，并补齐 `inventory ↔ hotbar` 跨 UI 拖拽协调。
- 2026-03-09: Phase 7 新增跨文档 `drag: clone + dragdrop` 探针和 Inventory 文档级布局测试；当前 headless 测试环境下若 `RmlUILayer` 不可用则自动跳过。
- 2026-03-10: 原 Phase 8 拆分为两阶段：Phase 8 先补齐 `GameScene` / `UIPresetManager` / 共享类型等残留迁移，Phase 9 再做最终删除与工具链清扫。

## 全局约束

- 采用最优方案，不考虑向后兼容。
- `ImGui` 调试面板不迁移，保持现状。
- 旧 UI 框架最终在 Phase 9 完整移除。
- Phase 执行顺序默认为 `0 -> 9`，不要跨过前置阶段直接删除旧系统。

## 架构共识（跨 Phase）

- RmlUi 多文档 + 文档归属（owner）+ 活跃场景交互隔离。
- 数据驱动优先：Data Model + `DirtyVariable()`。
- UI 事件通过桥接层映射回游戏命令（保持命令契约稳定）。
- `MapTransitionSystem` 通过 `IScreenFade` 接口解耦具体实现。
- 屏幕空间文字逐步迁移到 RmlUi，`TextRenderer` 最终仅保留世界文字路径。

## 使用方式

1. 进入要执行的 Phase，只打开对应 `phase-xx.md`。
2. 执行完成后仅回写本索引的状态（必要时补充 1-2 行备注）。
3. 若某 Phase 仅部分完成，统一标记为 `[~]`，并在该 Phase 文档顶部补充未完成项。
