# RmlUi 布局与浮动控件契约（Layout Contract）

本文档冻结当前生产 UI 的布局语义，作为 RmlUi UI 演进时的行为基线。

## 1. 适用范围
- RmlUi 文档与样式：
  - `ui/rmlui/**/*.rml`
  - `ui/rmlui/**/*.rcss`
- 运行时与控制器：
  - `src/engine/ui/rmlui/rml_ui_runtime.*`
  - `src/engine/ui/rmlui/rml_document_controller.*`
  - `src/engine/ui/rmlui/rml_screen_fade.*`
- 游戏 UI：
  - `src/game/ui/item_tooltip_ui.*`
  - `src/game/ui/dialogue_bubble_view.*`
  - `src/game/ui/hotbar_ui.*`
  - `src/game/scene/inventory_menu_scene.*`

目标：明确谁负责布局、什么时候能读取最终尺寸、哪些场景允许 C++ 写 `left/top`。

## 2. 基础语义：普通面板布局由 RmlUi 单独负责

### 2.1 布局真源
- 普通菜单、HUD 面板、slot grid、action menu、prompt bar 的布局真源是 DOM + RCSS
- C++ 不再维护独立的 `requested size / layout size / anchor / pivot / override size` 体系

### 2.2 允许 C++ 做什么
- 绑定 data model
- 切换 class / 布尔可见状态
- 读取真实 DOM 几何：
  - `GetOffsetLeft/Top()`
  - `GetOffsetWidth/Height()`
- 在少数浮动控件上写 `left/top`

### 2.3 明确不再做什么
- 不手写文本换行
- 不手写 tooltip / dialogue panel 的宽高计算
- 不在 C++ 中镜像保存与 RCSS 重复的 grid/slot/menu 位置常量

## 3. 尺寸读取时序

RmlUi 的布局是惰性完成的，因此：

- 只更新 data / inner RML 后，`GetOffsetWidth/Height()` 可能还是旧值
- 若同一帧就需要拿到最终尺寸，必须先刷新文档布局：
  - `document->UpdateDocument()`

当前约定：
- `ItemTooltipUI`、`DialogueBubbleView`
  - 改文本后如果当前可见，会立刻 `UpdateDocument()` 并刷新缓存尺寸
- 一般 Scene/HUD data binding
  - 允许等到下一轮 `RmlUiRuntime::update()` 后由 RmlUi 自然完成布局

## 4. 浮动控件语义

### 4.1 鼠标跟随型：`ItemTooltipUI`
- 内容尺寸由 RmlUi 自动排版得出
- C++ 每帧读取逻辑鼠标位置，并计算 tooltip 左上角
- 必须做屏幕边界钳制，保证 tooltip 不会跑出逻辑视口

### 4.2 世界锚点型：`DialogueBubbleView`
- 内容尺寸由 RmlUi 自动排版得出
- `DialogueBubbleController` 只负责 show/move/hide 路由
- `DialogueBubbleView` 每帧根据 world anchor、camera 和插值 alpha 计算屏幕锚点
- 最终以 `top_left = anchor - size * pivot` 计算面板位置

### 4.3 DOM 几何锚定型：`InventoryMenuScene` action menu
- action menu 不再通过 C++ 常量推导几何
- 必须基于真实 DOM 几何定位：
  - 读取 slot、slot-region、action-menu 的实际 box
  - 在真实容器范围内做左右翻转和边界钳制

## 5. `prepareUi(alpha)` 与世界锚点

世界锚点 UI 的更新时机固定为：

1. `SceneManager::prepareUi(interpolation_alpha)`
2. Scene 在 `prepareUi(alpha)` 中更新世界锚点控件位置
3. `GameApp::updateRmlUiFrame()` 调用 `RmlUiRuntime::update()`
4. 随后再进入渲染

约束：
- 需要相机插值的锚点 UI，必须在 `prepareUi(alpha)` 中完成位置刷新
- 不应等到 `render()` 再改 DOM，否则会和 RmlUi 的 update/layout 时机错开

## 6. 动画与过渡语义

### 6.1 普通视觉过渡
- 优先使用 RCSS `transition` / `animation`
- C++ 只切换 class、状态布尔或少量运行时属性

### 6.2 屏幕淡入淡出
- `RmlScreenFade` 使用：
  - `seconds > 0` 时动态写入 `transition: opacity <seconds>s linear-in-out`
  - `seconds <= 0` 时使用 `transition: none`
  - `.is-opaque` class
  - `transitionend`
- 约束：
  - phase 推进依赖 `opacity` 的 `transitionend`
  - 时长由 C++ 按 `fadeIn(seconds)` / `fadeOut(seconds)` 动态写入
  - 不再使用逐帧 alpha 插值状态机

## 7. 辅助工具边界

当前保留的最小辅助函数：
- `textToInnerRml(...)`
- `setPixelProperty(...)`
- `snapToPixel(...)`

约定：
- 辅助函数只负责写 DOM 属性和安全文本转义
- 不再保留从 computed style 反推布局的大批 helper

## 8. 测试映射
- `tests/game/ui_layout_integration_test.cpp`
  - Inventory grid / hotbar strip / action menu 几何
- `tests/game/dialogue_bubble_controller_test.cpp`
  - show/move/hide 路由与控制器不再手动换行
- `tests/engine/ui/rml_screen_fade_transition_source_test.cpp`
  - fade 使用 `transitionend` 与动态 transition 属性
- `tests/engine/ui/rmlui_transition_behavior_test.cpp`
  - tween 名称与 `transitionend` 行为
- `tests/game/rmlui_architecture_regression_test.cpp`
  - 整体 RmlUi 架构回归

## 9. 变更纪律
- 任何改变上述布局语义的改动，必须同时更新：
  - 本文档
  - 对应 RML/RCSS
  - 至少一个结构测试或行为测试

- 若发现某段 C++ 需要手算尺寸/位置才能工作，应优先先回答两个问题：
  1. 这部分能否直接交给 RmlUi 布局？
  2. 若必须读取几何，能否读取真实 DOM box，而不是再维护一套镜像常量？
