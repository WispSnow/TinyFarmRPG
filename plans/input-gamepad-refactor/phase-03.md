### Phase 3: UI 导航与上下文 - Menu 动作 + InputContext

**目标**：为菜单、对话、暂停界面和后续 JRPG UI 建立统一的导航动作与输入上下文边界。

#### 实现思路

- 输入层继续产出语义动作，不直接把手柄 SDL 事件塞给某个 UI 实现。
- 增加 `InputContext`，区分 Gameplay / Dialogue / Menu / Battle 等不同输入域。
- 让自研 UI 和 RmlUI 都消费同一组菜单动作。

#### 需要新增的文件

- `src/engine/input/input_context.h`
- `src/engine/input/input_context.cpp`
- `src/engine/ui/ui_navigation_controller.h`
- `src/engine/ui/ui_navigation_controller.cpp`
- `src/engine/ui/rmlui/rml_navigation_bridge.h`
- `src/engine/ui/rmlui/rml_navigation_bridge.cpp`
- `tests/engine/input/input_context_test.cpp`
- `tests/engine/ui/ui_navigation_controller_test.cpp`

#### 需要修改的文件

- `src/engine/input/input_manager.h`
- `src/engine/input/input_manager.cpp`
- `src/engine/ui/ui_manager.*`
- `src/engine/ui/rmlui/rml_ui_layer.cpp`
- `src/game/scene/pause_menu_scene.cpp`
- 后续对话/商店/菜单场景入口文件

#### Step 3.1: 定义菜单语义动作

- 规范以下动作：
  - `menu_up`
  - `menu_down`
  - `menu_left`
  - `menu_right`
  - `menu_confirm`
  - `menu_cancel`

#### Step 3.2: 引入 InputContext

- 设计可 push/pop 的上下文栈。
- 至少定义：
  - `GameplayContext`
  - `DialogueContext`
  - `MenuContext`
  - `BattleContext`
- 高优先级 context 可屏蔽低优先级动作。

#### Step 3.3: 自研 UI 菜单导航

- 通过 `UINavigationController` 为按钮、列表、面板提供焦点移动和确认/取消逻辑。
- 暂停菜单先作为首个接入点。

#### Step 3.4: RmlUI 导航桥接

- `RmlNavigationBridge` 负责把 `menu_*` 和确认/取消动作翻译成 RmlUI 可消费的导航行为。
- 仍保持输入源语义统一，不绕回原始 SDL 手柄事件。

#### Step 3.5: 场景切换与输入屏蔽

- 暂停菜单、对话和后续战斗菜单进入时切换到对应 context。
- 返回 gameplay 时恢复 `GameplayContext`。

#### 待办清单

- [ ] 定义并接入 `menu_*` 语义动作
- [ ] 实现 `InputContext`
- [ ] 为自研 UI 增加导航控制器
- [ ] 为 RmlUI 增加导航桥接
- [ ] 在暂停菜单中落地首个菜单导航路径
- [ ] 为场景切换补 context 管理
- [ ] 增加上下文与 UI 导航测试

#### 完成标准

- 菜单导航不再依赖鼠标悬停和点击才能操作。
- Gameplay / Menu / Dialogue 的输入边界清晰。
- 自研 UI 与 RmlUI 都能通过统一语义动作响应手柄。
