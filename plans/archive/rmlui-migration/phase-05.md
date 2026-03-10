### Phase 5: 战斗 UI

**目标**：迁移 `BattleScene` 的现有原型 UI 到 RmlUi，保持当前显示结构、按钮布局、输入阻断行为和战斗状态机逻辑不变；本阶段只替换 UI 层，不扩展技能/物品选择功能。

---

#### Step 5.0: 范围收敛与现状对齐

当前 `BattleScene` 旧版 UI 实际包含以下结构：

- 全屏半透明 dim overlay
- 全屏输入阻断层（防止点击穿透到底层场景）
- 居中战斗面板
- 文本区：标题 / 回合信息 / 单位汇总 / 行动结果
- 固定 2x3 操作按钮：`Attack / Skill / Item / Guard / Escape / End Turn`

本 Phase 必须 **尽量保持这些显示内容与交互行为一致**，不要在迁移过程中顺手改成全新 battle HUD。

明确不在本阶段处理：

- 技能选择窗口
- 物品选择窗口
- 单位详细状态卡 / 头像 / 行动条等扩展 UI
- 战斗状态机、行动解析、结算逻辑重构

说明：当前 `Skill` / `Item` 仍使用默认占位 ID 提交战斗动作，这一行为保持不变。

#### Step 5.1: 新建 RmlUi 文档

**新建** `ui/rmlui/scenes/battle.rml` + `battle.rcss`

建议结构：

- `#battle-overlay`：全屏 dim overlay
- `#battle-panel`：居中主面板
- `#battle-title`
- `#battle-turn`
- `#battle-units`
- `#battle-result`
- `#battle-actions`
  - `#battle-action-attack`
  - `#battle-action-skill`
  - `#battle-action-item`
  - `#battle-action-guard`
  - `#battle-action-escape`
  - `#battle-action-end-turn`

要求：

- 保持旧版“居中面板 + 两行按钮”的视觉结构
- `Attack` 保持主按钮风格，其余 5 个按钮保持次按钮风格
- 按钮 **始终占位显示**，不要使用 `data-if` 在禁用时隐藏按钮
- 禁用态通过 class / attr 绑定控制，避免布局抖动
- `#battle-units` 需要显式处理长文本换行或裁剪，避免单位汇总文本溢出面板
- 面板、间距、按钮尺寸尽量向旧版原型靠拢，参考旧实现中的精确数值：
  - panel: `560 x 320dp`
  - panel padding: `20dp`
  - panel background: `rgba(0,0,0,0.78)`
  - dim overlay: `rgba(0,0,0,0.60)`
  - button: `160 x 36dp`
  - button gap-x: `12dp`
  - turn y: `42dp`
  - units y: `78dp`
  - result y: `118dp`
  - button row 1 y: `180dp`
  - button row 2 y: `224dp`

#### Step 5.2: BattleScene 内部改为 RmlUi 场景文档驱动

**修改** `src/game/scene/battle_scene.h/cpp`

实现策略：

- 删除旧 `UIPanel` / `UILabel` / `UIButton` / `UIInputBlocker` 布局构建逻辑
- 删除 `initUI()` 中旧 UI 容器创建：
  - `ui_manager_ = std::make_unique<engine::ui::UIManager>(...)`
  - `buildLayout()`
- 改为：
  - `loadRmlDocument("ui/rmlui/scenes/battle.rml")`
  - 通过 data model 驱动文本与按钮状态
  - 通过 Rml 事件桥接按钮点击到现有 `queueXXXAction()`
- `BattleScene` 继续保留当前状态机：
  - `WaitingForInput`
  - `ExecutingAction`
  - `AnimatingResult`
  - `CheckVictory`
  - `NextTurn`
  - `BattleEnd`
- `refreshView()` 继续负责把 battle session 状态投影成 UI 文本与按钮状态

建议 data model 字段至少包含：

- `turn_text`（`Rml::String`）
- `units_text`（`Rml::String`）
- `result_text`（`Rml::String`）
- `actions_enabled`（`bool`）

如果实现时发现单个总开关不足以支撑按钮表现，也可以拆为：

- `can_attack`
- `can_skill`
- `can_item`
- `can_guard`
- `can_escape`
- `can_end_turn`

但前提仍是：**只表达现有 enable/disable 行为，不引入新的命令可用性规则。**

#### Step 5.3: 事件桥接与交互约束

事件桥接方式明确采用 **`RmlEventBridge + data-command`**，与已完成的 Phase 3 菜单场景保持一致。

示例：

- RML：`<button class="tf-button-primary" data-command="attack">Attack</button>`
- C++：`event_bridge_.on("attack", [this](Rml::Event&) { queueAttackAction(); });`
- 注册：`event_bridge_.registerTo(document_, "click");`

按钮点击事件映射回现有方法：

- `attack` → `queueAttackAction()`
- `skill` → `queueSkillAction()`
- `item` → `queueItemAction()`
- `guard` → `queueGuardAction()`
- `escape` → `queueEscapeAction()`
- `end_turn` → `queueEndTurnAction()`

要求：

- 禁用态按钮应当不可触发战斗命令
- 即便 RCSS/DOM 侧遗漏禁用样式，C++ 侧仍保留现有状态机保护，避免非法提交
- overlay 文档应阻断底层场景交互，等价替代旧 `UIInputBlocker`

#### Step 5.4: 清理顺序与生命周期

BattleScene 迁移后需要补齐 RmlUi 场景资源生命周期：

- 注册 data model
- 绑定按钮事件
- 新增 `clean()`，并与现有 Scene 基类行为协同：
  1. 移除事件监听 / 解绑回调
  2. 调用 `Scene::clean()`（基类内部已执行 `unloadAllRmlDocuments()`）
  3. 将 `document_` 置空
  4. 销毁 `data_bridge_`
- 新增析构函数，用于处理未走 `clean()` 路径时的防御性清理

要求：

- 反复进入/退出战斗场景不残留旧文档
- 不因文档卸载过程中的 blur / click 派发产生悬空访问
- 不重复创建旧 `UIManager` 容器

#### Step 5.5: 本阶段不做的旧代码清理

本 Phase 聚焦“BattleScene UI 迁移可用”，不顺带推进以下内容：

- 技能/物品二级菜单的设计与实现
- Battle UI 视觉重设计
- BattleScene 外部接口改名
- 旧 UI 框架全面删除（留待 Phase 8 统一收尾）

---

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/scenes/battle.rml` |
| 新建 | `ui/rmlui/scenes/battle.rcss` |
| 修改 | `src/game/scene/battle_scene.h` |
| 修改 | `src/game/scene/battle_scene.cpp` |

**验证**：
- 进入战斗场景后，dim overlay 与中心面板显示正常
- 战斗 UI 打开时可阻断底层场景点击
- 回合信息 / 单位汇总 / 结果文本刷新正确
- 六个按钮布局与旧版原型一致，不因禁用态发生重排
- 按钮禁用时不可点击，启用时能正确触发原有命令
- `Skill` / `Item` 仍沿用现有默认占位逻辑，不产生行为回归
- 战斗流程可完整跑通到胜利 / 失败 / 逃跑结束
- 反复进入/退出 BattleScene，不出现残留文档或崩溃

---
