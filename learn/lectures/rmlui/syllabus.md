# RmlUi 教学大纲 — 从零到 JRPG 完整 UI

> 基于 RmlUi 6.2 + TinyFarmRPG 引擎集成
> 目标：系统掌握 RmlUi，最终实现一套完整的 JRPG（RPGMaker 风格）游戏界面

---

## 课程总览

| 阶段 | 课程 | 主题 |
|------|------|------|
| **I. 基础** | L01–L04 | RmlUi 文档结构、盒模型、布局、样式 |
| **II. 交互** | L05–L08 | 事件、数据绑定、表单控件、自定义元素 |
| **III. 视觉** | L09–L11 | 精灵表与九宫格、动画与变换、滤镜特效 |
| **IV. JRPG 实战** | L12–L16 | 窗口框架、对话、战斗、物品装备、菜单系统 |

每课包含：知识讲解 + 动手练习（在 `learn/` 目录下独立构建）

---

## Phase I — RmlUi 基础

### L01: 文档结构与第一个界面

**目标**：理解 RML 文档的组成方式，能编写并加载一个基本页面。

**知识点**：
- RML 文档结构：`<rml>` / `<head>` / `<body>`
- RCSS 样式表引入：`<link type="text/rcss" href="...">`
- 基础元素：`<div>`, `<p>`, `<h1>`–`<h4>`, `<span>`, `<img>`, `<br>`
- 当前 learn 集成入口：`Scene::loadRmlDocument()` / `Context::getRmlUi()`
- RmlUi Debugger 的使用（元素检查、样式查看）

**练习**：`learn/rmlui_basics` — 已完成的 Hello World 页面


### L02: 盒模型与定位

**目标**：掌握 RCSS 盒模型和定位机制，能精确控制元素位置和尺寸。

**知识点**：
- 盒模型：`margin` / `padding` / `border` / `width` / `height`
- `box-sizing`: `content-box` vs `border-box`
- 尺寸单位：`dp`（密度无关像素）、`%`、`em`、`px`
- 定位模式：`static` / `relative` / `absolute` / `fixed`
- `top` / `right` / `bottom` / `left` / `z-index`
- `display`: `block` / `inline` / `inline-block` / `none`
- `overflow` 与滚动条

**练习**：制作一个绝对定位的信息卡片，包含头像图片、名称、描述文字，精确控制间距


### L03: Flexbox 布局

**目标**：使用 Flexbox 实现各种常见布局模式。

**知识点**：
- Flex 容器：`display: flex`
- 方向：`flex-direction`（row / column / row-reverse / column-reverse）
- 换行：`flex-wrap`
- 主轴对齐：`justify-content`（flex-start / center / space-between / space-around / space-evenly）
- 交叉轴对齐：`align-items` / `align-self` / `align-content`
- 弹性分配：`flex-grow` / `flex-shrink` / `flex-basis` / `flex` 简写
- 间距：`gap` / `row-gap` / `column-gap`

**练习**：用纯 Flexbox 实现以下三种布局：
1. 水平等分按钮栏（模拟快捷栏）
2. 垂直菜单列表（模拟主菜单）
3. 网格卡片排列（模拟背包物品格子，flex-wrap）


### L04: 文字排版与视觉样式

**目标**：掌握字体、颜色、边框、背景等核心视觉属性。

**知识点**：
- 字体属性：`font-family` / `font-size` / `font-weight` / `font-style`
- 文字控制：`text-align` / `text-decoration` / `text-transform` / `line-height`
- 文字溢出：`text-overflow`（ellipsis）、`white-space` / `word-break`
- 颜色值：命名色、`#RRGGBB` / `#RRGGBBAA`、`rgb()` / `rgba()`、`hsl()`
- 边框：`border` / `border-radius`
- 背景：`background-color`（RmlUi 无 background-image，通过 decorator 实现）
- 透明度：`opacity`
- 装饰器入门：`decorator: image(...)` 基础用法

