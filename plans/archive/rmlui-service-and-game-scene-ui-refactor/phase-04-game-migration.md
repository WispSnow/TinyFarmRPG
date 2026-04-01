### Phase 4: 游戏层场景与 UI Wrapper 迁移

**目标**：把游戏层对 `GLRenderer -> RmlUILayer` 的依赖全部切到 `Context::getRmlUi()`。

本阶段重点不只是“能编译”，而是：

- focus 行为不回退
- `InventoryMenuScene` 的复杂交互不回退
- wrapper 只依赖 runtime，不碰 render backend

#### 本阶段要做的事

1. 迁移菜单场景
   - `TitleScene`
   - `PauseMenuScene`
   - `SaveSlotSelectScene`
   - `RestDialogScene`
   - `BattleScene`

2. 重点迁移 `InventoryMenuScene`
   - `Rml::DataTypeRegister`
   - tooltip
   - 拖拽
   - action menu
   - 焦点保存与恢复
   - queued focus

3. 迁移游戏层 wrapper
   - `TimeClockHud`
   - `HotbarUI`
   - `ItemTooltipUI`
   - `DialogueBubbleView`
   - `RmlScreenFade`

4. 同步更新 `GameScene` 中的 wrapper 构造调用
   - wrapper 构造签名从 `RmlUILayer&` 迁到 `RmlUiRuntime&` 后，`GameScene` 中对应创建代码必须同步更新
   - 本阶段结束时，`GameScene` 必须在不依赖 `getRmlUILayer()` 的前提下保持可编译

5. 清理游戏层对 `getRmlUILayer()` 的直接调用

关于 `RmlScreenFade`：

- 它位于 `src/engine/ui/rmlui/`，语义上更接近引擎层 UI primitive
- 但当前它的构造和组合路径与 `GameScene` 强耦合
- 因此本计划把它放在 Phase 4 和游戏层调用点一起迁移，避免在 Phase 2 提前牵动 `GameScene`

#### 本阶段不做

- 不删除兼容壳
- 不提取 `GameSceneUiController`
- 不补最终测试文件

#### 涉及文件

- 修改 `src/game/scene/title_scene.cpp`
- 修改 `src/game/scene/pause_menu_scene.cpp`
- 修改 `src/game/scene/save_slot_select_scene.cpp`
- 修改 `src/game/scene/rest_dialog_scene.cpp`
- 修改 `src/game/scene/battle_scene.cpp`
- 修改 `src/game/scene/inventory_menu_scene.cpp`
- 修改 `src/game/scene/game_scene.cpp`
- 修改 `src/game/ui/time_clock_hud.h`
- 修改 `src/game/ui/time_clock_hud.cpp`
- 修改 `src/game/ui/hotbar_ui.h`
- 修改 `src/game/ui/hotbar_ui.cpp`
- 修改 `src/game/ui/item_tooltip_ui.h`
- 修改 `src/game/ui/item_tooltip_ui.cpp`
- 修改 `src/game/ui/dialogue_bubble_view.h`
- 修改 `src/game/ui/dialogue_bubble_view.cpp`
- 修改 `src/engine/ui/rmlui/rml_screen_fade.h`
- 修改 `src/engine/ui/rmlui/rml_screen_fade.cpp`

#### 验证

- 全部菜单场景正常加载、交互、默认聚焦
- `InventoryMenuScene` 的拖拽和 action menu 焦点恢复正常
- wrapper 不再通过 renderer 间接拿 RmlUi
- `GameScene` 中的 wrapper 构造调用已全部同步到新签名并可编译通过
- `src/game/**` grep `getRmlUILayer()` 结果为零

#### 完成标记

- [x] `TitleScene` 迁移
- [x] `PauseMenuScene` 迁移
- [x] `SaveSlotSelectScene` 迁移
- [x] `RestDialogScene` 迁移
- [x] `BattleScene` 迁移
- [x] `InventoryMenuScene` 迁移
- [x] `TimeClockHud` 迁移
- [x] `HotbarUI` 迁移
- [x] `ItemTooltipUI` 迁移
- [x] `DialogueBubbleView` 迁移
- [x] `RmlScreenFade` 迁移
- [x] `GameScene` 中的 wrapper 构造调用同步更新
