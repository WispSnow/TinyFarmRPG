# RmlUi UI 简化重构计划

## 元信息
- 任务ID：`UI-RML-001`
- 任务标题：`RmlUi UI 简化重构`
- 优先级：`P0`
- 状态：`Todo`
- 负责人：`TBD`
- 计划时间：`2026-04-01` 起
- 依赖任务：`无`
- 设计原则：`不考虑向后兼容，直接收敛到更简洁的最终架构`

## 背景
当前项目已经接入了 RmlUi 的核心能力：
- `data-model`
- `data-for`
- `data-if`
- `data-class-*`
- `data-style-*`
- `data-event-*`
- 键盘/手柄导航

但 UI 代码仍然存在以下结构性问题：
- 多个场景仍在使用自定义 `data-command + RmlEventBridge`，没有统一到 RmlUi 原生事件绑定模型。
- `ItemTooltipUI`、`DialogueBubbleView` 仍在 C++ 手写文本换行、测量和尺寸计算，职责没有真正交给 RmlUi。
- `RmlScreenFade` 仍在 C++ 侧逐帧写入 `opacity`，而不是优先使用 RmlUi 的 transition / animation 能力。
- `HotbarUI` 与 `InventoryMenuScene` 各自维护一套 slot view model、拖拽和 tooltip 路由，重复明显。
- 多个场景/HUD 在重复写文档生命周期、默认焦点、hover->focus 同步、modal overlay、reset RCSS。
- `InventoryMenuScene` 的动作菜单定位由 C++ 常量和 RCSS 常量双份维护，后续改样式风险很高。
- `rml_element_helpers.h` 中保留了大量“读取 computed style 后再由 C++ 反推布局”的辅助函数，这与目标架构不一致。

本计划目标不是做局部修补，而是把 UI 架构一次性收敛为“RML/RCSS 负责布局与大部分显隐逻辑，C++ 只负责状态投影、命令分发和少量世界坐标定位”。

## 重构目标
- 统一项目 UI 交互范式：只保留 `data-event-* + BindEventCallback`，移除 `data-command + RmlEventBridge`。
- 统一文档生命周期管理：减少每个 scene/HUD 重复写 `load/bind/focus/unregister/unload` 模板代码。
- 让 RmlUi 接管文本布局：移除手写文本 wrap/measure/宽高计算。
- 优先使用 RCSS transition / animation 处理 UI 过渡，避免 C++ 逐帧维护视觉插值状态。
- 收敛 slot 类 UI 的重复实现：背包、HUD hotbar、后续战斗/商店列表共享同一套 presenter 思路。
- 收敛 RCSS 基础设施：reset、modal、slot、导航焦点、按钮状态统一放入 theme。
- 删除未实际使用或价值不高的状态与桥接层，保持最终结构清晰。

## 非目标
- 本阶段不扩展新的 RPG 玩法。
- 本阶段不追求把所有 UI 都抽成复杂“通用框架”。
- 不保留旧的 `data-command` RML 写法兼容层。
- 不为了兼容旧代码而保留重复类和适配器。

## 最终目标架构

### 1. 基础层：RmlUi Runtime
保留现有 `RmlUiRuntime` 作为唯一 RmlUi 运行时入口，负责：
- context / document 生命周期
- SDL 事件转发
- 方向导航与确认
- scene owner 隔离

在此基础上新增一个轻量文档控制器，统一每个文档的常见样板逻辑。

### 2. 文档层：`RmlDocumentController`
新增统一的 per-document 控制器，负责：
- 创建/销毁 `RmlDataBridge`
- 加载/卸载 document
- 默认焦点
- hover->focus 同步注册与自动解绑
- 场景 owner 绑定

接入方式：
- 采用组合而非继承：`Scene` / HUD 控制器以成员变量持有 `RmlDocumentController`
- 不引入新的 scene 基类或额外继承链

最小接口草案：
- `load(...) / unload()`
- `bindModel(...)` 与模型注册辅助
- `bindEvent(...)`
- `setDefaultFocus(...)`
- `enableHoverFocusSync(...)`
- `show() / hide()`

目标效果：
- `TitleScene`
- `PauseMenuScene`
- `RestDialogScene`
- `BattleScene`
- `SaveSlotSelectScene`
- `GameSceneUiController` 内 overlay

