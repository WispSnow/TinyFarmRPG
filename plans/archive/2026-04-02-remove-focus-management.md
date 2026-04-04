# Focus 管理代码移除计划

## 元信息
- 任务ID：`UI-FOCUS-001`
- 任务标题：`移除 RmlUi Focus 管理，简化为纯鼠标交互`
- 优先级：`P1`
- 状态：`Completed（实现与自动化验证完成；人工鼠标回归待补）`
- 计划时间：`2026-04-02` 起
- 完成时间：`2026-04-02`
- 依赖任务：`无`
- 设计原则：`当前阶段仅支持鼠标，移除自定义 focus 管理与 menu_* -> RmlUi 导航桥接；RCSS 中 tab-index/nav-* 规则先保留，但需接受并回归确认鼠标点击后仍可能出现 RmlUi 原生 :focus 样式，必要时后续再收敛到 :hover / :focus-visible`

## 背景

当前项目为支持键盘/手柄 UI 导航，在 C++ 侧构建了一套 focus 管理体系，涉及约 400+ 行专用代码和 6 个场景的调用点。但现阶段仅使用鼠标操作 UI，这套体系增加了不必要的复杂度：

- `HoverFocusSyncListener`：将鼠标 hover 同步为键盘 focus，纯鼠标下无意义
- 延迟焦点队列（`PendingFocusRequest`）：解决布局前设焦点的时序问题，纯鼠标下不需要
- Default focus 策略：文档加载后自动聚焦到指定按钮，仅服务键盘导航
- 各 Scene 中的 focus 保存/恢复逻辑：打开子菜单时记住焦点位置，纯鼠标下不需要

RmlUi 原生自带完整的键盘导航支持（`tab-index` + `nav-*` CSS 属性 + `ProcessKeyDown`），因此当前可以先移除自定义 focus 管理，后续再按“简单菜单先恢复、复杂弹层局部恢复”的策略逐步加回。但需要注意：恢复键盘/手柄导航时，除了加回 C++ 侧输入转发，还要重新建立弹层进入焦点、关闭后焦点恢复/fallback，以及必要时调整 `InputManager::shouldSuppressRmlUiKeyboardEvent()` 的行为。

## 影响范围

### 删除的文件
| 文件 | 说明 |
|------|------|
| `src/engine/ui/rmlui/hover_focus_sync_listener.h` | 整个类删除 |
| `src/engine/ui/rmlui/hover_focus_sync_listener.cpp` | 整个类删除 |

### 修改的引擎层文件
| 文件 | 修改内容 |
|------|----------|
| `src/engine/ui/rmlui/rml_ui_runtime.h` | 移除：`navigateUp/Down/Left/Right`、`confirmFocusedElement`、`getFocusedElement`、`focusElement`、`focusElementById`、`focusFirstEnabledElementByClass`、`queueFocusElement*` 系列、`PendingFocusRequest` 结构体、`clearPendingFocusRequestsForDocument`、`pending_focus_requests_` 成员 |
| `src/engine/ui/rmlui/rml_ui_runtime.cpp` | 移除上述方法的实现；`update()` 中移除延迟焦点队列处理；`applyInteractionPolicy()` 中移除 `Blur()` 调用（保留 `pointer-events: none`） |
| `src/engine/ui/rmlui/rml_document_controller.h` | 移除：`#include hover_focus_sync_listener.h`、`enableHoverFocusSync`、`setDefaultFocusById`、`setDefaultFocusFirstEnabledByClass`、`queueDefaultFocus`、`queueFocusElement*` 系列、`DefaultFocusKind` 枚举、`applyHoverFocusSync`、`clearHoverFocusListener`、`hover_focus_listener_`/`hover_focus_candidate_filter_`/`hover_focus_sync_enabled_`/`default_focus_kind_`/`default_focus_token_` 成员 |
| `src/engine/ui/rmlui/rml_document_controller.cpp` | 移除上述方法的实现；`load()` 简化为只加载文档；`unload()` 移除 focus 相关清理 |
| `src/engine/core/game_app.h` | 移除：`connectMenuNavigationBindings`（返回值部分）、`disconnectMenuNavigationBindings`、`onMenuNavigateUpPressed`/`DownPressed`/`LeftPressed`/`RightPressed`、`onMenuConfirmPressed` |
| `src/engine/core/game_app.cpp` | 移除上述 5 个回调方法的实现（约 588–641 行），以及 connect/disconnect 调用 |

