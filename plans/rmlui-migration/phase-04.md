### Phase 4: 对话气泡 + Tooltip

**目标**：迁移世界锚定 UI 和鼠标跟随 UI。

#### Step 4.1: DialogueBubbleView

**新建** `ui/rmlui/hud/dialogue_bubble.rml` + `dialogue_bubble.rcss`

- 绝对定位面板，通过 C++ 每帧设置 `left` / `top`（世界→屏幕坐标转换）
- 支持 3 通道：加载 3 份文档实例（RmlUi 支持同文档多实例）
- 文本通过 data binding 更新
- 气泡背景使用 sprite sheet decorator 引用现有 `dialogue box.png`

**替换** `DialogueBubbleController` 中的 View 引用，改为操作 RML 文档元素

#### Step 4.2: ItemTooltipUI

**新建** `ui/rmlui/hud/item_tooltip.rml` + `item_tooltip.rcss`

- 绝对定位，每帧跟随鼠标
- data binding：`{{item_name}}` / `{{item_category}}` / `{{item_description}}`
- CSS 控制文字换行（`word-break`）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/dialogue_bubble.rml/rcss` |
| 新建 | `ui/rmlui/hud/item_tooltip.rml/rcss` |
| 修改 | `src/game/ui/dialogue_bubble_controller.h/cpp` |
| 删除 | `src/game/ui/dialogue_bubble_view.h/cpp` |
| 删除 | `src/game/ui/item_tooltip_ui.h/cpp` |
| 修改 | `src/game/scene/game_scene.cpp` |

**验证**：对话气泡跟随 NPC 移动、文本正确换行、tooltip 跟随鼠标、屏幕边缘不溢出。

---

