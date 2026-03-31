# RmlUi Runtime 并列化与 GameScene UI 控制器重构计划

## Context

当前 RmlUi 集成已经可用，但结构上有两个核心问题：

- `RmlUILayer` 实际上已经不是一个“纯渲染层”，而是一个完整的 UI runtime / manager：
  - Rml 初始化与销毁
  - `Rml::Context` 持有
  - 文档加载与 owner 管理
  - active scene 交互隔离
  - 事件转发
  - 焦点查询、直接聚焦、延迟聚焦
  - 导航与确认
  - texture filter 与 viewport 处理
- 但它目前被 `GLRenderer` 持有，导致上层代码普遍通过 `Context -> GLRenderer -> RmlUILayer` 访问 UI。

这会带来两个后果：

1. 场景和 UI wrapper 被迫依赖 renderer backend 细节
2. 如果只在外面再包一层 `RmlUiService`，很容易变成“薄转发层”，新增一层但没有真正改变 ownership 与职责边界

同时，`GameScene` 当前已经承担了大量 UI 组合职责：

- `TimeClockHud`
- `HotbarUI`
- `ItemTooltipUI`
- `DialogueBubbleController`
- `DialogueBubbleView`
- overlay prompt bar
- `RmlScreenFade`

因此，本次重构的目标不再是“给 renderer 下的 layer 再套一层 service”，而是：

1. 把当前 `RmlUILayer` 明确提升为 引擎层 UI runtime / manager，与 `GLRenderer` 并列
2. 让 `GLRenderer` 只负责 RmlUi 的渲染接入，而不是持有整个 UI manager
3. 在游戏层新增 `GameSceneUiController`，收拢 `GameScene` 的场景级 UI 组合逻辑

## 基于审阅的关键修正

结合审阅意见，本计划做以下修正：

- 不再新增 `RmlUiService` 薄封装层
- `RmlUILayer` 直接作为引擎层 UI manager 暴露
- `Context` 中的 UI 入口改为可空指针，而不是引用，便于 headless / 测试退化
- 焦点相关 API 作为首批迁移对象明确纳入范围
- `HoverFocusSyncListener` 明确纳入迁移清单
- `InventoryMenuScene` 明确列为重点调用点，而不是只放在文件列表里
- 迁移过程保留旧 `GLRenderer` 路径直到最后统一清理，保证中间步骤持续可编译
- 测试重点改为“无 RmlUi 时安全退化”和“ownership / 生命周期顺序”验证

## 范围

### 本次包含

- `RmlUILayer` ownership 从 `GLRenderer` 移到 `GameApp`
- `GLRenderer` 改为仅保留对 `RmlUILayer` 的非 owning 渲染接入
- `Context` 新增 `UiServices` 与 `getRmlUi()` 入口
- 将引擎层和游戏层对 RmlUi 的调用点迁移到 `Context::getRmlUi()`
- 将现有焦点 / 延迟焦点 / active scene / 事件处理路径一并收敛
- 新增 `GameSceneUiController`
- 将 `GameScene` 中的 HUD / overlay / tooltip / dialogue bubble / fade 逻辑迁移到 controller
- 同步补测试与必要注释

### 本次不包含

- 一开始就把 `RmlUILayer` 再拆成多个更细的类
- Title / Pause / SaveSlotSelect / Battle 的统一菜单基类抽象
- `RmlDataBridge` / `RmlEventBridge` 的底层重写
- RmlUi 渲染顺序、viewport 策略、texture filter 目标的重新设计

## 实现思路

### 1. 先承认当前 `RmlUILayer` 的真实角色

当前 `RmlUILayer` 从职责上看，已经是一个 UI runtime / manager，而不是 renderer 内部的普通 layer。

因此更合理的做法不是“再包一层 service”，而是：

- 保留 `RmlUILayer` 现有管理职责
- 改变它的 ownership 和暴露位置
- 让它成为与 `GLRenderer` 并列的引擎模块

后续如果名称仍然容易误导，可以在结构稳定后再考虑：

- `RmlUILayer` -> `RmlUiRuntime`

但这不是本次首批重构的必须项。

### 2. `GLRenderer` 只保留渲染接入，不再持有 manager

推荐改法：

- `GLRenderer` 去掉 `std::unique_ptr<RmlUILayer> rmlui_layer_`
- 改为持有非 owning 指针：
  - `engine::ui::rmlui::RmlUILayer* rmlui_layer_{nullptr};`
- 新增显式注入接口：
  - `void setRmlUiLayer(engine::ui::rmlui::RmlUILayer* layer);`

`GLRenderer` 继续负责：

- `present()` 中的 `setViewport -> update -> render`
- `resize()` 时同步 viewport
- 必要的 texture filter 转发
- RmlUi 与 ImGui 的渲染顺序