### 修改的构建、测试与文档文件
| 文件 | 修改内容 |
|------|----------|
| `src/CMakeLists.txt` | 从 `target_sources(engine ...)` 移除 `engine/ui/rmlui/hover_focus_sync_listener.cpp` |
| `tests/engine/ui/rml_document_controller_source_test.cpp` | 更新 source test，移除对 focus API 的存在性断言 |
| `tests/game/menu_hover_focus_sync_test.cpp` | 删除或重写 hover-focus sync 相关断言，保留仍适用的 UI 结构/样式断言 |
| `tests/engine/core/game_app_ui_navigation_source_test.cpp` | 删除或重写对 `GameApp -> RmlUiRuntime` 导航桥接的断言 |
| `tests/game/inventory_menu_scene_slot_grid_registration_test.cpp` | 移除对 `queueFocusFirstEnabledElementByClass(\"hb-slot\")` 的断言 |
| `docs/engine/ui_framework.md` | 移除“默认焦点 / hover-focus sync / queueFocus / menu_* -> navigate*”相关说明，改成鼠标优先版本 |
| `docs/testing/ui-regression-checklist.md` | 删除默认焦点、hover-focus sync、键盘/手柄导航等当前不再适用的回归项，改成鼠标-only 检查清单 |

### 修改的游戏层文件
| 文件 | 修改内容 |
|------|----------|
| `src/game/scene/title_scene.cpp` | 移除 `enableHoverFocusSync()` 和 `setDefaultFocusById("title-start-button")` 调用 |
| `src/game/scene/pause_menu_scene.cpp` | 移除 `enableHoverFocusSync()` 和 `setDefaultFocusById("pause-resume-button")` 调用 |
| `src/game/scene/rest_dialog_scene.cpp` | 移除 `setDefaultFocusById("rest-hours-down-button")` 调用 |
| `src/game/scene/battle_scene.cpp` | 移除 `queueDefaultFocus()` 和 `setDefaultFocusById("battle-action-attack")` 调用 |
| `src/game/scene/save_slot_select_scene.h` | 移除 `focus_before_confirm_` 成员、`queueDefaultFocus()` 声明、`shouldSyncHoverFocus()` 声明 |
| `src/game/scene/save_slot_select_scene.cpp` | 移除 `enableHoverFocusSync` 调用（含 lambda）、`queueDefaultFocus()` 方法定义、`shouldSyncHoverFocus()` 方法定义、`showOverwriteConfirm()` 中焦点保存逻辑、`hideOverwriteConfirm()` 中焦点恢复逻辑 |
| `src/game/scene/inventory_menu_scene.h` | 移除 `focus_before_action_menu_` 成员 |
| `src/game/scene/inventory_menu_scene.cpp` | 移除 `rememberFocusBeforeActionMenu()` 中 `getFocusedElement` 调用、`closeActionMenu()` 中焦点恢复、`showActionMenu()` 中 `queueFocusFirstEnabledElementByClass` 调用、各处 `focus_before_action_menu_ = nullptr` 清理 |

### 不修改的文件（保留）
| 文件 | 原因 |
|------|------|
| `ui/rmlui/theme/nav.rcss` | 先保留 `tab-index`/`nav-*`/`:focus` 规则，便于后续恢复导航；但需接受并验证鼠标点击后可能出现的原生 `:focus` 样式 |
| `ui/rmlui/theme/menu_widgets.rcss` | 同上 |
| `ui/rmlui/learn/**/*.rcss` | 教程文件，保留完整 |
| `ui/rmlui/scenes/inventory_menu.rcss` | 暂保留现有 `:focus` 样式；如鼠标体验出现残留高亮，再单独收敛到 `:hover` / `:focus-visible` |
| `src/engine/input/input_manager.cpp` | `menu_up/down/left/right/confirm` action 定义保留，不影响运行 |

## 执行步骤

### Stage 0：清理构建、测试与文档前置项
1. `src/CMakeLists.txt`：移除 `hover_focus_sync_listener.cpp`
2. 更新/删除失效的 source tests：
   - `tests/engine/ui/rml_document_controller_source_test.cpp`
   - `tests/game/menu_hover_focus_sync_test.cpp`
   - `tests/engine/core/game_app_ui_navigation_source_test.cpp`
   - `tests/game/inventory_menu_scene_slot_grid_registration_test.cpp`
3. 更新文档：
   - `docs/engine/ui_framework.md`
   - `docs/testing/ui-regression-checklist.md`
4. 编译验证，确保前置清理不再阻塞后续阶段

### Stage 1：删除 HoverFocusSyncListener
1. 删除 `hover_focus_sync_listener.h` 和 `hover_focus_sync_listener.cpp`
2. 从 `rml_document_controller.h` 移除 `#include` 和所有 hover_focus 相关成员/方法
3. 从 `rml_document_controller.cpp` 移除 `applyHoverFocusSync`、`clearHoverFocusListener` 实现
4. 更新 `load()` 和 `unload()` 移除 hover_focus 调用
5. 编译验证