都只保留：
- 绑定哪些变量
- 绑定哪些事件
- 事件触发后做什么

不再重复写文档样板代码。

### 3. 组件层：两类 UI 组件

#### 3.1 Floating Widget
用于：
- item tooltip
- dialogue bubble

职责：
- C++ 只负责文本内容、可见性、锚点和偏移
- RmlUi 负责文本换行、内容尺寸和盒模型
- C++ 只读取元素最终布局尺寸并设置 `left/top`

禁止再出现：
- 自定义 UTF-8 换行器
- 自定义文本宽度测量驱动的 UI 宽高计算

#### 3.2 Slot Grid Presenter
用于：
- HUD hotbar
- inventory hotbar
- inventory backpack
- 后续 battle/item/shop 列表可复用相同思路

职责：
- 统一 slot view model 结构
- 统一 hover / focus / drag / drop / click 路由
- 统一 tooltip/detail 同步入口
- 统一 decorator/count/selected/active 等状态投影

该层不负责业务规则本身，业务规则仍由 scene/HUD 注入回调决定。

### 4. 样式层：Theme 收敛
新增共享 theme 文件，至少拆分为：
- `ui/rmlui/theme/reset.rcss`
- `ui/rmlui/theme/modal.rcss`
- `ui/rmlui/theme/slot_widgets.rcss`
- `ui/rmlui/theme/nav.rcss`

原则：
- reset 只写一次，不再在每个生产 RCSS 顶部重复
- modal overlay / panel 外观统一
- `tab-index: auto` 和 `nav-*` 统一为 class
- slot 的基础尺寸、drag proxy、count、selected/active 状态统一
- 视觉过渡优先使用 RCSS transition / animation，C++ 只切换 class 或少量状态

## 关键设计决策

### 决策 1：删除 `RmlEventBridge`
原因：
- 它解决的是“简单按钮 -> 命令”映射，而 RmlUi 原生已经支持 `data-event-click`。
- 当前项目已经在 `SaveSlotSelectScene`、`HotbarUI`、`InventoryMenuScene` 中直接使用 `BindEventCallback`。
- 两套交互模型并存会增加维护成本。

结论：
- 删除 `src/engine/ui/rmlui/rml_event_bridge.h`
- 删除 `src/engine/ui/rmlui/rml_event_bridge.cpp`
- 所有简单场景改为 `data-event-click="xxx"` + `BindEventCallback("xxx", ...)`

### 决策 2：删除手写文本换行与尺寸计算
原因：
- RmlUi 原生支持 `white-space`、`word-break`、`max-width`、`line-height`
- 当前 C++ 的 wrap/measure 逻辑复杂且脆弱
- 文本表现应该由 RCSS 控制，而不是由 C++ 复制布局规则

结论：
- `ItemTooltipUI` 去掉 `wrapText()`、`measureText()`、内容高度计算逻辑
- `DialogueBubbleController` 去掉 `MAX_CHARS_PER_LINE` 风格的字符数换行
- `DialogueBubbleView` 去掉文本尺寸主导的宽高计算

### 决策 3：动作菜单位置只允许单一来源
原因：
- 当前 `InventoryMenuScene` 把 slot 尺寸、gap、列宽、menu 宽度同时写在 C++ 和 RCSS 两侧
- 这是典型的样式/逻辑双写

结论：
- 动作菜单锚点改为从实际 DOM 元素 box 读取
- 不再使用镜像 UI 常量推导位置
- 若实现更简洁，则允许直接把动作菜单挂在 slot DOM 附近生成

### 决策 4：不做兼容层
原因：
- 项目尚未上线，兼容旧写法没有收益
- 保留 adapter 只会延长重复状态

结论：
- 旧桥接类迁移后直接删除
- 旧 RML 写法直接更新，不保留 fallback

### 决策 5：`RmlScreenFade` 改为样式驱动过渡
原因：
- 当前 `RmlScreenFade` 在 C++ 中维护 `alpha/from/to/duration/timer`，本质是重复实现 UI 视觉插值。
- RmlUi 已支持 `transition`，且存在 `transitionend` 事件，可将“视觉插值”交回样式层。
- `IScreenFade` 仍要求暴露 `phase()`，且 `fadeOut(float seconds)` / `fadeIn(float seconds)` 允许调用方传入可变时长，因此 C++ 侧保留最小 phase 状态，并根据调用时长动态写入 `transition-duration` 或完整 `transition` 属性是合理的，但不应继续手写逐帧 alpha 动画。

