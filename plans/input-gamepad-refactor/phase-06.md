### Phase 6: 游戏内提示条显隐 + 菜单 hover/focus 同步

**目标**：解决两类输入体验问题：

- `GameScene` 左下角输入提示条过大且长期占位，改为**更紧凑**并支持**独立快捷键切换显隐**
- `TitleScene` / 菜单 scene 中，当前按钮的 `:focus` 外框与鼠标悬浮状态脱节；改为**鼠标 hover 即同步焦点**，让鼠标、键盘、手柄共用同一“当前激活项”

**前置**：Phase 1~5 已完成，`menu_*` 语义动作、`InputContext`、`UINavigationController` 与 RmlUI 默认焦点能力均已稳定可用。

---

#### 设计要点

**问题 1：游戏内提示条显隐**

- 当前左下角输入显示来自 `GameScene` 的 `game_overlay.rml`，不是 ImGui debug panel。
- 本阶段不把它做成设置菜单项，而是采用**游戏内独立快捷键**方案：
  - 新增语义动作：`toggle_prompt_bar`
  - 默认绑定：`F1`
  - 仅加入 `Gameplay` context 白名单
- `GameScene` 新增运行时布尔值 `show_prompt_bar_`，默认 `true`
- `show_prompt_bar_` 只控制左下角 prompt bar，本阶段**不影响右上角菜单按钮**
- 该状态只在运行时生效，本阶段**不持久化到配置/存档**

**问题 2：菜单 hover/focus 脱节**

- 当前菜单按钮的外框来自 `:focus`，鼠标悬浮只触发 `:hover` 样式，因此会出现：
  - 默认焦点停在旧按钮
  - 鼠标移到新按钮时视觉不统一
  - 上下键继续从旧焦点开始导航
- 本阶段统一规则为：
  - **鼠标 hover 到可用按钮时，立即把 Rml focus 同步到该按钮**
  - **键盘/手柄导航继续基于当前 focus**
  - **`hover` 与 `focus` 使用同一套高亮视觉**
- 同步后，鼠标、键盘、手柄共用同一个“当前激活项”，避免三套状态并存

---

#### 实现范围

**本阶段覆盖**

- `GameScene` 的左下角 prompt bar
- `TitleScene`
- `PauseMenuScene`
- `SaveSlotSelectScene`

**本阶段暂不覆盖**

- `RestDialogScene`
- `BattleScene`
- `InventoryUI` / `HotbarUI`
- 提示条显隐状态持久化

> `RestDialogScene` / `BattleScene` 如需同样的 hover/focus 同步，可在本阶段模式验证稳定后按相同方案补齐，但不阻塞当前交付。

---

#### 方案选择

**推荐方案：动作化 prompt 开关 + scene 内 hover/focus 同步**

- prompt bar 显隐通过新的语义动作 `toggle_prompt_bar` 驱动，而不是硬编码 SDL scancode 分支
- hover/focus 同步先做在具体 scene 内，而不是做成全局 RmlUI 规则
- hover/focus 同步不复用现有 `RmlEventBridge`
  - `RmlEventBridge` 是围绕 `data-command + click` 设计的
  - 本阶段改为新增一个轻量 `HoverFocusSyncListener`，实现 `Rml::EventListener`，专门处理 `mouseover -> focus`

**为什么不用更激进的全局方案**

- 把“hover 即 focus”塞进 `RmlUILayer` 全局事件处理中，容易误伤：
  - 物品栏 slot
  - 快捷栏 slot
  - 后续表单 / 输入框 / 可拖拽控件
- 当前用户问题集中在标题/菜单按钮，scene 内显式接入更稳妥，也更容易回归测试

---

#### 需要新增的文件

- `src/engine/ui/rmlui/hover_focus_sync_listener.h`
- `src/engine/ui/rmlui/hover_focus_sync_listener.cpp`

---

#### 需要修改的文件

- `config/input.json`
- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `src/CMakeLists.txt`
- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- `ui/rmlui/hud/game_overlay.rml`
- `ui/rmlui/hud/game_overlay.rcss`
- `src/game/scene/title_scene.h`
- `src/game/scene/title_scene.cpp`
- `src/game/scene/pause_menu_scene.h`
- `src/game/scene/pause_menu_scene.cpp`
- `src/game/scene/save_slot_select_scene.h`
- `src/game/scene/save_slot_select_scene.cpp`
- `ui/rmlui/theme/menu_widgets.rcss`
- `tests/CMakeLists.txt`
- `tests/engine/input/input_context_test.cpp`
- `tests/game/rml_menu_navigation_style_test.cpp`
- （如需要）新增一个 game 层结构性测试，验证场景已接入 hover/focus 同步

