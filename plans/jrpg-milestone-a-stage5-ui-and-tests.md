# Milestone A / Stage 5: Battle RML/RCSS 收尾与测试补强计划

## Context

Stage 1-4 已完成战斗菜单的核心可玩闭环：

- `BattleScene` 已有 `MainMenu / SkillList / ItemList / TargetSelect` 菜单子状态机
- `menu_up/down/left/right/confirm/cancel` 已由 `BattleScene` 场景级处理，并通过 `Focus(true)` 同步 RmlUi 焦点
- `SkillList` 已从 `BattleUnit::skill_ids` 与 `RpgCatalog` 构建真实技能候选
- `ItemList` 已从 `BattleSession::itemStocks()` 与 `ItemCatalog::battle_use_` 构建真实战斗物品候选
- 战斗物品消耗结果已通过 `BattleEndedEvent::remaining_item_stocks` 写回真实背包
- `Attack / Skill / Item` 已通过 `continueDraftAfterScopeSelected()` 与 `submitDraftAction()` 进入统一目标选择 / 提交流程
- `TargetEntryViewModel` 已包含 `enabled / is_ally / is_dead`，并绑定到 `battle.rml`
- Stage 4 明确没有改 `result_text`，完整结果文案留给 Stage 5

当前还需要收尾的地方：

- `battle.rml` 仍保留 `"Battle Prototype"` 标题
- `battle.rcss` 只有基础布局，尚未为 `is-ally / is-dead / disabled` 目标状态补专门样式
- `Skill / Item` 的结果文案仍较粗，例如 item 只显示 `"Result: Item used"`
- `battle.rml / battle.rcss` 的 UI 结构已经可用，但缺少专门的回归测试来防止后续误删导航类、目标状态绑定或回退到占位文案
- Battle 上下文仍会抑制菜单方向键转发给 RmlUi，因此 Stage 5 不能把导航方案改回依赖 RmlUi 原生方向键事件

Stage 5 的核心是让战斗菜单 UI 达到“后续可持续迭代”的稳定状态，而不是继续扩大战斗规则范围。

## 范围

### 本阶段包含

- `battle.rml` 文案与结构收尾
- `battle.rcss` 布局、空态、禁用态、目标 ally/dead 状态样式收尾
- 保持鼠标点击与键盘/手柄 `menu_*` 输入的统一路径
- 最小 result text 改善，覆盖伤害、恢复、miss、KO 与 item recover 的关键反馈
- Battle RML/RCSS 静态 smoke test 补强
- `BattleScene` result text helper 的轻量源码 smoke / 单元测试补强
- 运行现有 BattleScene / resolver / catalog / RmlUi style 相关测试

### 本阶段不包含

- 敌方 AI
- 战斗奖励、掉落、金币结算
- 战斗动画、粒子、音效
- 完整 battle log 或逐目标滚动日志
- 技能/物品描述详情面板
- 伤害预测、弱点提示、行动顺序条
- 多目标逐个选择 UI
- 复活类目标规则
- 新的 RmlUi 引擎层输入策略
- 大规模拆分 `BattleScene` 文件

## 设计决策

### 1. 继续使用 BattleScene 自主管理导航

Stage 5 不修改 `InputManager::shouldSuppressRmlUiKeyboardEvent()` 的 Battle 上下文策略。

继续保持：

- 鼠标点击走 RML `data-event-click`
- 键盘 / 手柄方向、确认、取消走场景级 `menu_*` action
- `BattleScene` 自己维护 `main_action_cursor_ / list_entry_cursor_ / target_entry_cursor_`
- 焦点同步继续由 `syncMenuFocus()` 调用 `Focus(true)`

`nav.rcss`、`tf-nav-root`、`tf-nav-auto` 仍然要保留，但它们在 Battle 菜单中主要用于：

- 元素可聚焦
- 统一 hover/focus 样式
- 鼠标与程序化焦点的视觉反馈

不要把 Stage 5 做成“依赖 RmlUi 原生方向键导航”的方案。

### 2. UI 收尾只使用现有 ViewModel 字段

Stage 5 默认不新增新的 Battle menu ViewModel 字段。

继续复用：

- `MainActionViewModel`
- `ListEntryViewModel`
- `TargetEntryViewModel`

理由：

- Stage 1-4 的数据字段已经足够支持主菜单、列表、目标与 disabled 状态
- Stage 5 的目标是 UI 稳定化，不是技能/物品详情系统
- 若此阶段新增 icon、描述、消耗详情面板，会把范围推向 Stage B 的完整战斗 HUD

允许的轻量例外：

- 为 result text 抽一个纯格式化 helper，例如 `formatActionResultText(...)`
- 仅当 RCSS 无法表达当前状态时，才考虑补一个最小 bool/string 字段；默认不需要

### 3. 保持单屏、低高度战斗菜单

当前逻辑分辨率为 640x360dp，`battle-panel` 为 560x320dp。Stage 5 可以调整内部排版，但必须保持：