结论：
- `RmlScreenFade` 改为“动态 transition 时长 + class/state + transitionend”驱动。
- C++ 仅保留：
  - `Idle / FadingOut / Holding / FadingIn`
  - 根据 `seconds` 动态设置过渡时长
  - pointer-events 切换
  - 过渡完成后的阶段推进
- 删除逐帧 alpha 插值和定时器逻辑。

### 决策 6：`rml_element_helpers.h` 只保留最小必要工具
原因：
- 当前文件中相当一部分 helper 的存在前提，是 C++ 仍在参与排版。
- 重构完成后，computed style 读取类 helper 应明显缩减，只保留真正必要的 DOM 几何与文本辅助。

结论：
- 保留：
  - `textToInnerRml()`
  - `setPixelProperty()`
  - 少量确有必要的几何读取 helper
- 删除或迁移大部分仅用于手写布局的 computed-style helper

### 决策 7：`HoverFocusSyncListener` 保留为内部 helper，由控制器托管
原因：
- hover->focus 同步本身仍然有价值，尤其对鼠标和手柄混合导航的菜单界面。
- 但业务 scene 不应继续直接持有/解绑 listener，这部分应收口到文档控制器。

结论：
- `HoverFocusSyncListener` 暂保留为独立 helper
- 由 `RmlDocumentController` 内部创建、注册和自动解绑
- 业务层只声明“是否启用 hover->focus 同步”及过滤条件，不再直接接触 listener 生命周期

## 分阶段实施

## Stage 1：文档控制器与交互模型统一

### 目标
- 建立统一的 `RmlDocumentController`
- 清理所有简单场景/HUD 的样板生命周期代码
- 全项目只保留 `data-event-* + BindEventCallback`

### 范围
- `TitleScene`
- `PauseMenuScene`
- `RestDialogScene`
- `BattleScene`
- `SaveSlotSelectScene`
- `GameSceneUiController` 内 overlay

### 主要改动
- 新增：
  - `src/engine/ui/rmlui/rml_document_controller.h`
  - `src/engine/ui/rmlui/rml_document_controller.cpp`
- `RmlDocumentController` 以成员组合方式接入现有 `Scene` / HUD 控制器
- `HoverFocusSyncListener` 从 scene 显式持有迁移为 `RmlDocumentController` 内部实现细节
- 删除：
  - `src/engine/ui/rmlui/rml_event_bridge.h`
  - `src/engine/ui/rmlui/rml_event_bridge.cpp`
- 场景 RML 中的按钮事件全部从 `data-command` 改为 `data-event-click`

### 验收标准
- 上述场景/HUD 不再直接使用 `RmlEventBridge`
- 上述场景/HUD 不再直接持有 `HoverFocusSyncListener`
- 场景代码中不再重复写 `registerTo(document_, "click")`
- 文档卸载前的 listener 清理由统一控制器负责

## Stage 2：Tooltip / Dialogue Bubble 收口为 Floating Widget

### 目标
- 把 tooltip 和 dialogue bubble 从“手写布局控件”改成“RmlUi 布局 + C++ 定位”

### 范围
- `src/game/ui/item_tooltip_ui.*`
- `src/game/ui/dialogue_bubble_view.*`
- `src/game/ui/dialogue_bubble_controller.*`
- `ui/rmlui/hud/item_tooltip.*`
- `ui/rmlui/hud/dialogue_bubble.*`

### 主要改动
- tooltip 与 bubble 文本改为绑定文本或直接更新内容
- `DialogueBubbleController` 不再负责按字符数插入换行；仅负责 speaker 前缀拼接、显隐和 world anchor 路由
- RCSS 中明确：
  - `max-width`
  - `white-space`
  - `word-break`
  - `line-height`
- 读取最终元素 `GetOffsetWidth/Height` 或等效布局结果作为定位边界
- 删除：
  - `wrapText()`
  - `syncStyleMetricsFromDocument()` 这类仅为手写布局服务的流程
  - 自定义字符数换行
  - 基于 `TextRenderer` 的 UI 宽高驱动逻辑
  - 与手写布局绑定的 font/padding/min-content 缓存成员

