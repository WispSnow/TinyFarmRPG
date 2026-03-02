### Phase 3: 菜单场景

**目标**：将 TitleScene、PauseMenuScene、RestDialogScene、SaveSlotSelectScene 的 UI 迁移到 RmlUi。这些是独立的全屏/模态 UI，不涉及拖拽，适合批量迁移。

#### Step 3.1: TitleScene

**新建** `assets/ui/rmlui/scenes/title.rml` + `title.rcss`

- 背景图 + Logo（CSS animation 实现上下浮动）
- 按钮列表（Start / Load / Menu / Exit）
- 按钮点击通过 `RmlEventBridge` → scene method 调用

**修改** `src/game/scene/title_scene.cpp`：移除 UIManager 创建，改为加载 RML 文档

#### Step 3.2: PauseMenuScene

**新建** `assets/ui/rmlui/scenes/pause_menu.rml` + `pause_menu.rcss`

- 半透明遮罩（CSS `background-color: rgba(0,0,0,0.5)`）
- 中央面板 + 按钮组（Resume / Save / Load / BackToTitle）
- 音量/时间刻度控制：data binding 绑定数值，±按钮触发 C++ 回调

**修改** `src/game/scene/pause_menu_scene.cpp`

#### Step 3.3: RestDialogScene

**新建** `assets/ui/rmlui/scenes/rest_dialog.rml` + `rest_dialog.rcss`

- 模态面板 + 小时±调节 + 确认/取消
- data binding 绑定 hours 数值

**修改** `src/game/scene/rest_dialog_scene.cpp`

#### Step 3.4: SaveSlotSelectScene

**新建** `assets/ui/rmlui/scenes/save_slot_select.rml` + `save_slot_select.rcss`

- 存档槽列表（`data-for` 循环渲染）
- 覆写确认模态框（CSS visibility 控制显隐）

**修改** `src/game/scene/save_slot_select_scene.cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `assets/ui/rmlui/scenes/title.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/pause_menu.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/rest_dialog.rml/rcss` |
| 新建 | `assets/ui/rmlui/scenes/save_slot_select.rml/rcss` |
| 修改 | `src/game/scene/title_scene.h/cpp` |
| 修改 | `src/game/scene/pause_menu_scene.h/cpp` |
| 修改 | `src/game/scene/rest_dialog_scene.h/cpp` |
| 修改 | `src/game/scene/save_slot_select_scene.h/cpp` |

**验证**：所有菜单场景正常交互、按钮响应、数值调节正确、场景切换流畅。

---