- 主要菜单区域仍在首屏可见
- 主菜单、列表菜单、目标菜单不产生垂直溢出
- hint / back hint 不挤压列表按钮
- 不引入 hero、营销式文案、卡片套卡片、渐变 blob 等无关视觉结构
- 文本在 520dp 内容宽度内能容纳，必要时收短文案，而不是靠超大字体或 viewport 缩放

若需要更大信息量，优先压缩文案或留到后续 battle HUD 阶段，不在 Stage 5 强塞详情面板。

### 4. 目标状态需要有明确视觉差异

`battle.rml` 已有：

- `data-class-disabled="!target.enabled"`
- `data-attrif-disabled="!target.enabled"`
- `data-class-is-ally="target.is_ally"`
- `data-class-is-dead="target.is_dead"`

Stage 5 应在 `battle.rcss` 中补齐对应样式：

- ally target 和 enemy target 至少在文字颜色或副色上可区分
- dead target 需要有更低对比度，并与 disabled 状态一致
- disabled target 在 hover/focus 下不能看起来像可确认项
- 空态 `#battle-target-empty` 与 `#battle-list-empty` 使用一致的低强调样式

不要仅依赖 resolver 拒绝 dead target；UI 应该让玩家能看出不可选。

### 5. Result text 做最小可读化，不做完整 battle log

Stage 5 可以改善 `BattleScene::refreshView()` 里的结果文案，但只做最小版本：

- Attack: 保持伤害与 KO 反馈
- Skill:
  - `missed` 时显示 missed
  - `damage > 0` 时显示伤害
  - `hp_recovered > 0` / `mp_recovered > 0` 时显示恢复
  - 如果 `damage == 0` 且存在恢复值，不显示 `"dealt 0 dmg"`，只显示恢复反馈
  - `states_added` 仍可只显示第一个状态，避免展开复杂状态日志
  - 没有明显数值变化时显示 applied
- Item:
  - `hp_recovered > 0` / `mp_recovered > 0` 时显示恢复
  - 否则保留 `"Result: Item used"` 或等价短文案

推荐抽出一个 `.cpp` 内的匿名 namespace helper，避免 `refreshView()` 继续膨胀，也避免为了 UI 文案测试暴露新的 public API：

```cpp
std::string formatActionResultText(const game::battle::BattleActionResult& result);
```

该 helper 不负责查 catalog 名称，不做多目标逐条描述，不做滚动日志。

Stage 5 默认只通过 smoke test 检查 result text 的恢复值路径，不直接单测该 helper。若后续确实需要直接单测 formatter，再单独把它提成可测试的小型 header/free function；不要使用 `BattleScene` private static 作为“可直接单测”方案，因为普通测试文件无法直接访问 private static 成员。

### 6. RML/RCSS 必须遵守项目 RmlUi 约束

修改 `battle.rml / battle.rcss` 时注意：

- 保留 `../theme/reset.rcss`
- 保留 `../theme/nav.rcss`
- 交互元素继续使用 `tf-nav-auto`
- 不使用 `border: ... solid ...`
- 不使用 bitmap 字体 italic
- 绝对定位元素需要显式 `width / height`
- 不依赖标准 CSS 的 `left + right` 或 `top + bottom` 拉伸
- 若新增 `div` / `span` 的布局行为，要显式设置 `display`

### 7. 测试以现有静态 smoke 风格为主

Stage 5 不要求新增完整 RmlUi runtime 交互测试。优先延续现有测试形态：

- `BattleSceneSmokeTest` 检查 `battle.rml / battle.rcss / battle_scene.cpp` 的关键接线
- `RmlMenuNavigationStyleTest` 只在修改共享导航 / button 样式时扩展
- 领域逻辑已有 resolver / session 测试，Stage 5 默认不为 result text helper 暴露新 API，只做 smoke 级别断言

测试目标是防止以下回归：

- `Battle Prototype` 或 Stage 4 占位文案重新出现
- Battle 按钮丢失 `tf-nav-auto`
- `target_entries` 丢失 `is_ally / is_dead / disabled` class 绑定
- `battle.rcss` 没有 target ally/dead/disabled 状态样式
- result text 对 `hp_recovered / mp_recovered` 完全没有反馈
- RCSS 引入 `solid` border 或 `font-style: italic`

## 实现步骤

### Step 1: RML 文案与结构收尾

修改：

- `ui/rmlui/scenes/battle.rml`

要点：

- 将 `"Battle Prototype"` 改成正式短标题，例如 `"Battle"`
- 保留 `tf-screen-root tf-nav-root`
- 保留 `data-model="battle_scene"`
- 保留 `data-for` / `data-if` 的 Stage 1-4 绑定结构
- 保留主菜单、列表、目标按钮的稳定 id：
  - `battle-main-action-{{ action.entry_index }}`
  - `battle-list-entry-{{ entry.entry_index }}`
  - `battle-target-entry-{{ target.entry_index }}`
