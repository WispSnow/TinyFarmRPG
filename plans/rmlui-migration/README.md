# RmlUi 迁移计划索引

本目录是 RmlUi 迁移计划的拆分版。

- 上层：本索引（状态看板 + 全局约束）
- 次层：每个阶段独立计划（执行某一阶段时，只读取对应 `phase-xx.md`）

状态标记：

- `[x]` 已完成
- `[~]` 部分完成
- `[ ]` 未开始

## 当前进度（2026-03-03）

| Phase | 状态 | 文档 |
|------|------|------|
| Phase 0: 应用层基础设施 | `[x]` | [`phase-00.md`](./phase-00.md) |
| Phase 1: 静态 HUD（TimeClockUI） | `[x]` | [`phase-01.md`](./phase-01.md) |
| Phase 2: 全屏 Overlay（ScreenFade） | `[x]` | [`phase-02.md`](./phase-02.md) |
| Phase 3: 菜单场景 | `[x]` | [`phase-03.md`](./phase-03.md) |
| Phase 4: 对话气泡 + Tooltip | `[ ]` | [`phase-04.md`](./phase-04.md) |
| Phase 5: 战斗 UI | `[ ]` | [`phase-05.md`](./phase-05.md) |
| Phase 6: 快捷栏（HotbarUI） | `[ ]` | [`phase-06.md`](./phase-06.md) |
| Phase 7: 物品栏（InventoryUI） | `[ ]` | [`phase-07.md`](./phase-07.md) |
| Phase 8: 清理旧 UI 框架 | `[ ]` | [`phase-08.md`](./phase-08.md) |


- 2026-03-06: Phase 3 已完成 TitleScene / PauseMenuScene / RestDialogScene / SaveSlotSelectScene 的 RmlUi 迁移。

## 全局约束

- 采用最优方案，不考虑向后兼容。
- `ImGui` 调试面板不迁移，保持现状。
- 旧 UI 框架最终在 Phase 8 完整移除。
- Phase 执行顺序默认为 `0 -> 8`，不要跨过前置阶段直接删除旧系统。

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
