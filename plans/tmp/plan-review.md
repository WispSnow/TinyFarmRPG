# UIR-010 计划审阅

> 审阅人: Claude
> 日期: 2026-02-17
> 审阅对象: `plans/ui-refactor/UIR-010.md`

## 结论：可执行，有两处需要注意

计划的根因分析、方案选择、方案 B 的拒绝理由均经代码验证正确。改动范围小、回滚路径清晰。以下是具体审阅结果。

---

## 已验证正确的内容

### 根因链条 — 全部准确
- `UINormalState` 确实只 override 了 `onMouseEnter()`，没有 `onMousePressed()` (`ui_normal_state.h:24`, `ui_normal_state.cpp:17-21`)
- `UIState::onMousePressed()` 基类默认实现是空 (`ui_state.h:56`)
- `UIManager::onMousePressed()` 不做 hover 同步，直接 `target->mousePressed()` (`ui_manager.cpp:165,172`)
- `UIInteractive::mousePressed()` 将事件转发到当前 `state_->onMousePressed()` (`ui_interactive.cpp:226`)
- `setNextState` 确实是延迟生效——在 `update()` 中才 `setState(std::move(next_state_))` (`ui_interactive.cpp:157-158`)

### 方案 B 的拒绝理由 — 正确
即使在 `onMousePressed()` 前先调用 `updateHovered(target)`，`mouseEnter()` 触发的是 `setNextState(HoverState)`，当前 state 仍然是 NormalState。后续 `mousePressed()` 到达时 NormalState 仍然不处理 `onMousePressed`。方案 B 不能修复问题。

### 方案 A — 正确且最小
`UINormalState::onMousePressed()` 中 `setNextState(UIPressedState)` 与 `UIHoverState::onMousePressed()` 做法一致 (`ui_hover_state.cpp:27`)。两条路径都只设置 next_state，由 `UIPressedState::enter()` 统一负责视觉和音效 (`ui_pressed_state.cpp:14-15`)。行为一致。

### UIR-001 基线测试 — 已验证存在
`tests/engine/ui/ui_interaction_state_source_test.cpp` 存在，包含 pressed release inside/outside 和 dispatch order 的源码契约测试。

---

## 需要注意的问题

### 1. 存在一个调用顺序差异（非阻塞，但应明确记录）

通过 **Hover 路径按下**:
```
mouseEnter() → state(Normal).onMouseEnter() → setNextState(Hover)
...下一帧 update() → setState(Hover) → HoverState.enter() [视觉=hover, hover_enter()]
...之后某帧
mousePressed() → state(Hover).onMousePressed() → setNextState(Pressed)
```

通过 **Normal 路径同帧按下**（修复后）:
```
mousePressed() → state(Normal).onMousePressed() → setNextState(Pressed)
...下一帧 update() → setState(Pressed) → PressedState.enter() [视觉=pressed, click音效]
```

差异：Normal 路径跳过了 `HoverState::enter()` 中的 `hover_enter()` 回调和 hover 视觉。这是正确行为（同帧按下不应该有一帧的 hover 闪烁），但 `hover_enter()` 回调不被触发这一事实应该在文档或测试中明确记录，避免后续 UIR-040/041 迁移时误认为是 regression。

**建议**：在 W3 的测试中加一条注释说明："Normal→Pressed 路径不经过 HoverState，不触发 hover_enter()，这是预期行为。"

### 2. UIR-001 的测试方式是源码字符串匹配，不是行为测试

`ui_interaction_state_source_test.cpp` 通过读源文件做 `string::find()` 来验证契约。这意味着：
- 它验证的是"代码中存在特定字符串"，不是"运行时行为正确"
- 如果 UIR-010 加的代码恰好不包含测试期望的字符串模式，测试通过但修复无效

对于 W3（补充测试），如果能写一个真正的行为测试（构造 mock context + UIInteractive 实例，调用 mousePressed/mouseReleased，断言 clicked() 被调用）会更可靠。如果当前测试基础设施不支持（UIInteractive 构造需要完整 Context），那继续用源码契约测试也可以接受，但应明确在测试注释中说明这是临时方案，UIR-022 应补真正的行为测试。

---

## 总结

| 项目 | 判定 |
|------|------|
| 根因分析 | 正确 |
| 方案选择 | 正确 |
| 方案 B 拒绝 | 正确 |
| 改动范围 | 合理（3 文件） |
| WBS 分解 | 合理 |
| 回滚策略 | 充分 |
| hover_enter 跳过差异 | 非阻塞，建议在测试中注释说明 |
| 测试方式 | 可接受，但建议标注为临时方案 |

**可以执行。**
