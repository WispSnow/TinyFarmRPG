# UI 框架约定：RmlUi Runtime / Document Controller / Retained UI

> 用途：统一当前项目生产 UI 的心智模型与职责边界。本文只描述游戏内正式 UI（RmlUi）；ImGui 调试面板见 `docs/engine/debug_ui.md`。

当前 TinyFarm 的 UI 可以用一句话概括：

> **`GameApp` 持有全局 `RmlUiRuntime`；Scene/HUD 通过 `RmlDocumentController` 管理各自的 RML 文档与 data model；布局、状态样式与大部分交互定义放在 `.rml/.rcss` 中，C++ 只负责数据投影、事件路由、世界锚点与少量运行时状态。**

## 1) 核心模块与职责边界

### 1.1 `RmlUiRuntime`：全局运行时
- 入口：`src/engine/ui/rmlui/rml_ui_runtime.h/.cpp`
- 职责：
  - 持有全局 `Rml::Context`
  - 接收 SDL 事件并转发给 RmlUi
  - 按 `owner_scene_id` 加载/卸载文档，避免跨 Scene 泄漏
  - 在每个渲染帧显式执行 `update()`

关于 `owner_scene_id`，有两个容易混淆的点：
- 它是“归属分组标签”，不是“单文档槽位”
- 同一个 `owner_scene_id` 下可以同时加载多个文档；关闭 Scene 时再按 owner 成组回收

因此：
- 一个 Scene 可以同时挂多个独立 UI 模块
- 例如 `GameScene` 下的时钟、Hotbar、overlay prompt、tooltip、dialogue bubble、screen fade 都可以是彼此独立的文档

### 1.2 `GameApp` / `SceneManager` / `Scene::prepareUi`
- 入口：
  - `src/engine/core/game_app.cpp`
  - `src/engine/scene/scene_manager.cpp`
  - `src/engine/scene/scene.h`
- 职责：
  - `GameApp::render()` 先调用 `scene_manager_->prepareUi(alpha)`，再调用 `rmlui_runtime_->update()`
  - `SceneManager::prepareUi()` 会遍历场景栈：
    - 栈顶场景传入真实 `interpolation_alpha`
    - 被覆盖场景传入 `1.0f`，保证冻结场景的 retained UI 不重复插值
  - `Scene::prepareUi()` 是 retained UI 的组合准备阶段，用于把世界锚点、插值相机等表现层数据先写回 DOM

### 1.3 `RmlDocumentController`：场景/HUD 级文档控制器
- 入口：`src/engine/ui/rmlui/rml_document_controller.h/.cpp`
- 标准职责：
  - `attach(runtime, owner_scene_id)`
  - `createModel(model_name, data_type_register)`
  - `bindSimpleEvent(...)` / `bindEvent(...)`
  - `load(document_path)` / `unload()`
  - `markDirty(name)` / `markAllDirty()`

推荐理解：
- `RmlUiRuntime` 是全局宿主
- `RmlDocumentController` 是单个 Scene / HUD 组件的装配助手

补充约定：
- 一个 `RmlDocumentController` 当前只托管一个文档实例和一套 data model 生命周期
- 它更适合对应“一个当前同时存在的 UI 模块”
- 如果一个 Scene 里有多个同时存在、且各自独立的数据模型/事件/焦点逻辑，通常就会有多个 controller
- 如果两个文档不会同时存在，也可以复用同一个 controller，先 `unload()` 再 `load()` 下一份文档

### 1.4 游戏侧 UI 组合层
- `src/game/scene/title_scene.cpp`、`pause_menu_scene.cpp`、`save_slot_select_scene.cpp`、`rest_dialog_scene.cpp`、`battle_scene.cpp`
  - 各自持有一个 `RmlDocumentController`
  - 负责简单菜单、按钮和鼠标点击流程
- `src/game/scene/inventory_menu_scene.cpp`
  - 也是一个独立 Scene，但会额外维护 slot grid view model、action menu 和 tooltip