**练习**：设计一个 JRPG 风格的角色简介卡：圆角边框、半透明深色背景、角色名（粗体亮色）、职业（小号灰色）、描述文字

---

## Phase II — C++ 动态交互

### L05: 事件系统

**目标**：理解 RmlUi 事件模型，能在 C++ 中响应 UI 交互。

**知识点**：
- DOM 事件流：捕获阶段 → 目标阶段 → 冒泡阶段
- 常用事件：`click` / `mouseover` / `mouseout` / `focus` / `blur` / `keydown` / `change`
- C++ `Rml::EventListener` 接口：`ProcessEvent(Rml::Event& event)`
- 注册监听：`element->AddEventListener("click", listener)`
- 事件参数读取：`event.GetParameter<T>("key", default)`
- 内联事件绑定：`onclick="..."` (仅用于简单场景)
- 事件传播控制：`StopPropagation()` / `StopImmediatePropagation()`

**练习**：创建 3 个按钮，点击时通过 C++ EventListener 打印不同日志；实现鼠标悬停时按钮变色（纯 RCSS `:hover`）+ 点击音效播放（C++ 端）


### L06: 数据绑定

**目标**：掌握 RmlUi 的 data model 系统，实现 C++ 数据到 UI 的自动同步。

**知识点**：
- 数据模型创建：`context->CreateDataModel("model_name")`
- 标量绑定：`constructor.Bind("hp", &hp)`
- 文本插值：`<p>HP: {{hp}} / {{max_hp}}</p>`
- 条件显示：`data-if="hp > 0"`
- 列表渲染：`data-for="item : items"`（数组绑定）
- 结构体绑定：`constructor.RegisterStruct<T>()` + 成员注册
- 事件回调：`data-event-click="on_attack"`
- 脏标记：`model.DirtyVariable("hp")` / `DirtyAllVariables()`
- 管道（Transform）函数：`{{gold | format_number}}`

**练习**：
1. 绑定一个角色数据结构（名称/HP/MP/等级），实时显示在 UI 上
2. 用 `data-for` 渲染一个技能列表，点击技能触发 `data-event-click` 回调


### L07: 表单控件

**目标**：使用 RmlUi 内置表单控件构建交互式界面。

**知识点**：
- 文本输入：`<input type="text">` / `<input type="password">`
- 多行文本：`<textarea>`
- 复选框：`<input type="checkbox">`
- 单选按钮：`<input type="radio">`（分组）
- 下拉选择：`<select>` + `<option>`
- 滑块：`<input type="range">`（`min` / `max` / `step`）
- 进度条：`<progress>`
- 标签页：`<tabset>` / `<tab>` / `<tab-content>`
- 表单提交：`<form>` + `submit` 事件
- 数据绑定与表单结合：`data-value`

**练习**：构建一个「游戏设置」面板：
- 音量滑块（BGM / SE）
- 文字速度下拉（慢/中/快）
- 全屏切换复选框
- 按键绑定输入框
- 确认 / 取消按钮


### L08: 自定义元素与文档管理

**目标**：创建可复用的自定义元素，掌握多文档管理。

**知识点**：
- `Rml::ElementInstancer` 与 `Rml::ElementInstancerGeneric<T>`
- 自定义元素注册：`Rml::Factory::RegisterElementInstancer("tag", ...)`
- 自定义元素生命周期：`OnChildAdd` / `OnChildRemove` / `OnAttributeChange`
- 自定义属性响应
- 多文档管理：`context->LoadDocument()` / `UnloadDocument()`
- 文档层级：`PullDocumentToFront()` / `PushDocumentToBack()`
- DOM 查询：`GetElementById()` / `QuerySelector()` / `QuerySelectorAll()`
- 动态 DOM 操作：`SetInnerRML()` / `AppendChild()` / `RemoveChild()`

**练习**：
1. 创建自定义 `<hp-bar>` 元素：接受 `value` / `max` 属性，内部渲染填充条
2. 实现两层文档：底层为游戏 HUD，顶层为可显示/隐藏的菜单面板

