# UI 布局回归检查清单（RmlUi Layout）

本文档用于当前 RmlUi 布局体系的人工回归。

## 使用说明
- 适用范围：
  - `ui/rmlui/**`
  - `src/game/ui/**`
  - `src/game/scene/inventory_menu_scene.cpp`
  - `src/engine/ui/rmlui/rml_screen_fade.cpp`
- 执行时机：改动 RML/RCSS、slot 布局、浮动控件定位、屏幕过渡后至少执行一次
- 结果记录：建议按场景标注 `PASS/FAIL/N/A`，并附同分辨率截图

## 预置条件
1. 游戏可进入 `GameScene`，可打开 InventoryMenu 与 Hotbar。
2. 推荐固定逻辑分辨率做对比，避免视口差异引入误判。
3. Inventory 中准备足够物品，确保可以覆盖空槽、堆叠物品、拖拽、action menu。

## 执行记录模板
- 日期：
- 分支/提交：
- 分辨率：
- 场景结果：
  - InventoryMenu 网格：
  - InventoryMenu action menu：
  - HUD Hotbar：
  - Tooltip：
  - Dialogue bubble：
  - Screen fade：
- 备注（失败复现步骤或截图路径）：

## 重点场景

### 1) InventoryMenu：背包网格与 hotbar 区

1. 打开 InventoryMenu。
2. 观察背包网格首行首列是否对齐，无越界、无重叠。
3. 相邻槽位的水平与垂直间距应一致。
4. 背包区与 hotbar 区之间的相对位置稳定。
5. 空槽与有物品槽混排时，槽位尺寸不应因内容变化而拉伸。
6. 打开/关闭 action menu 前后，grid 自身位置不应抖动。

通过标准：
- 网格稳定，间距一致，两个区域都处在各自容器内。

### 2) InventoryMenu：action menu 几何

1. 在中间槽位打开 action menu，菜单应贴近当前槽位。
2. 在最右侧槽位打开 action menu，应向左翻转。
3. 在最下侧槽位打开 action menu，应保持在 `slot-region` 内，不越界。
4. action menu 打开后不应挤压背包网格本身。

通过标准：
- 菜单锚定正确，边界钳制稳定，不影响原始布局。

### 3) HUD Hotbar

1. 进入 `GameScene`，确认 Hotbar 位于底部中间区域。
2. 所有槽位沿水平方向排列，间距一致。
3. 切换 active slot 时，高亮变化不应导致容器尺寸或位置变化。
4. 多次显示/隐藏 Hotbar 后，位置不应漂移。
5. 进行拖拽后，slot 容器几何不应变化。

通过标准：
- 只变视觉状态，不变几何。

### 4) Tooltip

1. hover 有物品槽位时，tooltip 自适应内容高度。
2. 长描述文本自动换行，不溢出面板。
3. 移动鼠标时 tooltip 跟随。
4. 靠近屏幕边缘时会自动翻转/钳制，不出界。

通过标准：
- 尺寸由内容驱动，定位稳定，无裁切。

### 5) Dialogue bubble

1. 长文本自动换行，面板高度自动增长。
2. 气泡跟随实体世界坐标移动。
3. 镜头移动或角色移动时，气泡相对目标位置稳定。
4. `show / move / hide` 多次切换后，不应残留旧尺寸或旧位置。

通过标准：
- 文本布局与世界锚点都稳定，无明显一帧跳动。

### 6) Screen fade

1. fade overlay 覆盖整个逻辑视口。
2. `fadeOut(seconds)` 与 `fadeIn(seconds)` 时，透明度变化平滑。
3. 淡入完成后 overlay 隐藏，不再遮挡下层内容。

通过标准：
- 过渡正确、范围正确、不残留遮罩。

## 自动化参考

- `tests/game/ui_layout_integration_test.cpp`
- `tests/game/dialogue_bubble_controller_test.cpp`
- `tests/engine/ui/rml_screen_fade_transition_source_test.cpp`
- `tests/engine/ui/rmlui_transition_behavior_test.cpp`

说明：
- `ui_layout_integration_test` 在 headless 环境下可能被 `SKIPPED`
- 这类情况下仍需要一轮实机布局确认
