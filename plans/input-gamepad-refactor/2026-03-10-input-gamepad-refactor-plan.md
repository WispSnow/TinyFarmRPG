# Input 模块手柄重构计划索引

本文件是方案 B 的上层概览和索引。

- 上层：本索引，负责记录目标、全局约束、阶段状态和阅读方式
- 下层：4 个独立阶段计划，执行时只需要读取对应阶段文档

已确认采用方案 B：统一 Binding 模型 + 语义动作整理。

状态标记：

- `[x]` 已完成
- `[~]` 部分完成
- `[ ]` 未开始

## 当前进度（2026-03-10）

| Phase | 状态 | 文档 |
|------|------|------|
| Phase 1: 输入核心（Binding + SDL3 手柄接入） | `[ ]` | [`phase-01.md`](./phase-01.md) |
| Phase 2: 玩法语义重构（语义动作 + Controller 目标） | `[ ]` | [`phase-02.md`](./phase-02.md) |
| Phase 3: UI 导航与上下文（Menu 动作 + InputContext） | `[ ]` | [`phase-03.md`](./phase-03.md) |
| Phase 4: 后续增强（Buffer / Glyph / Rumble / Rebind） | `[ ]` | [`phase-04.md`](./phase-04.md) |

## 目标

- 为 `InputManager` 补齐 SDL3 手柄支持。
- 将输入层从“键鼠特化”整理为“设备无关动作层”。
- 让键盘、鼠标、手柄共用同一套语义动作。
- 为后续 JRPG 菜单、对话、战斗和技能系统预留统一输入基础。

## 全局约束

- 采用最优方案，不考虑向后兼容。
- `InputManager` 仍然是 SDL 事件唯一入口，不新增第二个 poll 点。
- 保留现有动作查询主接口：`onAction()`、`isActionDown()`、`isActionPressed()`、`isActionReleased()`。
- Phase 默认按 `1 -> 4` 顺序执行，不要跳过前置阶段直接做后置特性。
- 每次实施只读取本索引 + 当前 Phase 文档，避免无关上下文占用。

## 架构共识（跨 Phase）

- Binding 模型从字符串猜测逻辑中独立出来，显式区分键盘、鼠标、手柄按钮、手柄轴方向。
- 上层玩法逐步从 `mouse_left` / `mouse_right` / `hotbar_1..10` 迁移到 `primary_action` / `secondary_action` / `hotbar_prev` / `hotbar_next` 等语义动作。
- 手柄目标选择不走“强模拟鼠标”，而走控制器友好的目标模型。
- 菜单导航通过 `menu_*`、`menu_confirm`、`menu_cancel` 等语义动作驱动，不直接绑定某个 UI 框架的 SDL 事件。
- `InputContext` 和输入缓冲是后续 JRPG 扩展的基础设施，但不提前把所有复杂度塞进 Phase 1。

## 使用方式

1. 先读本索引，确认当前阶段和全局约束。
2. 只打开当前要执行的 `phase-0x.md`。
3. 执行完成后，只回写本索引状态和对应 Phase 文档中的待办。
4. 如果某阶段只完成一部分，统一标记为 `[~]`，并在该阶段文档顶部补未完成项。

## 阶段划分说明

- Phase 1：只解决输入核心问题，让 SDL3 手柄事件、binding 解析、动作状态流转真正跑通。
- Phase 2：只解决玩法层“鼠标中心”问题，让手柄能自然参与世界操作。
- Phase 3：只解决菜单导航和上下文切换问题，让 UI 和场景切换有统一输入边界。
- Phase 4：补增强项，不阻塞前 3 个阶段交付。
