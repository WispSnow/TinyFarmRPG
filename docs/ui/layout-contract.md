# UI 布局语义契约（Layout Contract）

本文档冻结当前 UI 布局语义，作为 `UIL-*` 重构期间的行为基线。

## 1. 适用范围
- 核心类型：
  - `src/engine/ui/ui_element.h`
  - `src/engine/ui/layout/ui_layout.h`
  - `src/engine/ui/layout/ui_stack_layout.h`
  - `src/engine/ui/layout/ui_grid_layout.h`
- 目标：明确 `requested size` / `layout size`、锚点/拉伸、margin/padding 与 `onLayout()` 约束。

## 2. 术语与 API 对应
- `requested size`：
  - 含义：元素声明“希望的尺寸”。
  - 存储：`UIElement::size_`。
  - API：`getRequestedSize()`、`setSize(...)`。
- `layout size`：
  - 含义：本轮布局计算后的实际尺寸（渲染与命中使用）。
  - 存储：`UIElement::layout_size_`。
  - API：`getLayoutSize()`、`getSize()`。
- `layout position`：
  - 含义：元素左上角的屏幕坐标。
  - 存储：`UIElement::layout_position_`。
  - API：`getScreenPosition()`、`getLayoutPosition()`。
- `layout override size`：
  - 含义：父布局容器在布局阶段临时覆盖子节点实际尺寸，不改变子节点 requested size。
  - 存储：`UIElement::layout_override_size_`。
  - API：`setLayoutOverrideSize(...)`、`clearLayoutOverrideSize()`。
- `content bounds`：
  - 含义：父节点可供子节点布局的区域（扣除父节点 padding）。
  - API：`getContentBounds()`。

## 3. 基础语义（UIElement）

### 3.1 脏标记与布局时机
- 以下操作会触发布局脏化（`layout_dirty_ = true`）：
  - `setSize`、`setPosition`、`setAnchor`、`setPadding`、`setMargin`
  - `addChild/removeChild/removeAllChildren`
  - `setParent`
- `setPivot` 会脏化自己与后代（`invalidateLayout(true)`）。
- 布局为惰性计算：在 `update/render/getLayoutSize/getBounds/getScreenPosition/...` 中通过 `ensureLayout()` 触发。

### 3.2 尺寸计算规则
- 若元素无父节点（根节点）：
  - `layout_size = requested_size`
  - `layout_position = position`
  - 注意：根节点路径不会调用 `onLayout()`。
- 若元素有父节点：
  1. 取父节点 `content bounds` 作为可用区域。
  2. 计算锚点绝对位置：
     - `anchor_min_pos = parent_origin + parent_size * anchor_min`
     - `anchor_max_pos = parent_origin + parent_size * anchor_max`
  3. 判断是否 stretch：
     - 任一轴 `anchor_min != anchor_max` 即视为 stretch。
  4. 计算 `layout_size`：
     - 若存在 `layout_override_size`：优先使用 override。
     - 否则，非 stretch：`layout_size = requested_size`
     - 否则，stretch：`layout_size = (anchor_max_pos - anchor_min_pos) - (margin.left+right, margin.top+bottom)`，并按轴 `clamp >= 0`

### 3.3 位置计算规则
- `anchor_reference = anchor_min_pos + position`
- `layout_position = anchor_reference + (margin.left, margin.top) - layout_size * pivot`
- 结论：
  - margin 总是影响位置（至少 left/top）。
  - margin 仅在 stretch 模式下参与尺寸扣减。

### 3.4 getSize() 语义约定
- `getSize()` 返回 `layout size`（不是 `requested size`）。
- 需要“配置尺寸”时必须使用 `getRequestedSize()`。

## 4. padding / margin / anchor / pivot 优先级
- 父节点 `padding` 先定义子节点可用布局区域（`content bounds`）。
- 子节点 `anchor` 决定参考区域与是否 stretch。
- 子节点 `margin` 在锚点区域内做偏移与（stretch 时）尺寸扣减。
- 子节点 `pivot` 在最终尺寸基础上做锚点对齐偏移。
- 子类 `onLayout()` 最后执行，用于安排子节点。

## 5. `onLayout()` 约束
- 推荐约束：
  - 只做子节点布局定位（`setPosition`）与容器内部排布。
  - 避免写入会触发链式脏化的持久属性（例如子节点 `setSize`、`setAnchor`）。
- 原因：
  - `onLayout()` 在 `ensureLayout()` 内部调用，写入持久属性会导致再次脏化，易形成抖动或隐式耦合。

## 6. 布局容器当前行为

### 6.1 UIStackLayout
- 主轴长度统计使用 `child->getLayoutSize()`（最终布局尺寸语义）。
- 仅统计可见子项，`spacing` 也只在可见子项间生效。
- `Alignment` 仅作用主轴（`Start/Center/End`）。
- 交叉轴固定 `Start`（Vertical=Left，Horizontal=Top）。
- 当内容总长度大于容器主轴可用空间时，Center/End 允许出现负偏移（不自动裁剪）。
- `auto_resize=true` 时会更新容器自身 requested size（通过 `setSizeInternal`）。
- 若检测到“主轴 stretch 子项”，会输出一次调试告警（提示完整伸缩协商仍待 `UIL-021` 收敛）。

### 6.2 UIGridLayout
- 可见子项按行优先填充，隐藏子项不占格子。
- `column_count <= 0` 的设置请求会被忽略，保留原值。
- `cell_size` 同时大于 0 时使用固定 cell；否则使用子项 requested size。
- 固定 cell 模式通过 `setLayoutOverrideSize(cell_size)` 生效，不会修改子项 requested size。

## 7. 已知偏差与后续任务
- 偏差 A：`UIStackLayout` 对主轴 stretch 子项的处理仍是兼容实现，完整协商语义待 `UIL-021` 继续收敛。
- 偏差 B：部分控件在 `onLayout()` 中修改会触发脏化的属性（如 `UIProgressBar` 调整 fill anchor，`UIL-023` 收敛）。

## 8. 测试映射（当前已覆盖）
- `tests/engine/ui/ui_stack_layout_test.cpp`
  - visible 子项跳过、spacing、Center/End 对齐、auto_resize、主轴 stretch 对齐语义。
- `tests/engine/ui/ui_grid_layout_test.cpp`
  - fixed cell + spacing、visible 子项跳过、列数边界、fixed→intrinsic 回退。

## 9. 变更纪律
- 任何影响上述语义的改动，必须同时更新：
  - 本文档；
  - 对应布局测试（至少单测）；
  - `plans/2026-02-17-ui-layout-refactor-backlog.md` 的任务状态与验收记录。
