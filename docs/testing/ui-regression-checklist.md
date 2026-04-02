# UI 回归检查清单（RmlUi 生产界面）

本文档用于当前 RmlUi UI 架构的人工回归。

## 使用说明
- 适用范围：
  - `ui/rmlui/**`
  - `src/engine/ui/rmlui/**`
  - `src/game/scene/*_scene.cpp` 中的 RmlUi 场景
  - `src/game/ui/**` 中的 HUD / tooltip / dialogue / fade
- 执行时机：任何修改 UI 结构、样式、导航、drag-drop、文档生命周期后至少执行一次
- 结果记录：建议在 PR 描述中逐项标注 `PASS/FAIL/N/A`

## 预置条件
1. 可正常进入：Title、Game、Pause、SaveSlotSelect、InventoryMenu、RestDialog、Battle
2. 鼠标与键盘菜单输入可用；若有手柄，也建议补一轮
3. 若启用日志，建议打开 `debug/trace` 便于定位文档加载、事件绑定与焦点切换

## 重点场景

### 1) 菜单按钮与焦点导航
覆盖：Title / Pause / SaveSlotSelect / RestDialog / Battle

1. 进入菜单后，默认焦点应落在首个可操作元素。
2. 鼠标悬停按钮：视觉状态应切到 hover，焦点应同步跟随。
3. 鼠标点击按钮：应只触发一次对应动作。
4. 键盘或手柄 `menu_up/down/left/right/confirm`：
   - 焦点按预期移动
   - `confirm` 触发当前 focused 元素
5. 不可用按钮或槽位：
   - 不响应点击
   - 焦点不应停在不可用元素上

### 2) Gameplay HUD
覆盖：prompt bar / clock / hotbar

1. 进入 `GameScene` 后，overlay prompt bar 正常显示输入提示。
2. Hotbar 显示/隐藏切换后，位置与可见状态正确。
3. Hotbar active slot 切换只影响高亮，不应导致整体几何漂移。
4. 时钟文本和表针会随 `GameTime` 变化更新。

### 3) InventoryMenu 交互

1. 打开背包菜单后，背包槽位与 hotbar 区域都能正确显示内容。
2. 背包内拖拽：
   - move / swap / merge 行为正确
   - 无效区域释放时状态回收，不残留拖拽态
3. 背包 <-> hotbar：
   - 绑定、互换、解绑行为正确
4. 右键 action menu：
   - 在正确槽位打开
   - 最右/最下边界不会跑出容器
   - 关闭后焦点恢复到原先槽位或合理的 fallback
5. `sort`、`trash`、`use`、`activate` 等命令只触发一次且结果正确

### 4) 模态 Scene 与输入隔离

1. 打开 Pause / Save 覆盖层 / RestDialog / InventoryMenu 时：
   - gameplay 不应继续响应世界交互
   - `menu_cancel` / Back 能关闭当前顶层 Scene
2. 关闭覆盖层后：
   - 底层 Scene 恢复交互
   - 输入上下文与焦点状态恢复正常

### 5) 浮动控件

1. Tooltip：
   - hover 有物品槽位时显示
   - 跟随鼠标
   - 靠近屏幕边缘时会自动翻转/钳制，不跑出视口
2. Dialogue bubble：
   - `show / move / hide` 正常
   - 长文本自动换行
   - 跟随目标世界坐标移动，不明显滞后

### 6) Screen fade

1. 切场景或触发 fade 时有真实淡入/淡出过渡。
2. 淡出完成后可停留在全黑 holding 态。
3. 淡入完成后文档隐藏，不残留黑屏遮罩。
4. 不同时长的 `fadeIn/fadeOut(seconds)` 都应正常工作。

## 建议同时查看的自动化回归

- `tests/game/rmlui_architecture_regression_test.cpp`
- `tests/game/menu_hover_focus_sync_test.cpp`
- `tests/game/ui_layout_integration_test.cpp`
- `tests/game/game_scene_ui_controller_smoke_test.cpp`
- `tests/engine/ui/rml_document_controller_source_test.cpp`
- `tests/engine/ui/rml_screen_fade_transition_source_test.cpp`

## 失败记录要求

任一项失败时至少记录：
- 最短复现路径
- 发生场景
- 输入方式（鼠标 / 键盘 / 手柄）
- 是否稳定复现
- 截图或录屏