但不再负责：

- 创建 `RmlUILayer`
- 销毁 `RmlUILayer`
- 把它作为业务入口暴露给场景

这一步完成后，renderer 只扮演“渲染管线接入者”，不再扮演“UI 模块持有者”。

### 3. `GameApp` 直接持有 `RmlUILayer`

`RmlUILayer` 的生命周期应改由 `GameApp` 控制。

建议新增成员：

- `std::unique_ptr<engine::ui::rmlui::RmlUILayer> rmlui_layer_;`

建议新增初始化步骤：

1. `initGLRenderer()`
2. `initRmlUiLayer()`
3. `initInputManager()`
4. `initUINavigationController()`
5. `initContext()`

原因：

- RmlUi 初始化需要 GL 上下文已存在
- `InputManager` 的 RmlUi event forwarder 需要在 `Context` 之前就能拿到 layer
- `UINavigationController` 也需要在 `Context` 之前接线

建议销毁顺序：

- `scene_manager_` / `context_` 先释放
- `rmlui_layer_` 在 `gl_renderer_` 之前释放
- `gl_renderer_` 最后清理其自身 GL 资源与上下文

这样可以保持：

- `RmlUILayer` 的 GL 资源销毁发生在有效 OpenGL 上下文内
- ownership 关系清晰，不再靠 renderer 析构顺带清理 UI manager

### 4. `Context` 增加可空的 `UiServices`

由于 headless 测试环境可能没有可用的 RmlUi runtime，因此不建议把 UI 服务做成引用。

建议新增：

```cpp
struct UiServices {
    engine::ui::rmlui::RmlUILayer* rmlui{nullptr};
};
```

`Context` 增加：

- `UiServices ui_;`
- `[[nodiscard]] const UiServices& ui() const`
- `[[nodiscard]] engine::ui::rmlui::RmlUILayer* getRmlUi() const`

保留 `UiServices` 的原因：

- 与 `CoreServices / RenderServices / ResourceServices` 对称
- 后续若出现第二个 UI 级 service，不需要再次改 `Context` 结构

但这里明确采用“指针而不是引用”，以保证：

- headless 环境安全
- 单元测试更容易构造
- 上层代码能显式表达“UI runtime 不可用”

### 5. 公开入口必须覆盖完整焦点能力，而不只是文档管理

审阅意见里最关键的一点是：RmlUi 的高频调用不只有 `loadDocument()`。

当前场景中实际依赖的常用接口包括：

- `getContext()`
- `processEvent(SDL_Event&)`
- `loadDocument() / unloadDocument() / unloadDocumentsByOwner()`
- `showDocument() / hideDocument()`
- `setActiveScene()`
- `getFocusedElement()`
- `focusElement() / focusElementById() / focusFirstEnabledElementByClass()`
- `queueFocusElement() / queueFocusElementById() / queueFocusFirstEnabledElementByClass()`
- `navigateUp()/Down()/Left()/Right()`
- `confirmFocusedElement()`
- `setTextureFilterMode() / getTextureFilterMode()`

本次重构后，这整组能力都应通过 `Context::getRmlUi()` 获取，而不是只迁移一部分。

目标是让所有场景都能在不触碰 `GLRenderer` 的前提下完成 RmlUi 交互。

### 6. `HoverFocusSyncListener` 必须一并迁移

`HoverFocusSyncListener` 当前直接依赖 `RmlUILayer&`，并调用 `focusElement()`。

这意味着它也是 UI manager API 的消费者，必须显式纳入本次迁移。

本次建议：

- 先保持它继续依赖 `RmlUILayer&`
- 但构造来源改为 `Context::getRmlUi()`
- 不再从 `GLRenderer` 间接获取

如果后续需要进一步抽象焦点控制接口，再单独拆。

### 7. 引擎层调用点先统一切到 `Context::getRmlUi()`

首批迁移的引擎层公共路径：

- `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()`
- `SceneManager::syncRmlActiveScene()`
- `GameApp::initInputManager()`
- `GameApp::initUINavigationController()`
- `RmlUiDebugPanel`

这里的目标不是删除旧 renderer 接口，而是先建立新的主路径：

- 场景 owner 管理只通过 `Context::getRmlUi()`
- 输入与导航接线只依赖 `GameApp` 持有的 `rmlui_layer_`
- debug 面板优先展示 runtime 层状态，而不是 renderer 内部细节

### 8. `InventoryMenuScene` 是重点，不是附带项

`InventoryMenuScene` 的 RmlUi 使用比普通菜单场景更复杂，至少包括：