- 确认所有可交互按钮仍有：
  - `tf-button-secondary`
  - `tf-nav-auto`
  - `data-event-click`
  - disabled 的 class 与 attr 绑定

注意：

- 不要在 Stage 5 为列表添加 icon / description 新字段
- 不要把 `data-if` 与 `data-for` 的 backing vector 清理时序改回同帧清空问题路径

### Step 2: Battle RCSS 视觉状态收尾

修改：

- `ui/rmlui/scenes/battle.rcss`

建议补充：

- `#battle-title` 的正式标题样式
- `.battle-list-entry.disabled`
- `.battle-list-entry.disabled .battle-entry-sublabel`
- `.battle-target-entry.is-ally`
- `.battle-target-entry.is-dead`
- `.battle-target-entry.disabled`
- `.battle-target-entry.is-dead:focus`
- `#battle-list-empty / #battle-target-empty` 的一致空态样式

要点：

- 目标 ally/dead/disabled 视觉上能分辨
- dead target 和 disabled target 不应在 focus 时变成高亮可确认状态
- 保持 560x320 panel 与 520dp 内容宽度内的可读性
- 不使用 `solid`、italic 或未显式尺寸的绝对定位拉伸

### Step 3: 最小 result text helper

修改：

- `src/game/scene/battle_scene.cpp`
- `src/game/scene/battle_scene.h` 仅在 helper 需要声明时修改

推荐方案：

- 将 `last_action_result_` 的文案拼接抽成 `battle_scene.cpp` 内的匿名 namespace helper
- 不在 `battle_scene.h` 为 formatter 新增声明，除非实现阶段明确决定把 formatter 提成可复用/可直接单测的小型 free function
- `refreshView()` 只负责选择 outcome / default / last action result，再调用 helper

最低要求：

- `BattleActionType::Skill` 支持显示 `hp_recovered` / `mp_recovered`
- `BattleActionType::Skill` 在 `damage == 0` 且存在恢复值时跳过伤害文案，避免显示 `"dealt 0 dmg"`
- `BattleActionType::Item` 支持显示 `hp_recovered` / `mp_recovered`
- 保留 rejected `failure_reason`
- 保留 Attack KO 反馈
- 不引入 catalog name 查找或逐目标日志

### Step 4: UI 静态 smoke test 补强

修改：

- `tests/game/battle/battle_scene_smoke_test.cpp`
- 如共享导航样式被改动，再修改 `tests/game/rml_menu_navigation_style_test.cpp`

建议新增 / 扩展断言：

- `battle.rml` 不包含 `"Battle Prototype"`
- `battle.rml` 包含 `data-class-is-ally="target.is_ally"`
- `battle.rml` 包含 `data-class-is-dead="target.is_dead"`
- `battle.rml` 包含三个菜单层级的 `tf-nav-auto`
- `battle.rcss` 包含 `.battle-target-entry.is-ally`
- `battle.rcss` 包含 `.battle-target-entry.is-dead`
- `battle.rcss` 不包含 `"solid"` 与 `"font-style: italic"`
- `battle_scene.cpp` 包含 `hp_recovered` / `mp_recovered` 的 result text 路径
- `battle_scene.cpp` 不包含 `"Target selection coming in Stage 4"`

Stage 5 默认保持 smoke 级别即可，不为了 formatter 单测改动头文件可见性。若实现阶段主动把 formatter 提成 header free function，再补对应直接单测。

### Step 5: 验证

建议运行：

```bash
ninja -C build/debug tests/game_tests
build/debug/tests/game_tests '--gtest_filter=BattleSceneSmokeTest.*:RmlMenuNavigationStyleTest.*'
build/debug/tests/game_tests
ctest --test-dir build/debug --output-on-failure --quiet
git diff --check
```

若 headless 环境继续输出 OpenGL context / TextureLoader 日志，只要测试不失败，按既有环境噪音处理。

## ToDo

- [x] 清理 `battle.rml` 的原型标题与确认导航/事件绑定
- [x] 补齐 `battle.rcss` 的目标 ally/dead/disabled 视觉状态与空态样式
- [x] 抽出或收敛 result text 拼接逻辑，补最小恢复类结果反馈
- [x] 补强 `BattleSceneSmokeTest` 的 RML/RCSS/result text 回归断言
- [x] 如共享导航样式被改动，补强 `RmlMenuNavigationStyleTest`
- [x] 运行 targeted gtest、完整 `game_tests`、`ctest` 与 `git diff --check`

## 完成标准

- 战斗菜单不再显示 `"Battle Prototype"` 或 Stage 4 占位文案
- `Attack / Skill / Item / TargetSelect` 的交互闭环不被 UI 收尾改动破坏
- 目标列表能区分 ally、enemy、dead、disabled 状态
- Skill / Item 的恢复类结果至少有可读反馈
- Battle 菜单按钮仍可通过鼠标点击与 `menu_*` 输入操作
- RML/RCSS 静态 smoke test 覆盖关键绑定与样式约束
- 相关测试与 `git diff --check` 通过