### 布局时序策略
- 对首次显示或需要同帧精确钳制的路径：更新文本后显式调用 `document_->UpdateDocument()`，再读取 `GetOffsetWidth/Height`
- 对持续刷新锚点的位置更新：允许使用上一轮布局结果，必要时在下一帧完成最终定位
- 不允许重新引入 `measureText()` 一类手写排版逻辑来规避布局时序问题

### 验收标准
- `ItemTooltipUI` 不再包含自定义换行器
- `DialogueBubbleController` 不再按字符数插入换行
- 文字表现只通过 RML/RCSS 调整
- `item_tooltip_ui.*` 与 `dialogue_bubble_view.*` 的复杂度明显下降，不再自行主导布局

## Stage 3：Slot Grid Presenter 抽象

### 目标
- 合并 HUD hotbar 与 inventory slot 类 UI 的重复实现

### 范围
- `src/game/ui/hotbar_ui.*`
- `src/game/scene/inventory_menu_scene.*`
- 相关 RML/RCSS

### 主要改动
- 新增统一的 slot view model/presenter
- 新增共享的基础 view model 定义，避免 `HotbarSlotViewModel` / `SlotViewModel` 多处重复定义
- 新增统一的 `populateSlotViewModel(...)` 或等价 helper，收敛 “stack -> decorator/count/flags” 投影逻辑
- 新增共享 drag-drop 状态对象，避免各自维护一套 `dragging_/drop_handled_/suppress_next_primary_mouse_up_`
- 把以下逻辑收敛为共享实现：
  - decorator/count/selected/active 状态投影
  - hover / focus / click / drag / drop 分发
  - tooltip 与 detail 联动入口
- Scene/HUD 只保留业务差异：
  - 左键行为
  - 右键行为
  - 拖拽落点规则
  - detail panel / action menu 的业务动作

### 边界约束
- presenter 只负责 slot 状态投影、drag state 管理、DOM 事件转发
- 具体 drop 后如何交换、装备、消费、取消，全部由外部策略/回调决定
- 不把业务规则塞进 presenter，避免抽象后再次膨胀

### 验收标准
- `HotbarUI` 与 `InventoryMenuScene` 不再各自注册一整套重复 slot 事件
- slot 结构命名统一
- 后续新增 slot 类页面时不需要再复制一套 drag/drop glue code
- “stack -> view model” 投影逻辑在项目内只有单一实现来源

## Stage 3B：Fade Overlay 与 UI 过渡样式化

### 目标
- 将 `RmlScreenFade` 从逐帧属性插值改为样式驱动过渡

### 范围
- `src/engine/ui/rmlui/rml_screen_fade.*`
- `ui/rmlui/overlay/screen_fade.*`

### 主要改动
- 在 RCSS 中声明 fade overlay 的默认过渡规则
- C++ 在每次 `fadeIn(seconds)` / `fadeOut(seconds)` 时动态设置过渡时长，并通过 class/property 切换触发 fade in / fade out
- 使用 `transitionend` 或等价完成事件推进 `phase`
- 删除：
  - `alpha_`
  - `from_alpha_`
  - `to_alpha_`
  - `duration_`
  - `timer_`
  - 逐帧 opacity 插值逻辑

### 验收标准
- `RmlScreenFade::update()` 不再负责逐帧 alpha 插值
- 视觉淡入淡出主要由 RCSS transition 驱动
- `phase()` 语义保持清晰，但内部状态机明显简化

## Stage 4：Inventory Menu 简化

### 目标
- 把 `InventoryMenuScene` 从“大一统巨型控制器”拆成清晰的状态投影与行为编排

### 范围
- `src/game/scene/inventory_menu_scene.*`
- `ui/rmlui/scenes/inventory_menu.*`

### 主要改动
- action menu 改为从实际 DOM 元素定位
- detail / selection / action menu / tooltip 的状态边界明确分层
- 删除无用字段与冗余状态
- 清理背包与 hotbar 之间重复的 slot 更新逻辑

### 重点清理项
- 删除未实际参与表现的 `highlighted_action_id_`
- 移除所有镜像 RCSS 的几何常量
- 将 action menu 的位置计算改为读取实际元素位置

### 验收标准
- `InventoryMenuScene` 的 UI 常量大幅减少
- action menu 改样式后不需要同步修改 C++
- 文件规模明显收缩，职责更清晰

## Stage 5：Theme 与样式体系收敛

