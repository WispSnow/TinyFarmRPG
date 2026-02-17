# FND-002 审阅意见

## 整体评价

计划结构完整、思路正确。将硬编码顺序收口到 `SystemScheduler`，用 `GameMode` 做调度主键，是正确的方向。以下是具体意见：

## 1. 设计层面的核心问题：两次 transition 检查的建模

当前 `update()` 的控制流不是简单的"gate 开/关"，而是一个**两阶段检查**：

```
remove_entity                     ← 始终执行
─── gate 1: transition 已激活? ─── 是 → transition + light_toggle + UI → return
gameplay 阶段 (time ~ movement)   ← 正常执行
transition + light_toggle          ← 正常执行
─── gate 2: transition 刚触发? ─── 是 → UI → return
post-gameplay (spatial ~ animation)
UI
```

计划中描述的冻结规则（"过渡激活时，仅执行 `remove_entity + transition + light_toggle + ui update`"）只覆盖了 **gate 1**。**gate 2**（本帧触发过渡后跳过 spatial/pickup/interaction/camera/animation）也是关键行为，需要在设计文档中显式建模，否则迁移时容易遗漏。

**建议**: 在实现思路中明确区分两个 gate，并在 T7 测试中分别覆盖。

## 2. `render()` 中也有系统调用

```cpp
void GameScene::render() {
    systems_->ysort_system->update(registry_);
    systems_->render_system->update(registry_, renderer, camera);
    systems_->light_system->update(registry_, renderer);
    // ...
}
```

计划说"不改动渲染管线"，这是合理的。但需确认 scheduler **只管 `update()` 阶段**，`render()` 保持原样。建议在"非目标"小节中显式提一句，避免后续实施者混淆。

## 3. `abort_to_title_` 分支

`update()` 开头有：
```cpp
if (abort_to_title_) {
    Scene::update(delta_time);
    return;
}
```
这是一个"紧急退出"路径，不走任何系统。scheduler 需要决定：这仍然留在 `GameScene` 作为 scheduler 之前的短路？还是作为一个特殊 mode？建议保留在 `GameScene` 侧，不纳入 scheduler，但计划中应提及。

## 4. GameMode 与 SceneStack 的职责边界

计划提到 `PauseOverlay` 作为 GameMode，但当前 PauseMenu 是通过 **SceneStack push** 实现的（`scene_manager.cpp` 只更新栈顶场景）。这意味着 `PauseOverlay` 不需要 scheduler 参与——底层 GameScene 根本不会收到 `update()` 调用。

**建议**：要么将 `PauseOverlay` 从 GameMode 枚举中移除（它靠 SceneStack 机制天然冻结），要么明确说明 `PauseOverlay` mode 仅用于"GameScene 内嵌暂停"（不 push 新 scene）的未来场景。否则会造成概念冗余。

## 5. 系统调用签名不统一

当前系统的 `update()` 签名各不相同：
- `remove_entity_system->update(registry_)`
- `movement_system->update(registry_, delta_time)`
- `time_system->update(delta_time)`
- `day_night_system->update()` （无参）
- `spatial_index_system->update(registry_)`

scheduler 的 step 抽象如何统一这些签名？常见方案：
- **A**: step 存 `std::function<void()>`，在 profile 构建时用 lambda 捕获参数
- **B**: 所有系统统一为 `update(UpdateContext&)` 接口（改动大，不适合本阶段）
- **C**: step 分类型（`StepWithDt`, `StepWithRegistry`, `StepPlain`）

建议在实现步骤 T2 中明确选型，避免实现时临时决策。

## 6. 测试计划的覆盖度

计划列出的 3 个测试方向是对的，但还缺一条：

- T5: `movement -> spatial_index` ✓
- T6: `time -> day_night/light` ✓
- T7: transition gate 冻结 ✓
- **缺失**: `remove_entity` 始终在所有 mode 下最先执行

`remove_entity` 放在最前面是关键不变量（确保已标记删除的实体不污染后续系统），建议加一条测试。

## 7. 现有测试 `game_scene_light_toggle_hook_test` 的处理

计划说"改为基于 scheduler 语义校验"，这是对的。但需注意：这个测试验证的核心语义是 **map_transition 和 light_toggle 始终成对出现，且 light_toggle 紧跟 map_transition**。迁移到 scheduler 后，这个"成对"关系应当在 profile 定义层面被保证，而不是只在测试中检查。

## 8. 文件清单补充

`src/CMakeLists.txt` 需要加入 `system_scheduler.cpp`，计划已列出。但 `system_bundle.cpp` 也在 FND-001 中新增了，确认 CMakeLists 中已包含即可（应该已经包含了）。

## 9. 步骤粒度

步骤 3（迁移 GameScene::update）和步骤 4（mode 切换入口）可以合并——迁移 update 的同时自然就会引入 `scheduler.tick(mode, dt, ctx)` 调用，mode 切换入口也顺带提供了。分成两步反而可能导致中间状态不可编译。

## 总结

| 维度 | 评价 |
|------|------|
| 目标清晰度 | 好 |
| 范围控制 | 好，非目标明确 |
| 基线分析 | 需补充 gate 2 和 abort_to_title_ |
| 设计方案 | 需明确签名统一策略和双 gate 建模 |
| PauseOverlay 定位 | 需澄清与 SceneStack 的关系 |
| 测试覆盖 | 需增加 remove_entity 首位不变量 |
| 步骤合理性 | T3/T4 建议合并 |
| 风险识别 | 到位 |

建议先解决上述第 1、4、5 点再开始实现。