- `Rml::DataTypeRegister`
- tooltip UI
- 拖拽交互
- action menu 焦点保存与恢复
- `getFocusedElement()`
- `queueFocusElement()`
- `queueFocusFirstEnabledElementByClass()`

因此本次要把它单独视为重点调用点。

换句话说，迁移顺序上不能只先处理 Title / Pause / SaveSlotSelect 这种轻场景，必须显式覆盖：

- `InventoryMenuScene`

否则焦点与拖拽路径仍会残留 renderer backend 依赖。

### 9. `GameSceneUiController` 收拢场景级 UI 组合逻辑

`GameScene` 当前已经明显过载，应新增：

- `game::ui::GameSceneUiController`

推荐职责边界：

- 创建 / 销毁
  - `TimeClockHud`
  - `HotbarUI`
  - `ItemTooltipUI`
  - `DialogueBubbleController`
  - `DialogueBubbleView`
  - overlay prompt bar 文档与 data bridge
  - `RmlScreenFade`

- 每帧更新
  - `update(float delta_time, entt::registry&)`
  - `refreshAnchoredWidgets(Camera&, float interpolation_alpha)`

- 对 `GameScene` 暴露高层接口
  - `toggleHotbar()`
  - `applyHotbarChanged(...)`
  - `applyHotbarSlotChanged(...)`
  - `setPromptBarVisible(bool visible)`
  - `[[nodiscard]] engine::ui::IScreenFade* screenFade() const`

`GameSceneUiController` 推荐直接持有 `Context&`，而不是把 `InputManager&` 等依赖拆成函数参数层层传入。

因此，不采用：

- `refreshOverlayPrompts(InputManager&)`

而是采用：

- controller 内部直接从 `context_` 读取 `InputManager`

这样能减少调用噪音，并让依赖关系更真实。

### 10. 编译安全优先，旧路径延后删除

迁移步骤必须保证每一步都可编译。

因此建议：

- 第一步先引入 `GameApp` 持有的 `rmlui_layer_` 与 `Context::getRmlUi()`
- `GLRenderer::getRmlUILayer()` 暂时保留
- 各场景 / wrapper / panel 逐步切换到新入口
- 最后统一删除 renderer 侧旧便捷入口

这能避免“迁移到一半整个分支无法编译”的中间状态。

### 11. 关于“逻辑与渲染是否进一步拆类”

从结构上讲，你的判断是对的：

- RmlUi 的逻辑 / 文档 / 事件 / 焦点 与 渲染接入是可以分离考虑的
- 当前 `RmlUILayer` 的确更像 manager，而不是单纯渲染层

但本次首批重构不建议立刻把它拆成两个新类。

原因：

- 当前最主要的问题是 ownership 和依赖方向错误
- 一旦 `RmlUILayer` 从 `GLRenderer` 中剥离，并由 `GameApp` / `Context` 直接管理，它作为 manager 的角色就变得合理了
- 这时 renderer 只剩“渲染接入”的职责，结构已经显著变干净

因此首批最优路径是：

1. 先把 manager 从 renderer ownership 中移出
2. 让 renderer 只做 render hook
3. 等结构稳定后，再决定是否把 `RmlUILayer` 重命名或继续拆分

## 需要新增的文件

- `src/game/ui/game_scene_ui_controller.h`
- `src/game/ui/game_scene_ui_controller.cpp`
- `plans/rmlui-service-and-game-scene-ui-refactor.md`

建议新增测试文件：

- `tests/engine/ui/rmlui_runtime_wiring_test.cpp`
- `tests/game/game_scene_ui_controller_smoke_test.cpp`

## 预计修改的主要文件

- `src/engine/core/context_modules.h`
- `src/engine/core/context.h`
- `src/engine/core/context.cpp`
- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`
- `src/engine/scene/scene.h`
- `src/engine/scene/scene.cpp`
- `src/engine/scene/scene_manager.cpp`
- `src/engine/debug/panels/rmlui_debug_panel.h`
- `src/engine/debug/panels/rmlui_debug_panel.cpp`
- `src/engine/ui/rmlui/hover_focus_sync_listener.h`
- `src/engine/ui/rmlui/hover_focus_sync_listener.cpp`
- `src/engine/ui/rmlui/rml_screen_fade.h`
- `src/engine/ui/rmlui/rml_screen_fade.cpp`
- `src/engine/render/opengl/gl_renderer.h`
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/game/scene/game_scene.h`
- `src/game/scene/game_scene.cpp`
- `src/game/scene/title_scene.cpp`
- `src/game/scene/pause_menu_scene.cpp`
- `src/game/scene/save_slot_select_scene.cpp`
- `src/game/scene/rest_dialog_scene.cpp`
- `src/game/scene/battle_scene.cpp`
- `src/game/scene/inventory_menu_scene.cpp`
- `src/game/ui/hotbar_ui.h`
- `src/game/ui/hotbar_ui.cpp`
- `src/game/ui/time_clock_hud.h`
- `src/game/ui/time_clock_hud.cpp`
- `src/game/ui/item_tooltip_ui.h`
- `src/game/ui/item_tooltip_ui.cpp`
- `src/game/ui/dialogue_bubble_view.h`
- `src/game/ui/dialogue_bubble_view.cpp`