### 目标
- 删除生产 RCSS 中的大量重复 reset / modal / navigation / slot 基础规则

### 范围
- `ui/rmlui/theme/*.rcss`
- `ui/rmlui/scenes/*.rcss`
- `ui/rmlui/hud/*.rcss`
- `ui/rmlui/overlay/*.rcss`
- `src/engine/ui/rmlui/rml_element_helpers.h`

### 主要改动
- 新建共享 theme 文件
- 基于 Stage 3 稳定后的 slot DOM 结构提炼 `slot_widgets.rcss`
- 替换重复 overlay/panel/button/nav/slot 规则
- 所有生产 RCSS 顶部只保留必要的特化样式
- 清理 `rml_element_helpers.h` 中仅为手写布局服务的 computed-style helper

### 验收标准
- 不再每个 RCSS 都重复写同一份 reset
- overlay / modal panel 的公共外观被抽到 theme
- `tab-index/nav-*` 通过共享 class 复用
- `rml_element_helpers.h` 收缩为最小必要工具集

## Stage 6：清理、测试与收尾

### 目标
- 删除旧桥接层、死代码、未使用状态
- 补齐结构性测试和回归清单

### 测试重点
- scene 按钮点击与导航
- hover->focus 同步
- tooltip 跟随与边界钳制
- dialogue bubble 文本与世界锚点定位
- inventory/hotbar 的 drag/drop 行为
- action menu 定位与焦点恢复

### 验收标准
- 不再引用已删除的桥接层
- UI 重构后 `ninja` 构建通过
- 关键交互路径至少有一轮手工回归

## 预计新增文件
- `src/engine/ui/rmlui/rml_document_controller.h`
- `src/engine/ui/rmlui/rml_document_controller.cpp`
- `src/game/ui/rml_slot_view_models.h`
- `src/game/ui/rml_drag_drop_state.h`
- `src/game/ui/rml_slot_grid_presenter.h`
- `src/game/ui/rml_slot_grid_presenter.cpp`
- `ui/rmlui/theme/reset.rcss`
- `ui/rmlui/theme/modal.rcss`
- `ui/rmlui/theme/slot_widgets.rcss`
- `ui/rmlui/theme/nav.rcss`

## 预计删除文件
- `src/engine/ui/rmlui/rml_event_bridge.h`
- `src/engine/ui/rmlui/rml_event_bridge.cpp`

## 预计重点修改文件
- `src/game/scene/title_scene.cpp`
- `src/game/scene/pause_menu_scene.cpp`
- `src/game/scene/rest_dialog_scene.cpp`
- `src/game/scene/battle_scene.cpp`
- `src/game/scene/save_slot_select_scene.cpp`
- `src/game/scene/inventory_menu_scene.h`
- `src/game/scene/inventory_menu_scene.cpp`
- `src/game/ui/game_scene_ui_controller.cpp`
- `src/game/ui/hotbar_ui.h`
- `src/game/ui/hotbar_ui.cpp`
- `src/game/ui/item_tooltip_ui.h`
- `src/game/ui/item_tooltip_ui.cpp`
- `src/game/ui/dialogue_bubble_view.h`
- `src/game/ui/dialogue_bubble_view.cpp`
- `src/game/ui/dialogue_bubble_controller.h`
- `src/game/ui/dialogue_bubble_controller.cpp`
- `src/engine/ui/rmlui/rml_screen_fade.h`
- `src/engine/ui/rmlui/rml_screen_fade.cpp`
- `src/engine/ui/rmlui/rml_element_helpers.h`
- `src/engine/ui/rmlui/hover_focus_sync_listener.h`
- `src/engine/ui/rmlui/hover_focus_sync_listener.cpp`
- `ui/rmlui/scenes/title.rml`
- `ui/rmlui/scenes/pause_menu.rml`
- `ui/rmlui/scenes/rest_dialog.rml`
- `ui/rmlui/scenes/battle.rml`
- `ui/rmlui/scenes/save_slot_select.rml`
- `ui/rmlui/scenes/inventory_menu.rml`
- `ui/rmlui/scenes/inventory_menu.rcss`
- `ui/rmlui/hud/hotbar.rml`
- `ui/rmlui/hud/hotbar.rcss`
- `ui/rmlui/hud/item_tooltip.rml`
- `ui/rmlui/hud/item_tooltip.rcss`
- `ui/rmlui/hud/dialogue_bubble.rml`
- `ui/rmlui/hud/dialogue_bubble.rcss`
- `ui/rmlui/overlay/screen_fade.rml`
- `ui/rmlui/overlay/screen_fade.rcss`

