# TinyFarmRPG UI 交互重构 Backlog（2026-02-17）

## 输入来源与采纳策略
- 参考文档：`plans/tmp/claude-ui-review.md`、`plans/tmp/codex-ui-review.md`
- 采纳（来自 Claude 且经本地代码核对后合理）：
  - 用 `InteractionPhase`（枚举）替代当前 `UIState` 类状态机。
  - 在 `UIInteractive` 中集中状态迁移（`transitionTo()`）而不是分散在多个状态类。
  - 逐步收敛 `UIButtonVisualState` 与通用交互状态的双轨并存问题。
  - 行为扩展继续走 `InteractionBehavior`，并考虑补充通用点击行为。
- 调整后执行（避免一次性大改）：
  - 不采用“立刻删除 `state/` 目录”的一次性方案，改为“并行引入 -> 切换主路径 -> 删除遗留”。
  - 先修正确性问题（点击丢失、释放回调缺失、禁用态不一致）再做架构替换。
- 暂缓（P2）：
  - 拖拽状态完全行为化（`DragTracker`）
  - `UIInputAdapter` 抽象层

## 目标与非目标
- 目标：
  - 交互状态逻辑可读、可测、可扩展。
  - 消除 `UIState + UIButtonVisualState + behavior` 的职责重叠。
  - 保持现有 UI 玩法行为不回退（按钮、背包、快捷栏、模态遮罩、拖拽预览）。
- 非目标：
  - 不在本轮重做整个布局系统。
  - 不在本轮引入新的输入设备体系。

## 完成定义（DoD）
- 每个迭代项单独提交，支持独立回滚。
- 每个迭代项都通过：
  - 编译 + 单测/回归测试（至少 `ctest --test-dir build --output-on-failure -j4`）
  - 指定手工回归清单
- 不允许跨迭代引入“先改 A 才能跑 B”的隐式耦合。

## 迭代节奏（小步）
- 每个 `UIR` 控制在 0.5~1.5 天。
- 每次只处理一类风险：
  - 先行为正确性
  - 再状态内核迁移
  - 最后做结构收口与性能优化

---

## Iteration 0：基线与防护网

### UIR-000（P0）建立 UI 回归基线 （已完成）
- 目标：冻结当前行为，避免后续“看起来能跑但交互悄悄变了”。
- 主要改动：
  - 新增 `docs/testing/ui-regression-checklist.md`（或等价文档）
  - 记录关键场景回归步骤：Title/Pause/SaveSlot/Inventory/Hotbar/RestDialog
  - 标记当前已知问题（同帧移入按下）为“预期失败项”
- 验收标准：
  - checklist 可被他人独立执行
  - 每个后续迭代都能复用该 checklist

### UIR-001（P0）补最小自动化验证入口 （已完成）
- 目标：为后续状态迁移准备可重复测试点。
- 主要改动：
  - 在现有测试体系中补 1~2 个可执行的 UI 交互状态测试（可先是 `UIInteractive` 级别）
  - 明确当前测试缺口：`tests/engine/ui/` 现有 `ui_button_factory_api_test.cpp`、`ui_preset_manager_*`，但没有交互状态迁移测试
  - 现阶段先采用源码契约测试做快速防护；已在 `UIR-022` 增补运行时行为测试
- 验收标准：
  - 至少覆盖 `press/release inside/outside` 的基本语义

---

## Iteration 1：先修正确性（不动大架构，可并行）

### UIR-010（P0）修复“同帧移入并按下”点击丢失（已完成：代码+自动化）
- 目标：消除最关键行为 bug。
- 主要改动：
  - 在 `UINormalState::onMousePressed()` 直接切换到 `UIPressedState`
  - 说明：不采用 “`UIManager::onMousePressed()` 先 `updateHovered()` 再 `mousePressed()`” 方案；当前 `setNextState` 是延迟生效，该方案在同一调用链内仍可能停留在 `Normal`，不能根治问题
- 验收标准：
  - 同帧移入并按下 -> 释放于元素内，必触发 click
  - 代码与自动化已完成；手工回归按 `docs/testing/ui-regression-checklist.md` 执行

### UIR-011（P1）补齐 `onReleased` 与取消路径（已完成：代码+自动化）
- 目标：保证行为链事件完整、可收尾。
- 主要改动：
  - `UIInteractive::mouseReleased()` 中调用 `InteractionBehavior::onReleased()`
  - `UIManager::clearMouseState()` 增加取消释放流程（避免仅置空指针）
- 验收标准：
  - 拖拽中隐藏 UI / clearElements 时，行为层能收到结束或取消通知
  - 代码与自动化已完成；手工回归按 `docs/testing/ui-regression-checklist.md` 执行

### UIR-012（P1）统一启用/禁用语义（已完成：代码+自动化）
- 目标：避免 `setInteractive(false)` 与视觉状态脱钩。
- 主要改动：
  - 新增 `setEnabled(bool)`（内部处理交互开关 + 视觉切换 + 按下态清理）
  - 场景代码迁移：`pause_menu_scene`、`save_slot_select_scene` 等
