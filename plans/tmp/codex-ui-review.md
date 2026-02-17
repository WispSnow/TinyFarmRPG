# UI 模块复查（`src/engine/ui`）

## 结论摘要
- 对问题 1 的结论：**`InteractionBehavior` 以当前实现无法完全替代 `UIState`**，不建议直接做“全量替换”。
- 更现代、也更稳妥的方向是：**保留一个集中式交互状态内核（建议改为 enum/数据驱动），`InteractionBehavior` 作为可组合扩展层**。
- 目前 UI 交互链路里存在几个高优先级问题（含一个点击丢失边界），建议先修复再推进架构替换。

---

## 1. `InteractionBehavior` 能否完全替代 `UIState`

### 1.1 当前两者职责并不等价
- `UIState` 在承担核心状态机职责：
  - 状态切换与视觉落地：`UINormal/Hover/Pressed` 在 `enter()` 中切换视觉（`src/engine/ui/state/ui_normal_state.cpp:11`, `src/engine/ui/state/ui_hover_state.cpp:12`, `src/engine/ui/state/ui_pressed_state.cpp:12`）。
  - 点击/悬停核心语义：点击在 `UIPressedState::onMouseReleased` 触发（`src/engine/ui/state/ui_pressed_state.cpp:19`）。
  - 状态查询来源：`UIInteractive::isHovered/isPressed` 直接依赖 `state_`（`src/engine/ui/ui_interactive.h:90`）。
- `InteractionBehavior` 当前是“旁路回调扩展层”：
  - 接口是钩子集合，不维护统一状态（`src/engine/ui/behavior/interaction_behavior.h:10`）。
  - 在 `UIInteractive` 中与 `state_` 并行触发（`src/engine/ui/ui_interactive.cpp:198`, `src/engine/ui/ui_interactive.cpp:221`, `src/engine/ui/ui_interactive.cpp:235`）。

### 1.2 为什么“直接全替换”为高风险
- 你仍然需要一个统一状态源来保证：
  - `hover/pressed/disabled` 的互斥与转移规则。
  - `isHovered/isPressed` 的可查询一致性。
  - 视觉与业务回调（clicked/hover_enter/hover_leave）按同一状态变化驱动。
- 若只保留 behavior 回调，状态会分散到各行为对象中，容易出现多行为冲突、顺序依赖和不可预测的组合效果。

### 1.3 推荐替换方向（可做“全面替换 UIState 类”，但不是“去状态机”）
建议目标架构：
1. 在 `UIInteractive` 内引入统一交互相位（如 `enum class InteractionPhase { Normal, Hovered, Pressed, Disabled }`）。
2. 由 `UIInteractive` 统一做转移与视觉同步（`applyStateVisual`、sound、clicked 触发）。
3. `InteractionBehavior` 仅做可插拔增强（拖拽、Tooltip、统计、埋点等），并增加可选 `onStateChanged` 钩子。
4. 最后移除 `state/ui_*_state.*` 类层次。

结论：
- **可以把 `UIState` 这套“类状态机”替换掉**（用 enum/数据驱动）。
- **不建议用当前 `InteractionBehavior` 直接替换 `UIState` 的全部职责**。

---

## 2. 其它建议的重构改进点（按优先级）

### [P0] 点击丢失边界：`Normal` 状态下按下不进入 `Pressed`
- 触发条件：鼠标在同一帧“移入并按下”时，按下事件先于 UI hover 更新。
- 证据链：
  - 输入回调先于场景/UI 更新：`src/engine/core/game_app.cpp:62`, `src/engine/core/game_app.cpp:63`。
  - 输入系统在同一轮 `update()` 里先处理事件再触发动作回调：`src/engine/input/input_manager.cpp:99`, `src/engine/input/input_manager.cpp:110`。
  - `UIManager::onMousePressed` 直接调用 `mousePressed`，不先同步 hover：`src/engine/ui/ui_manager.cpp:165`, `src/engine/ui/ui_manager.cpp:172`。
  - `UINormalState` 没有处理 `onMousePressed`（仅处理 `onMouseEnter`）：`src/engine/ui/state/ui_normal_state.cpp:17`。