---

## Phase III — 视觉增强

### L09: 精灵表与九宫格装饰器

**目标**：使用 spritesheet 和 tiled 装饰器构建可缩放的 UI 皮肤。

**知识点**：
- `@spritesheet` 声明：定义精灵图集与各精灵矩形区域
- `resolution` 属性：高 DPI 适配
- 装饰器类型：
  - `image(sprite)` — 单张图片
  - `tiled-horizontal(left, center, right)` — 水平三段拉伸
  - `tiled-vertical(top, center, bottom)` — 垂直三段拉伸
  - `tiled-box(...)` — 九宫格（四角 + 四边 + 中心）
- 装饰器与伪类结合：不同状态下切换装饰器
- 渐变装饰器：`linear-gradient` / `radial-gradient`
- `image-color`：图片染色

**练习**：
1. 制作一套 JRPG 九宫格窗口皮肤（深蓝底 + 金色边框）
2. 用该皮肤创建不同尺寸的面板，验证自适应拉伸效果
3. 为按钮制作 normal / hover / active 三态精灵切换


### L10: 动画、变换与过渡

**目标**：为 UI 添加流畅的动态效果。

**知识点**：
- 过渡（Transition）：
  - `transition: property duration [delay] [timing-function]`
  - 多属性过渡 / `transition: all`
  - 缓动函数：`ease` / `ease-in` / `ease-out` / `cubic-bezier()`
- 关键帧动画（Animation）：
  - `@keyframes name { from {} to {} }` / 百分比关键帧
  - `animation: name duration [iterations] [alternate]`
  - `animation-play-state: paused / running`
- 变换（Transform）：
  - `transform: translate() / rotate() / scale() / skew()`
  - `transform-origin`
- `transitionend` / `animationend` 事件

**练习**：
1. 菜单项悬停：平滑放大 + 颜色过渡
2. 伤害数字弹出动画：从目标位置上移 + 淡出（keyframe）
3. 窗口打开/关闭：scale 从 0.8→1.0 + opacity 渐显


### L11: 滤镜、阴影与视觉特效

**目标**：运用 RmlUi 的 GL3 滤镜管线实现高级视觉效果。

**知识点**：
- `filter` 属性：`blur()` / `brightness()` / `contrast()` / `grayscale()` / `sepia()` / `drop-shadow()` / `hue-rotate()`
- `backdrop-filter`：对元素背景区域施加滤镜（毛玻璃效果）
- `box-shadow`：普通阴影 / `inset` 内阴影
- `mask-image`：遮罩裁切
- 滤镜动画：`filter` 属性可参与 transition / animation
- 性能注意事项：滤镜渲染开销与 box-shadow 缓存

**练习**：
1. 对话窗口的毛玻璃背景（`backdrop-filter: blur()`）
2. 中毒状态下角色头像变绿（`filter: hue-rotate() saturate()`）
3. 聚焦面板的外发光效果（`box-shadow`）

---

## Phase IV — JRPG 界面实战

> 此阶段将前三个阶段的知识整合，实现 RPGMaker 风格的完整游戏界面。
> 每课产出一个可运行的 learn target。

### L12: JRPG 窗口框架与键盘导航

**目标**：建立 JRPG UI 的基础设施——可复用的窗口皮肤系统和键盘/手柄导航。

**内容**：
- **窗口皮肤系统**
  - 九宫格窗口模板（`.window` 类 + tiled-box 装饰器）
  - 标题栏区域 + 内容区域的标准结构
  - 不同用途的变体：对话框、菜单、提示框
- **键盘/手柄导航**（JRPG 核心交互）
  - `nav-up` / `nav-down` / `nav-left` / `nav-right` 空间导航
  - `tab-index` 焦点顺序
  - `:focus` / `:focus-visible` 样式
  - 模拟方向键在菜单项之间移动
- **窗口打开/关闭动画**
  - 淡入淡出 + 缩放动画
  - `display` 属性动画支持