## 实现步骤

### Step 1: `GameApp` 接管 `RmlUILayer` ownership

新增 `GameApp::initRmlUiLayer()`，在 `GLRenderer` 初始化后创建 layer，并注入到 renderer。

### Step 2: `Context` 新增 `UiServices`

在 `Context` 中加入 `UiServices` 与 `getRmlUi()`，使用可空指针表达 headless / 无 UI runtime 状态。

### Step 3: 让 `GLRenderer` 退回非 owning 渲染接入

移除 renderer 对 `RmlUILayer` 的创建/销毁职责，只保留 `setRmlUiLayer()`、`present()`、`resize()` 中的渲染接线。

### Step 4: 迁移引擎层公共调用点

将 `Scene`、`SceneManager`、`InputManager` 接线、`UINavigationController` 接线、`RmlUiDebugPanel` 迁移到新的 runtime 持有方式。

### Step 5: 迁移焦点相关调用点与 `HoverFocusSyncListener`

确保 direct focus / queued focus / hover-focus sync 全部切到 `Context::getRmlUi()`。

### Step 6: 迁移菜单场景与 `InventoryMenuScene`

将 Title / Pause / SaveSlotSelect / Rest / Battle / Inventory 的 RmlUi 调用点统一切换到 runtime 入口，重点验证 `InventoryMenuScene` 的拖拽与焦点恢复。

### Step 7: 新增 `GameSceneUiController`

把 `GameScene` 中的 HUD / overlay / tooltip / dialogue bubble / fade 组合逻辑迁移到 controller。

### Step 8: 收缩 `GameScene`

让 `GameScene` 只保留玩法场景职责，通过 controller 调用 UI 能力。

### Step 9: 清理旧 renderer 入口并补测试

确认所有调用点迁移完成后，再删除 `GLRenderer` 上不再需要的旧 RmlUi 业务入口，并补齐 runtime wiring 与 controller smoke test。

## 待办追踪

- [ ] `GameApp` 新增 `rmlui_layer_` 成员并实现 `initRmlUiLayer()`
- [ ] `GLRenderer` 改为非 owning `RmlUILayer*`
- [ ] `GLRenderer` 新增 `setRmlUiLayer(...)`
- [ ] `GameApp::close()` 调整 `RmlUILayer` 与 `GLRenderer` 的销毁顺序
- [ ] `Context` 新增 `UiServices`
- [ ] `Context::getRmlUi()` 改为返回可空指针
- [ ] `Scene::loadRmlDocument()` / `unloadAllRmlDocuments()` 切到 `getRmlUi()`
- [ ] `SceneManager::syncRmlActiveScene()` 切到 `getRmlUi()`
- [ ] `InputManager` 的 RmlUi event forwarder 改为使用 `GameApp` 持有的 `rmlui_layer_`
- [ ] `UINavigationController` 的导航接线改为使用 `GameApp` 持有的 `rmlui_layer_`
- [ ] 焦点 API 调用点全部迁移
- [ ] `HoverFocusSyncListener` 迁移到新的 runtime 获取路径
- [ ] `TitleScene` 迁移
- [ ] `PauseMenuScene` 迁移
- [ ] `SaveSlotSelectScene` 迁移
- [ ] `RestDialogScene` 迁移
- [ ] `BattleScene` 迁移
- [ ] `InventoryMenuScene` 迁移
- [ ] `TimeClockHud` 迁移
- [ ] `HotbarUI` 迁移
- [ ] `ItemTooltipUI` 迁移
- [ ] `DialogueBubbleView` 迁移
- [ ] `RmlScreenFade` 迁移
- [ ] 新增 `GameSceneUiController`
- [ ] `GameScene` UI 初始化、update、render 前同步、clean 逻辑迁移到 controller
- [ ] 新增 runtime wiring 测试
- [ ] 新增 `GameSceneUiController` smoke test
- [ ] 最终清理旧 `GLRenderer` RmlUi 业务入口

## 备注

本计划的首要目标是先修正 ownership 与依赖方向。

如果这一步完成后，`RmlUILayer` 仍然显得语义过重，再单独开下一份计划考虑：

- `RmlUILayer` 更名为 `RmlUiRuntime`
- 进一步抽出更薄的 renderer-side render hook
