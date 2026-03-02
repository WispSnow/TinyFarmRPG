### Phase 5: 战斗 UI

**目标**：迁移 BattleScene 的 UI。

**新建** `ui/rmlui/scenes/battle.rml` + `battle.rcss`

- 战斗面板：回合信息、单位状态、行动结果
- 操作按钮（Attack / Skill / Item / Guard / Escape / EndTurn）
- 按钮启禁状态通过 data binding 的 `data-if` / `data-attr-class` 控制
- 状态机不变，仅 UI 层替换

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/scenes/battle.rml/rcss` |
| 修改 | `src/game/scene/battle_scene.h/cpp` |

**验证**：战斗流程全通、按钮状态正确切换、结果显示正常。

---