**练习**：创建一个可复用的 `.rcss` 窗口主题文件 + 键盘可导航的垂直菜单


### L13: 对话系统

**目标**：实现完整的 JRPG 对话界面。

**内容**：
- **对话窗口布局**
  - 角色头像区域（左侧 / 大立绘）
  - 角色名标签（上方独立小窗）
  - 文字显示区域（自动换行）
- **打字机效果**
  - C++ 端逐字更新 `SetInnerRML()` 或 data binding
  - 光标闪烁动画（`@keyframes blink`）
  - 按键跳过（立即显示全文）
- **选项分支**
  - `data-for` 渲染选项列表
  - 键盘导航选择 + 确认
  - 选中状态高亮（`:focus` + 指针图标）
- **与引擎对接**
  - 数据绑定：`speaker_name` / `dialogue_text` / `choices[]`
  - 事件回调：`data-event-click="on_choice_select"`

**练习**：完整的对话场景——NPC 对话 → 打字机文字 → 出现选项 → 选择后切换文字


### L14: 战斗界面

**目标**：实现回合制 RPG 的战斗 UI。

**内容**：
- **角色状态面板**
  - HP/MP 条：自定义 `<hp-bar>` 元素或 `<progress>` + 装饰器
  - 数值显示：`{{hp}} / {{max_hp}}`
  - 状态异常图标行（中毒/麻痹/…）：`data-for` + 精灵图
  - 多角色排列（Flex 水平排列，每人一列）
- **行动命令菜单**
  - 竖向命令列表：攻击 / 技能 / 道具 / 防御 / 逃跑
  - 键盘导航 + 当前选中指示器
  - 子菜单展开（技能列表 / 道具列表）
  - 多层菜单的焦点管理
- **技能/道具选择子面板**
  - 带 MP 消耗 / 物品数量的列表
  - 详情 tooltip / 描述区域
  - 不可用项（MP 不足）灰显：`data-if` + `:disabled` 样式
- **战斗反馈**
  - 伤害数字弹出动画（`@keyframes float-up`）
  - 受击闪烁（`@keyframes flash`）
  - 回合指示文字

**练习**：4 人队伍的战斗 UI：底部状态栏 + 左侧命令菜单 + 技能子菜单（全键盘操作）


### L15: 物品与装备系统

**目标**：实现背包管理和装备界面。

**内容**：
- **背包界面**
  - 网格布局（`flex-wrap` 实现 N×M 格子）
  - 物品图标（精灵表）+ 数量角标
  - 分类标签页（自定义 tab 按钮组）：全部 / 消耗品 / 装备 / 关键道具
  - 滚动支持（`overflow: auto`）
  - 物品详情区域：名称、图标、描述、效果
- **装备界面**
  - 角色预览区 + 属性面板
  - 装备槽位布局（武器/盾牌/头盔/铠甲/饰品）
  - 装备更换：选中槽位 → 弹出可装备列表 → 属性对比
  - 属性变化预览：攻击力 +3 ↑（绿色）/ -2 ↓（红色）
- **物品使用**
  - 使用按钮 + 目标选择
  - 使用后数量更新（data binding 自动刷新）

**练习**：背包标签页 + 物品详情 + 装备槽面板，数据通过 data model 绑定


### L16: 菜单系统整合

**目标**：实现完整的 JRPG 菜单体系，整合所有 UI 子系统。

**内容**：
- **主菜单（标题画面）**
  - 游戏 Logo / 标题
  - 新游戏 / 继续 / 设置 按钮
  - 背景装饰动画
- **游戏内菜单（暂停菜单）**
  - 左侧导航栏：物品 / 装备 / 技能 / 状态 / 任务 / 存档 / 设置
  - 右侧内容区：根据选中项切换文档或子面板
  - 角色切换（多角色时的 tab/arrow 切换）
- **商店界面**
  - 买入/卖出标签页
  - 商品列表 + 价格 + 持有数量
  - 数量选择（`<input type="range">` 或 +/- 按钮）
  - 金币余额显示