- 验收标准：
  - disabled 元素不可 hover/press/click，且视觉一致
  - 代码与自动化已完成；手工回归按 `docs/testing/ui-regression-checklist.md` 执行

---

## Iteration 2：并行引入新状态内核（不删旧实现）

### UIR-020（P0）在 `UIInteractive` 引入 `InteractionPhase`（已完成：代码+自动化）
- 目标：建立未来主状态模型，但不立即切流量。
- 主要改动：
  - 增加 `enum class InteractionPhase { Normal, Hovered, Pressed, Disabled }`
  - 增加只读查询与调试日志（phase 变化可观测）
- 验收标准：
  - 不改变现有行为，编译与回归通过
  - 代码与自动化已完成；手工回归按 `docs/testing/ui-regression-checklist.md` 执行

### UIR-021（P0）新增 `transitionTo()` 并让旧状态类委托到它（已完成：代码+自动化）
- 目标：先合并状态副作用逻辑（视觉/音效/回调）到一处。
- 主要改动：
  - `UIInteractive::transitionTo()` 承接状态进入副作用
  - `UINormal/Hover/Pressed` 保留，但内部改为调用 `transitionTo()`
  - 在本迭代内同步补基础迁移测试：`Normal->Hovered->Pressed->(Hovered|Normal)`（先覆盖主链，再在 UIR-022 覆盖边界）
- 验收标准：
  - 逻辑等价，回归不变
  - 新旧路径输出一致（可用 trace 校验）
  - `Normal -> Hovered` 迁移必须保留 hover 音效触发（与当前 `UINormalState::onMouseEnter()` 语义一致）
  - 代码与自动化已完成；手工回归按 `docs/testing/ui-regression-checklist.md` 执行

### UIR-022（P1）补状态迁移测试（已完成：代码+自动化）
- 目标：为“删除状态类”提供安全垫。
- 主要改动：
  - 覆盖边界迁移：disabled 切入/切出、按下中取消、隐藏/清空时的状态收敛
  - 补充声音与回调时序断言（hover/click 不重复、不丢失）
  - 增补运行时行为测试（替代/补强源码字符串契约测试）
  - 新增 `tests/engine/ui/ui_interaction_runtime_test.cpp`，覆盖：
    - `Normal -> Hovered -> Pressed -> (Hovered|Normal)` 主链行为
    - `Normal -> Pressed` 路径不触发 `hover_enter()`（UIR-010 期望语义）
    - `setEnabled(false)` 按下中取消（release(false)/no click）
    - `UIManager::clearElements()` 收敛按下捕获并触发取消释放
  - 接入构建：`tests/CMakeLists.txt` 新增 `ui_interaction_runtime_test.cpp`
- 验收标准：
  - 关键迁移均有自动化断言
  - `ctest --test-dir build --output-on-failure -j4` 通过（运行时测试在无 SDL video 的环境下为 `GTEST_SKIP`，不影响结果）

---

## Iteration 3：切换主路径并移除 `state/` 目录

### UIR-030（P0）`mouse*` 事件改为直接驱动 `InteractionPhase`（已完成：代码+自动化）
- 目标：让类状态机退场，事件路径扁平化。
- 主要改动：
  - `UIInteractive::mouseEnter/Exit/Pressed/Released` 改为基于 `InteractionPhase` 直接驱动迁移，不再走 `state_->onMouse*` 分发
  - `UIInteractive::transitionTo()` 从 `setNextState()` 切换为 `setState()`，迁移在事件回调内立即生效
  - `mouseReleased()` 内联 Pressed release 语义（inside -> Hovered + `clicked()`；outside -> Normal），并保持 `onReleased/onClick` 行为分发
  - 同步更新源码契约测试：
    - 新增 mouse 事件“直接驱动 phase”契约
    - `transitionTo` 契约从 deferred 改为 immediate
    - 更新 release 回调顺序契约
  - 运行时测试断言同步为“事件内立即生效”时序
- 验收标准：
  - 功能无回退，且无额外内存分配状态对象
  - 文档记录并验证时序变化对外部行为的影响（尤其点击、hover 音效、drag begin/end）
  - `ctest --test-dir build --output-on-failure -j4` 通过（运行时测试在无 SDL video 的环境下为 `GTEST_SKIP`，不影响结果）

### UIR-031（P0）删除 `UIState` 遗留实现
- 目标：完成架构收口。
- 主要改动：
  - 删除 `src/engine/ui/state/*`
  - 删除 `setState/setNextState/getState` 及引用
  - 更新构建脚本/CMake
- 验收标准：
  - 全仓通过编译与测试
  - 无对 `state/` 路径的 include

### UIR-032（P1）迁移控件初始化路径
- 目标：去掉控件构造时对旧状态类的依赖。
- 主要改动：
  - `UIButton`、`UIItemSlot` 初始化改为 phase 初始化