- `src/game/ui/game_scene_ui_controller.cpp`
  - 负责 `GameScene` 的 HUD 组合：
    - `HotbarUI`
    - `TimeClockHud`
    - `ItemTooltipUI`
    - `DialoguePresentationController`
    - `DialogueBoxView`
    - `FloatingNoticeView`
    - overlay prompt bar
    - `RmlScreenFade`

一个实际例子是 `GameScene`：
- 同一个 `scene_instance_id` 下会同时存在多份文档
- 其中 `HotbarUI`、`TimeClockHud`、overlay prompt bar 各自有自己的 `RmlDocumentController`
- `ItemTooltipUI`、`DialogueBoxView`、`FloatingNoticeView` 这类 HUD 控件则直接使用 runtime + DOM 操作，不强制经过 data model controller

### 1.5 浮动控件：直接操作 DOM，但不手写排版
- `src/game/ui/item_tooltip_ui.cpp`
- `src/game/ui/dialogue_box_view.cpp`
- `src/game/ui/floating_notice_view.cpp`

这两类控件允许 C++ 直接：
- `GetElementById()`
- `SetInnerRML()`
- `SetProperty("left"/"top", ...)`

但约定是：
- 文本排版、自动换行、面板尺寸仍交给 RmlUi
- C++ 只负责内容、显隐和世界/鼠标定位；底部对话框是屏幕固定 HUD，不参与世界锚点刷新

## 2) 标准生命周期模式

一个典型 Scene/HUD 文档的装配顺序如下：

1. `document_controller_.attach(runtime, instanceId())`
2. `auto constructor = document_controller_.createModel("model_name", &type_register_)`
3. `constructor.Bind(...)` 绑定字符串、布尔、数组或 struct view model
4. `document_controller_.bindSimpleEvent(constructor, "event_name", ...)`
5. `document_controller_.load("ui/rmlui/...")`
6. 初始同步后 `markDirty(...)` / `markAllDirty()`
7. `clean()` 或析构时统一 `document_controller_.unload()`

如果一个 Scene 里有多个并存模块，模式就是把这套流程各自跑一遍，而不是把所有模块硬塞进同一个 controller。

典型伪代码：

```cpp
document_controller_.attach(runtime, instanceId());

auto constructor = document_controller_.createModel("pause_menu");
constructor.Bind("can_save", &can_save_);
constructor.Bind("status_text", &status_text_);

document_controller_.bindSimpleEvent(constructor, "resume", [this] { onResume(); });
document_controller_.bindSimpleEvent(constructor, "back_to_title", [this] { onBackToTitle(); });

document_controller_.load("ui/rmlui/scenes/pause_menu.rml");
document_controller_.markAllDirty();
```

## 3) 布局、逻辑与样式的分工

### 3.1 应该放在 `.rml/.rcss` 的内容
- DOM 层级
- 大部分布局与尺寸
- hover / focus / selected / active / disabled 样式
- `data-for` / `data-if` / `data-class-*` / `data-style-*`
- 纯视觉动画与过渡

### 3.2 应该放在 C++ 的内容
- data model 绑定
- 命令分发与 dispatcher 交互
- 场景切换、游戏状态切换
- 世界坐标 -> 屏幕坐标的锚点定位
- 运行时拼装 prompt text / slot view model / action menu entries
- 对真实 DOM 几何的读取与少量像素级定位

### 3.3 明确禁止的旧模式
- 再引入额外的命令桥接层来替代 `data-event-*`
- 手写文本换行和手动测量 tooltip / dialogue 尺寸
- 在 C++ 里镜像维护一套和 RCSS 重复的 grid/menu 几何常量
- 为简单的 signal -> runtime 转发单独创建一层薄控制器抽象

## 4) 输入与交互

### 4.1 事件进入 RmlUi 的路径
- 原始 SDL 键盘/鼠标事件：`InputManager` 通过 `setRmlUiEventForwarder(...)` 转发给 `RmlUiRuntime`