- **任务日志**
  - 进行中 / 已完成 分组
  - 任务列表（`data-for`）+ 选中展开详情
  - 任务目标检查列表
- **存档/读档**
  - 存档槽列表（3~5 个）
  - 槽位预览：角色名、等级、游戏时间、截图缩略图
  - 覆盖确认弹窗（模态对话框）
- **设置面板**
  - 音量滑块 / 文字速度 / 全屏切换
  - 恢复默认按钮

**练习**：完整的游戏内菜单骨架（多文档切换模式），包含至少 3 个可键盘导航的子面板

---

## 附录

### A. 每课对应的 learn target 命名规范

| 课程 | target 名 | 目录 |
|------|-----------|------|
| L01 | `learn_rmlui_basics` | `learn/rmlui_basics/` |
| L02 | `learn_rmlui_box_model` | `learn/rmlui_box_model/` |
| L03 | `learn_rmlui_flexbox` | `learn/rmlui_flexbox/` |
| L04 | `learn_rmlui_styling` | `learn/rmlui_styling/` |
| L05 | `learn_rmlui_events` | `learn/rmlui_events/` |
| L06 | `learn_rmlui_data_binding` | `learn/rmlui_data_binding/` |
| L07 | `learn_rmlui_forms` | `learn/rmlui_forms/` |
| L08 | `learn_rmlui_custom_elements` | `learn/rmlui_custom_elements/` |
| L09 | `learn_rmlui_spritesheet` | `learn/rmlui_spritesheet/` |
| L10 | `learn_rmlui_animation` | `learn/rmlui_animation/` |
| L11 | `learn_rmlui_filters` | `learn/rmlui_filters/` |
| L12 | `learn_jrpg_window` | `learn/jrpg_window/` |
| L13 | `learn_jrpg_dialogue` | `learn/jrpg_dialogue/` |
| L14 | `learn_jrpg_battle` | `learn/jrpg_battle/` |
| L15 | `learn_jrpg_inventory` | `learn/jrpg_inventory/` |
| L16 | `learn_jrpg_menu` | `learn/jrpg_menu/` |

### B. JRPG 界面 RmlUi 特性映射

| JRPG 界面需求 | 对应 RmlUi 特性 |
|---------------|----------------|
| 窗口边框皮肤 | `@spritesheet` + `tiled-box` 装饰器 |
| 菜单键盘导航 | `nav-up/down/left/right` + `tab-index` |
| HP/MP 条 | `<progress>` 或自定义元素 + 装饰器 |
| 物品格子 | Flexbox `flex-wrap` + `data-for` |
| 对话打字机 | C++ 定时 `SetInnerRML()` / data binding |
| 选项分支 | `data-for` + `data-event-click` |
| 伤害数字 | `@keyframes` 动画 + 动态创建元素 |
| 状态图标 | `@spritesheet` 精灵 + `data-for` |
| 菜单子面板 | 多文档 / `data-if` 条件显示 |
| 半透明背景 | `background-color` alpha / `backdrop-filter` |
| 属性增减色 | `data-if` 条件 class + 颜色样式 |
| 模态弹窗 | 文档层级 + 输入遮挡层 |
| 音量滑块 | `<input type="range">` + `data-value` |

### C. 参考资源

- RmlUi 6.2 源码：`external/RmlUi-6.2/`
- RmlUi 官方文档：https://mikke89.github.io/RmlUiDoc/
- 引擎 RmlUi 集成层：`src/engine/ui/rmlui/`
- learn 文档加载入口：`src/engine/scene/scene.h` — `loadRmlDocument()`
- 共享 RmlUi 运行时：`src/engine/ui/rmlui/rml_ui_runtime.h`
- RmlUi Debugger：运行时按 F8 或通过 `Rml::Debugger::SetVisible(true)` 启用
- RmlUi 官方示例：`external/RmlUi-6.2/Samples/`（animation / data_binding / drag / effects / invaders 等）