---

#### Step 6.1: 为游戏内提示条增加独立切换动作

- 在 `config/input.json` 与 `defaultMappings()` 中新增：
  - `toggle_prompt_bar: ["F1"]`
- 在 `Gameplay` context 白名单中加入 `toggle_prompt_bar`
- 不把该动作加入 `Menu / Dialogue / Battle` context，避免在菜单里误触
- `GameScene` 绑定 `onAction("toggle_prompt_bar", PRESSED)`：
  - 切换 `show_prompt_bar_`
  - 标记 overlay data model 脏字段

约束：

- 该动作是**游戏内 HUD 控制**，不参与 RmlUI 菜单导航
- 本阶段不提供 gamepad 默认绑定；先满足“独立快捷键切换”的明确诉求

---

#### Step 6.2: 收紧 prompt bar 视觉体积

- 在 `game_overlay.rml` 中明确采用 `data-if="show_prompt_bar"` 包裹 prompt bar 区域
  - 本阶段不采用手动 `SetProperty("display", ...)`
  - 也不额外引入 class 切换方案
  - 由于该开关只会被用户低频手动切换，`data-if` 的 DOM 重建开销可以接受
- 在 `game_overlay.rcss` 中压缩以下尺寸：
  - `padding`
  - item 间距
  - label/value 字号或 line-height
- 保留当前四组提示（`Use / Alt / Bag / Menu`），只做**减重**，不改信息结构
- 右上角菜单按钮保持常驻，不随 `show_prompt_bar_` 一起隐藏

完成标准：

- 默认显示时比当前明显更紧凑
- 关闭后左下角不再占位、不影响其他 HUD 观察
- 切换时不需要重建文档，只更新 data binding

---

#### Step 6.3: 菜单场景接入 hover -> focus 同步

不直接复用 `RmlEventBridge`。改为新增共享小工具：

```cpp
class HoverFocusSyncListener final : public Rml::EventListener {
public:
    // 持有 RmlUILayer 指针与可选过滤谓词
    void ProcessEvent(Rml::Event& event) override;
};
```

建议放在 `src/engine/ui/rmlui/hover_focus_sync_listener.*`，由各 scene 各自持有一个实例，再注册到自己的文档上。

每个目标 scene 在文档根上监听 `mouseover`：

- 从 `event.GetTargetElement()` 开始遍历祖先链
- 找到最近的可聚焦按钮/控件元素
- 若按钮未禁用，则调用 `RmlUILayer::focusElement(...)`

监听器职责约束：

- 只处理菜单按钮类元素，不扩展为全局任意控件 hover 规则
- 默认忽略 `disabled` / `data-attrif-disabled` 已落成的元素
- 允许 scene 注入一个轻量过滤谓词，处理个别场景边界条件

生命周期约束：

- 通过 `AddEventListener("mouseover", listener)` 注册的监听器，必须在 `clean()` 中于 `unloadAllRmlDocuments()` **之前**配对执行 `RemoveEventListener("mouseover", listener)`
- 这是项目的 RmlUI 硬性约束，不能依赖文档析构自动清理

推荐接入方式：

- `TitleScene`：同步 `.title-button` 与右上角 `title-menu-button`
- `PauseMenuScene`：同步主操作按钮与音量/速度调节图标按钮
- `SaveSlotSelectScene`：同步 slot 按钮、`Back` 按钮、confirm modal 中的 `OK/Cancel`

行为约束：

- **只在 mouseover 时同步，不在 mouseout 时回退旧焦点**
- 焦点应停留在“最近一次鼠标指向的可用按钮”上
- 这样鼠标移开后，键盘/手柄上下键仍从最新 hover 项继续导航

---

#### Step 6.4: 统一 `:hover` 与 `:focus` 的按钮视觉

- 在 `ui/rmlui/theme/menu_widgets.rcss` 中统一以下控件的高亮规则：
  - `.tf-button-primary`
  - `.tf-button-secondary`
  - `.tf-icon-button`
- 原则是：
  - `:hover` 与 `:focus` 使用同样的高亮色/描边
  - `:active` 继续保留按下态
- 不再让 `hover` 只改文字、`focus` 只画外框，避免双状态并存

完成后应满足：