当前阶段是鼠标优先：
- 菜单与弹层交互以 hover / click 为主
- `GameApp` 不再把 `menu_*` 逻辑动作桥接到 RmlUi 导航
- 在菜单上下文中，绑定到 `menu_*` 的扫描码仍会被 `shouldSuppressRmlUiKeyboardEvent()` 抑制，因此键盘/手柄菜单导航当前处于关闭状态

### 4.2 当前交互约定
- 项目不再在 Scene C++ 层维护默认焦点、hover-focus 同步或关闭弹层后的焦点恢复
- RmlUi 原生鼠标点击仍可能触发 `:focus`，因此点击后的 focus 样式应视为库自身行为，而不是项目级 focus 管理
- 若未来恢复键盘/手柄导航，再按场景类型分层加回默认焦点和弹层焦点语义

### 4.3 模态与输入隔离
- 项目不再使用旧式的全屏点击阻断器
- 当前策略是：
  - 顶层 Scene 独占 `update/fixedUpdate`
  - 菜单 Scene 在进入时 `pushContext(Menu)`
  - Rml 文档自身通过 `pointer-events` 和栈顶渲染顺序接管交互

## 5) 当前主要 UI 形态

### 5.1 简单菜单 Scene
- 代表：Title / Pause / SaveSlotSelect / RestDialog / Battle
- 特征：
  - 单文档
  - 少量布尔/文本 data binding
  - `data-event-click`
  - 鼠标交互优先

### 5.2 Gameplay HUD
- 代表：`GameSceneUiController`
- 特征：
  - 组合多个 HUD 子组件
  - frame update 中维护时钟、tooltip、fade
  - `prepareUi(alpha)` 中维护 dialogue bubble 等世界锚点控件

### 5.3 Slot Grid UI
- 代表：`HotbarUI`、`InventoryMenuScene`
- 共享层：
  - `src/game/ui/slot_grid_support.h/.cpp`
  - 统一 slot view model
  - 统一 drag/drop state
  - 统一 indexed data event 绑定

### 5.4 浮动面板与屏幕过渡
- Tooltip / Dialogue bubble：
  - 用 RmlUi 自动布局内容
  - C++ 负责定位
- `RmlScreenFade`：
  - 用 RCSS transition + `transitionend`
  - C++ 只负责 phase、动态 duration 与显示/隐藏

## 6) 主题与资源组织

- `ui/rmlui/theme/`
  - `reset.rcss`
  - `base.rcss`
  - `modal.rcss`
  - `nav.rcss`
  - `slot_widgets.rcss`
- `ui/rmlui/scenes/`
  - 覆盖式菜单、InventoryMenu 等 Scene 文档
- `ui/rmlui/hud/`
  - 热键栏、时钟、tooltip、dialogue bubble、game overlay
- `ui/rmlui/overlay/`
  - 屏幕淡入淡出等全局 overlay

约定：
- 共享结构样式优先沉到 `theme`
- 具体场景只保留布局差异与局部视觉差异

## 7) 常见坑

1. 数据变了但界面没刷新
- 原因：忘记 `markDirty()` / `markAllDirty()`

2. 读取到旧尺寸
- 原因：`SetInnerRML()` 后立即读取 `GetOffsetWidth/Height()`，但布局还没刷新
- 解决：同帧需要精确尺寸时先 `document->UpdateDocument()`

3. Scene 退出后文档残留
- 原因：没有统一 `unload()`
- 解决：所有生产 UI 都通过 `RmlDocumentController` 或 owner-scene 文档统一清理

4. 菜单导航重复触发或不触发
- 原因：当前鼠标优先阶段仍在期待键盘/手柄菜单导航，或者菜单上下文没正确 push/pop
- 解决：先按鼠标路径回归；若要恢复键盘/手柄，再同步恢复 `menu_* -> RmlUi` 桥接与默认焦点语义

5. 布局常量在 C++ 和 RCSS 双份维护
- 解决：优先读取真实 DOM 几何；只在浮动控件定位时做必要的像素级补充
