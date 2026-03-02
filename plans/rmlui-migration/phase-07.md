### Phase 7: 物品栏 — InventoryUI

**目标**：迁移最复杂的 UI 组件。

**新建** `assets/ui/rmlui/hud/inventory.rml` + `inventory.rcss`

- 可拖动面板（RmlUi `drag="drag"` 在标题栏 + `move_target` 属性）
- 5×4 网格：RmlUi 6.2 不支持 CSS Grid，采用 **flexbox wrap** 布局——外层容器 `display: flex; flex-wrap: wrap; width: 5*slot_width`，每个 slot 固定宽度，`data-for` 循环渲染 20 个 slot
- 分页：上/下页按钮 + 页码标签，通过 data binding 切换当前页数据
- 关闭按钮
- 拖拽排序：slot 间拖拽 → `InventoryMoveCommand`
- 跨 UI 拖拽：inventory slot ↔ hotbar slot
  - RmlUi drag-drop 支持跨文档拖放（同一 `Rml::Context` 内）
  - 拖拽源标记 `data-source="inventory"` `data-slot-index="N"`
  - 目标通过 `dragdrop` 事件解析来源并分发对应命令
- 右键使用：→ `UseItemCommand`

**替换** `src/game/ui/inventory_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/hud/inventory.rml/rcss` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 删除 | `src/game/ui/inventory_ui.h/cpp` |
| 删除 | `src/game/ui/ui_drag_drop_helpers.h` |

**验证**：物品栏开关正常、分页切换、拖拽排序、跨 UI 拖拽（inventory ↔ hotbar）、右键使用、物品数量显示正确。

---

