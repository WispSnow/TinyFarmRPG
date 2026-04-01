### Phase 5: 提取 GameSceneUiController

**目标**：把 `GameScene` 里的场景级 UI composition 拆出去。

本阶段完成后，`GameScene` 应明显收缩，只保留玩法场景职责。

#### 本阶段要做的事

1. 新增 `game::ui::GameSceneUiController`
   - 直接持有 `Context&`
   - 不把 `InputManager&`、`Renderer&`、`RmlUiRuntime&` 拆成零散参数往下传
   - 推荐构造参数只额外补充 `Context` 里拿不到的场景局部状态，例如：
     - `entt::registry&`
     - `entt::entity player`
     - `uint64_t scene_instance_id`
   - `entt::dispatcher&` 继续优先通过 `Context` 获取，不单独作为主构造参数传入

2. 收拢创建 / 销毁职责
   - `TimeClockHud`
   - `HotbarUI`
   - `ItemTooltipUI`
   - `DialogueBubbleController`
   - `DialogueBubbleView`
   - overlay prompt bar 文档与 data bridge
   - `RmlScreenFade`

3. 收拢每帧逻辑
   - `update(float delta_time, entt::registry&)`
   - `refreshAnchoredWidgets(Camera&, float interpolation_alpha)`

4. 暴露高层接口给 `GameScene`
   - `toggleHotbar()`
   - `applyHotbarChanged(...)`
   - `applyHotbarSlotChanged(...)`
   - `setPromptBarVisible(bool visible)`
   - `screenFade()`

5. 收缩 `GameScene`
   - 删除零散 UI 成员
   - 删除零散 UI 初始化 / clean / update 逻辑
   - 改为通过 controller 驱动

#### 本阶段不做

- 不删除兼容壳
- 不做最终清理与测试收尾

#### 涉及文件

- 新建 `src/game/ui/game_scene_ui_controller.h`
- 新建 `src/game/ui/game_scene_ui_controller.cpp`
- 修改 `src/game/scene/game_scene.h`
- 修改 `src/game/scene/game_scene.cpp`
- 修改 `tests/game/menu_hover_focus_sync_test.cpp`

#### 验证

- `GameScene` 仍能正确显示 HUD、overlay、tooltip、dialogue bubble、fade
- `GameScene` 的字段和方法明显减少
- controller 的依赖关系清晰，接口只保留高层命令

#### 完成标记

- [x] 新增 `GameSceneUiController`
- [x] `GameScene` UI 初始化逻辑迁移到 controller
- [x] `GameScene` UI update 逻辑迁移到 controller
- [x] `GameScene` anchored widget 刷新逻辑迁移到 controller
- [x] `GameScene` clean 逻辑迁移到 controller