- 鼠标悬浮哪个按钮，哪个按钮立刻表现为当前激活项
- 上下键切换到哪个按钮，哪个按钮也呈现同样的当前激活态

---

#### Step 6.5: 保持现有默认焦点与 modal 焦点恢复语义

- `TitleScene` / `PauseMenuScene` / `SaveSlotSelectScene` 当前已有默认焦点逻辑，Phase 6 不重写
- hover/focus 同步是在其基础上做增量增强：
  - 初次进入 scene 仍由默认焦点决定首个激活项
  - 鼠标 hover 后焦点迁移到 hover 按钮
  - `SaveSlotSelectScene` confirm modal 继续沿用：
    - 弹出后聚焦 `OK`
    - 关闭后恢复到 `focus_before_confirm_` 或默认焦点
- `SaveSlotSelectScene` 需要补一个 modal 守卫：
  - 当 confirm modal 打开时，hover listener **只允许** modal 内部的 `OK / Cancel` 按钮触发 focus 同步
  - 底层 slot grid / `Back` 按钮即使仍在 DOM 中，也不得把焦点从 modal 拉走
  - 建议通过 `HoverFocusSyncListener` 的 scene 过滤谓词实现，而不是在 listener 内硬编码 scene 名称

---

#### 测试用例

| 测试用例 | 说明 |
|---------|------|
| `TogglePromptBarActionRespectsContextFilter` | 行为测试：`toggle_prompt_bar` 在 `Gameplay` context 下能触发 callback，在 `Menu` context 下被过滤 |
| `GameOverlaySupportsPromptBarVisibilityBinding` | overlay RML 已包含 prompt bar 显隐绑定 |
| `MenuWidgetsUseUnifiedHoverAndFocusHighlight` | 菜单按钮样式中 `:hover` / `:focus` 视觉规则保持一致 |
| `TitleSceneRegistersHoverFocusSync` | 标题场景接入 mouseover -> focus 同步 |
| `PauseMenuSceneRegistersHoverFocusSync` | 暂停菜单接入 mouseover -> focus 同步 |
| `SaveSlotSelectSceneRegistersHoverFocusSync` | 存档选择接入 mouseover -> focus 同步，并不破坏 confirm modal 的焦点恢复 |
| `SaveSlotSelectSceneHoverSyncGuardsModalFocus` | 结构性测试：confirm modal 打开时，hover 同步不会把焦点拉回底层 slot/back 按钮 |
| `SceneHoverFocusSyncRemovesListenersBeforeUnload` | 结构性测试：新增的 `mouseover` 监听器会在卸载文档前显式移除 |

说明：

- 如果 headless 环境下难以稳定做运行时 Rml 行为测试，可退一步做**结构性测试**：
  - 检查 scene 源码中已注册 `mouseover`
  - 检查同步逻辑会调用 `focusElement(...)`
  - 检查 RML/RCSS 中存在对应显隐与高亮规则
- `TogglePromptBarActionRespectsContextFilter` 应优先做成真实行为测试，可复用现有 `createManager() + pushKey()` 模式，而不是只做文本扫描

---

#### 待办清单

- [ ] 新增 `toggle_prompt_bar` 动作与默认绑定
- [ ] 将 `toggle_prompt_bar` 纳入 `Gameplay` context 白名单
- [ ] 为 `GameScene` prompt bar 增加运行时显隐绑定
- [ ] 压缩左下角 prompt bar 的视觉体积
- [ ] 新增共享 `HoverFocusSyncListener` 工具类
- [ ] 在 `TitleScene` 中接入 hover -> focus 同步
- [ ] 在 `PauseMenuScene` 中接入 hover -> focus 同步
- [ ] 在 `SaveSlotSelectScene` 中接入 hover -> focus 同步
- [ ] 为 `SaveSlotSelectScene` 增加 confirm modal 打开时的 hover 守卫
- [ ] 统一菜单按钮 `:hover` / `:focus` 样式
- [ ] 确保所有新增 `mouseover` 监听器在卸载文档前移除
- [ ] 补齐输入与菜单结构性测试

#### 完成标准

- 游戏中可通过独立快捷键切换左下角输入提示条显示
- prompt bar 默认更紧凑，不再显得“太大且碍事”
- 标题与菜单场景中，鼠标 hover、键盘方向键、手柄导航共享同一个当前激活按钮
- 上下键导航从最近 hover 的按钮继续，而不是从旧默认焦点继续
- 不引入新的全局输入分支，也不破坏现有 `menu_*` 导航与 modal 焦点恢复