## 执行顺序建议
1. Stage 1
2. Stage 2
3. Stage 3B
4. Stage 3
5. Stage 4
6. Stage 5
7. Stage 6

说明：
- 先统一交互模型和文档生命周期，后面改组件时阻力最小。
- 先把 tooltip/bubble 收掉，能最快减少“布局逻辑写在 C++”的问题。
- fade overlay 紧随其后处理，可以尽早把“C++ 逐帧做视觉插值”的模式从架构中清掉。
- slot presenter 先稳定，再做 `slot_widgets.rcss` 的最终收敛，可以避免共享样式跟着抽象一起返工。
- `InventoryMenuScene` 最后单独做，因为它是重复逻辑和业务逻辑最密集的点。

## 待办清单
- [x] T1 新增 `RmlDocumentController`，采用成员组合方式并提供 `load/bindEvent/bindModel/setDefaultFocus/enableHoverFocusSync/show/hide/unload`
- [x] T2 将 hover->focus 注册与解绑迁移到 `RmlDocumentController` 内部
- [x] T3 全量迁移简单场景到 `data-event-* + BindEventCallback`
- [x] T4 删除 `RmlEventBridge`
- [x] T5 tooltip 改为 RmlUi 原生文本布局，并明确布局刷新时序
- [x] T6 dialogue bubble 改为 RmlUi 原生文本布局，并删除 `MAX_CHARS_PER_LINE` 风格换行
- [ ] T7 `RmlScreenFade` 改为动态 transition 驱动
- [ ] T8 收缩 `rml_element_helpers.h` 到最小必要工具集
- [ ] T9 抽出共享 slot view model / `populateSlotViewModel(...)` / drag-drop state
- [ ] T10 抽出 `RmlSlotGridPresenter`
- [ ] T11 合并 HUD hotbar / inventory slot 重复逻辑
- [ ] T12 inventory action menu 改为基于 DOM 几何定位
- [ ] T13 收敛 theme/reset/modal/nav/slot 样式
- [ ] T14 删除死字段与废弃 glue code
- [ ] T15 用 `ninja` 完成构建并做关键交互回归

## DoD
- 项目内不再存在 `data-command + RmlEventBridge` 交互链路。
- tooltip / dialogue bubble 不再包含手写文本换行与宽高计算。
- `RmlScreenFade` 不再依赖 C++ 逐帧 alpha 插值驱动视觉淡入淡出。
- HUD hotbar 与 inventory slot 的重复 presenter 逻辑被统一。
- `stack -> slot view model` 投影逻辑只有单一实现来源。
- `InventoryMenuScene` 不再维护镜像 RCSS 的几何常量。
- 生产 RCSS 的 reset / modal / nav / slot 基础规则被集中到 theme。
- `rml_element_helpers.h` 不再保留仅为手写布局服务的大量 computed-style helper。
- 最终架构中，C++ 主要负责状态和命令，RML/RCSS 主要负责布局和显隐。

## 风险与缓解
- 风险：slot 抽象过度，反而引入新的复杂度。
  - 缓解：只抽“状态投影 + 事件分发”这一层，不抽业务规则。
- 风险：tooltip/bubble 迁移后文本表现与旧版略有差异。
  - 缓解：先用视觉回归清单锁定宽度、行高、边缘钳制行为。
- 风险：位图字体 `VonwaonBitmap 16px` 在 CJK / `word-break` 场景下的自动换行表现不符合预期。
  - 缓解：先使用 `rmlui_tester` 或等价最小场景验证位图字体下的自动换行、行高和断词效果，必要时通过 RCSS 与文案组织调整，不重新引入 C++ 手写换行。
- 风险：一次性删除旧桥接层，迁移中间阶段会有编译波动。
  - 缓解：按 Stage 小步推进，每阶段结束后立即构建。

## 备注
- 本计划默认采用最优最终结构，不保留兼容适配层。
- 若后续确认商店/任务/战斗菜单也采用 RmlUi，则本计划中的 `RmlDocumentController` 与 `RmlSlotGridPresenter` 将直接作为复用基底。
