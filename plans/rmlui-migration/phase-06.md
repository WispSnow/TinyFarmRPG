### Phase 6: 快捷栏 — HotbarUI

**目标**：迁移带拖拽交互的快捷栏。

**新建** `ui/rmlui/hud/hotbar.rml` + `hotbar.rcss`

- 水平 10 槽布局（CSS flexbox）
- 每槽：图标 + 数量标签 + 选中高亮
- data binding：`data-for="slot : hotbar_slots"`，每个 slot 绑定 `item_icon` / `item_count` / `is_active`
- 拖拽：RmlUi `drag="drag-drop"` + `dragdrop` 事件 → `HotbarBindCommand` / `HotbarUnbindCommand`
- 右键使用：`contextmenu` 或自定义事件 → `UseItemCommand`

**替换** `src/game/ui/hotbar_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/hotbar.rml/rcss` |
| 修改 | `src/game/scene/game_scene.cpp` |
| 删除 | `src/game/ui/hotbar_ui.h/cpp` |

**验证**：10 槽显示正确、拖拽物品到/从快捷栏、活跃槽高亮、右键使用物品。

---