- 结果：可能出现“明明点中按钮但未触发 clicked”。
- 建议：
  - 方案 A：在 `UINormalState::onMousePressed` 直接转 `UIPressedState`。
  - 方案 B：`UIManager::onMousePressed` 先 `updateHovered(target)` 再转发按下。
  - 优先做 A（状态机语义更闭合）。

### [P1] `InteractionBehavior::onReleased` 是死接口
- 声明存在：`src/engine/ui/behavior/interaction_behavior.h:25`。
- 实际未调用：`UIInteractive::mouseReleased` 只调 `onDragEnd/onClick`（`src/engine/ui/ui_interactive.cpp:239`, `src/engine/ui/ui_interactive.cpp:250`）。
- 建议：补齐调用，明确调用时序（推荐：`onReleased` 在 `onDragEnd` 后、`onClick` 前）。

### [P1] Disabled 语义分散，状态与视觉容易失配
- 目前 `setInteractive(false)` 仅改布尔值，无状态/视觉收敛（`src/engine/ui/ui_interactive.h:80`）。
- 场景层需要手动补 `applyStateVisual(disabled)`（`src/game/scene/pause_menu_scene.cpp:304`, `src/game/scene/save_slot_select_scene.cpp:280`）。
- 建议：提供统一 `setEnabled(bool)`：
  - 内部同步交互可用性、视觉相位（Normal/Disabled）、必要时清理 pressed/hover 捕获。

### [P1] 鼠标状态清理不完整，可能漏发释放/拖拽结束
- `clearMouseState()` 仅 `mouseExit + 指针置空`，不发 `mouseReleased/cancel`（`src/engine/ui/ui_manager.cpp:211`）。
- 该路径会在 `clearElements()` 和 root 不可见时触发（`src/engine/ui/ui_manager.cpp:58`, `src/engine/ui/ui_manager.cpp:140`）。
- 建议：补一个 `cancelPointerInteraction()`，保证按下中元素收到取消释放，行为层能做收尾。

### [P2] 频繁堆分配的“类状态机”可降成本
- 状态切换每次 `make_unique`（`src/engine/ui/state/ui_hover_state.cpp:22`, `src/engine/ui/state/ui_pressed_state.cpp:22`）。
- 高频 hover/press 抖动时会有不必要分配与虚调用开销。
- 建议：改为 enum 相位 + 转移函数（同 1.3）。

### [P2] 布局容器使用 `getRequestedSize()`，与真实布局尺寸可能偏离
- `UIStackLayout` 与 `UIGridLayout` 都按 requested size 排布（`src/engine/ui/layout/ui_stack_layout.cpp:50`, `src/engine/ui/layout/ui_grid_layout.cpp:50`）。
- 对 stretch/anchor 子元素时可能出现对齐与换行误差。
- 建议：增加 measure/arrange 两阶段；排布阶段以最终测量值（而非原始请求值）为准。

### [P2] 命中测试每节点 `dynamic_cast`，可做轻量优化
- `findInteractiveAt` 中每次命中都 `dynamic_cast`（`src/engine/ui/ui_element.cpp:198`）。
- 建议：改成虚函数 `asInteractive()` 或类型标志位，避免递归 hit-test 的 RTTI 成本。

---

## 3. 推荐实施顺序（最小风险）
1. 先修 P0/P1 行为正确性问题（点击丢失、released 死接口、取消路径、enabled 语义统一）。
2. 再引入 `InteractionPhase`（与旧 `UIState` 并存一段时间），完成按钮/槽位迁移。
3. 最后移除 `state/ui_*` 类并清理 `setState/setNextState` API。

---

## 4. 建议补的回归测试清单
1. 光标同帧移入并按下按钮，释放于按钮内，必须触发 click。
2. 按下后移出再释放，不触发 click，状态回 Normal。
3. `setEnabled(false)` 时视觉必为 Disabled，且不再响应 hover/press/click。
4. 拖拽中 UI 被隐藏或 clear，必须触发一次取消收尾（drag end/cancel）。
5. 多 behavior 同挂载时，`onPressed -> onReleased -> onClick` 顺序稳定。