- 验收标准：
  - 按钮与槽位交互行为保持一致

---

## Iteration 4：事件回调机制收敛（参考 Claude 建议，分步做）

### UIR-040（P1）补 `ClickBehavior` 与 `onStateChanged` 钩子
- 目标：增强 behavior 作为统一扩展层的表达能力。
- 主要改动：
  - 新增 `behavior/click_behavior.h`
  - `InteractionBehavior` 增加可选 `onStateChanged(UIInteractive&, old, now)`
- 验收标准：
  - 可不改业务逻辑即挂载点击/状态行为

### UIR-041（P1）将 `UIButton` 回调路径迁移到 behavior（兼容期）
- 目标：减少“虚函数 + behavior”双轨并存。
- 主要改动：
  - `UIButton::create()` 内部使用 behavior 绑定 click/hover
  - `clicked()/hover_enter()/hover_leave()` 保留一版兼容
- 验收标准：
  - `UIButton` 外部 API 不破坏

### UIR-042（P2）移除 `UIInteractive` 业务型虚回调
- 目标：交互层只保留状态与事件分发，不承载业务行为接口。
- 主要改动：
  - 删除或废弃 `clicked()/hover_enter()/hover_leave()`
- 验收标准：
  - 全部调用方迁移完成

---

## Iteration 5：状态与视觉模型收口 + 低风险优化（仍属于交互域）

### UIR-050（P1）收敛 `UIButtonVisualState` 与通用交互状态
- 目标：消除按钮的双重状态来源。
- 主要改动：
  - 明确 `Disabled` 的统一来源（建议由 `InteractionPhase` 主导）
  - `UIButton` 仅保留必要视觉映射层
- 验收标准：
  - `UIButton` 不再维护重复状态真相

### UIR-051（P2）命中测试去 RTTI 热点
- 目标：降低 `findInteractiveAt()` 递归命中成本。
- 主要改动：
  - `dynamic_cast` 改为 `asInteractive()` 或类型标志
- 验收标准：
  - 行为一致，性能不回退

---

## Backlog 总表（执行顺序）

| ID | 标题 | 优先级 | 预计工作量 | 依赖 |
|---|---|---|---|---|
| UIR-000 | 建立 UI 回归基线 | P0 | 0.5d | - |
| UIR-001 | 补最小自动化验证入口 | P0 | 1d | UIR-000 |
| UIR-010 | 修复同帧移入按下丢点击 | P0 | 0.5d | UIR-001 |
| UIR-011 | 补齐 onReleased 与取消路径 | P1 | 1d | UIR-001 |
| UIR-012 | 统一 setEnabled 语义 | P1 | 1d | UIR-010 |
| UIR-020 | 引入 InteractionPhase（并行） | P0 | 0.5d | UIR-001 |
| UIR-021 | transitionTo + 旧状态委托 | P0 | 1d | UIR-020 |
| UIR-022 | 状态迁移测试补齐 | P1 | 1d | UIR-021 |
| UIR-030 | 切到 phase 主路径 | P0 | 1d | UIR-022 |
| UIR-031 | 删除 state/ 遗留 | P0 | 0.5d | UIR-030 |
| UIR-032 | 控件初始化迁移 | P1 | 0.5d | UIR-031 |
| UIR-040 | ClickBehavior + onStateChanged | P1 | 1d | UIR-031 |
| UIR-041 | UIButton 回调迁移到 behavior | P1 | 1d | UIR-040 |
| UIR-042 | 移除业务型虚回调 | P2 | 0.5d | UIR-041 |
| UIR-050 | 收敛 UIButtonVisualState | P1 | 1d | UIR-031 |
| UIR-051 | 命中测试去 RTTI | P2 | 0.5d | UIR-031 |

---

## 每轮必跑回归（最小集合）
1. Pause 菜单：按钮 hover/press/click + disabled 显示正确。
2. SaveSlot：可用/不可用槽位点击语义正确。
3. Inventory/Hotbar：拖拽开始/更新/结束 + tooltip hover。
4. 模态遮罩（`UIInputBlocker`）下，底层点击被阻断。
5. 拖拽中隐藏 UI 或清空元素，不出现悬挂按下状态。

## 风险与回滚策略
- 风险 R1：状态迁移导致输入边界行为回退。
  - 缓解：UIR-020/021 阶段先并行，再切主路径。
- 风险 R2：行为机制收敛时改动面过大。
  - 缓解：UIR-040/041/042 分三步，保留兼容期。
- 风险 R3：删除 `state/` 后难以快速恢复。
  - 缓解：在 UIR-030 保留短期兼容开关，并在下一迭代再物理删除文件。

## 已移出本 Backlog（另立专项）
- 布局测量/排布两阶段（原 UIR-052）已移出“UI 交互重构”范围，改由 `plans/ui-layout-refactor-backlog.md` 跟踪，避免交互重构范围膨胀。