### Stage 2：精简 RmlDocumentController focus API
1. 从 `.h` 移除：`DefaultFocusKind` 枚举、`default_focus_kind_`/`default_focus_token_` 成员、`setDefaultFocusById`、`setDefaultFocusFirstEnabledByClass`、`queueDefaultFocus`、`queueFocusElement*` 系列
2. 从 `.cpp` 移除对应实现
3. 编译验证（此时游戏层调用点会报错，进入 Stage 3 修复）

### Stage 3：清理游戏层 Scene 调用
1. `title_scene.cpp`：删除 2 行 focus 调用
2. `pause_menu_scene.cpp`：删除 2 行 focus 调用
3. `rest_dialog_scene.cpp`：删除 1 行 focus 调用
4. `battle_scene.cpp`：删除 2 行 focus 调用
5. `save_slot_select_scene.h/.cpp`：删除 `focus_before_confirm_` 成员、`queueDefaultFocus()` 和 `shouldSyncHoverFocus()` 的声明与实现、`showOverwriteConfirm`/`hideOverwriteConfirm` 中焦点保存恢复逻辑、`enableHoverFocusSync` 调用
6. `inventory_menu_scene.h/.cpp`：删除 `focus_before_action_menu_` 成员及所有相关使用
7. 编译验证

### Stage 4：合并清理 Runtime 导航 API 与 GameApp 桥接
1. 从 `game_app.h/.cpp` 移除：
   - `initMenuNavigationBindings()` / `disconnectMenuNavigationBindings()`
   - `onMenuNavigateUpPressed` / `DownPressed` / `LeftPressed` / `RightPressed`
   - `onMenuConfirmPressed`
   - 启动/清理流程中的 connect/disconnect 调用
2. 从 `rml_ui_runtime.h/.cpp` 移除：
   - `navigateUp/Down/Left/Right`
   - `confirmFocusedElement`
   - `getFocusedElement`
   - `focusElement*`
   - `queueFocusElement*`
   - `PendingFocusRequest`、`clearPendingFocusRequestsForDocument`、`pending_focus_requests_`
3. `update()` 中移除延迟焦点队列处理逻辑
4. `applyInteractionPolicy()` 中移除 `entry.doc->Blur()` 调用（保留 `pointer-events: none`）
5. 编译验证
6. 全量测试运行

## 验证清单
- [x] 编译通过（`ninja -C build`）
- [ ] 标题画面鼠标点击正常
- [ ] 暂停菜单鼠标点击正常
- [ ] 存档选择界面鼠标操作正常（含覆盖确认弹窗）
- [ ] 背包界面鼠标操作正常（含动作菜单）
- [ ] 休息对话框鼠标操作正常
- [ ] 战斗界面鼠标操作正常
- [ ] 鼠标点击后若出现 `:focus` 高亮，视觉表现符合预期；若不符合，记录并单独收敛样式
- [ ] 非活跃场景的 UI 不响应鼠标点击（`pointer-events: none` 仍生效）
- [x] 测试通过（`ctest --output-on-failure`，`433/433` 通过，0 失败，8 skip）

## 将来恢复键盘/手柄导航的路径

如果未来需要恢复，建议按复杂度分层恢复，不要预设“只加几十行就完全回到当前体验”：

1. **简单菜单优先恢复**
   - `RmlUiRuntime` 加回 `navigateUp/Down/Left/Right` + `confirmFocusedElement`
   - `GameApp` 加回 `menu_*` 到上述 API 的桥接
   - 这一步足以恢复 `Title` / `Pause` / `RestDialog` / `Battle` 这类简单菜单的基础导航
2. **按需恢复默认焦点**
   - 如进入菜单后需要立即可导航，再加回简化版默认焦点（优先 `queueFocusElementById` 或 `queueFocusFirstEnabledElementByClass`）
3. **复杂弹层/菜单局部恢复**
   - `SaveSlotSelectScene` overwrite confirm：重新建立“打开时把焦点送进 confirm 层，关闭后恢复到原位置或 fallback”
   - `InventoryMenuScene` action menu：重新建立“打开时聚焦首个 action entry，关闭后恢复到原 slot 或合理 fallback”
4. **输入路由策略复核**
   - 如果继续走 `menu_*` 逻辑动作桥接，需确认 `InputManager::shouldSuppressRmlUiKeyboardEvent()` 仍与设计一致
   - 如果改为直接把原生键盘事件交给 RmlUi，则需要同步调整或移除这层抑制逻辑
5. **Hover-Focus 同步单独评估**
   - 只有在“鼠标悬停后立即接手柄 confirm”这类混合输入体验明确需要时，再考虑加回
   - 否则优先依赖 RmlUi 原生 `:hover` / `:focus-visible` 与基础导航能力
