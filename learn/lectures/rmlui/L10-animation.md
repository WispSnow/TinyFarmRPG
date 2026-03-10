# L10: 动画、变换与过渡

> 配套代码：`learn/rmlui_animation/` | 构建目标：`learn_rmlui_animation`

---

## 1. 为什么需要 UI 动画？

静态 UI 能传达信息，但动态 UI 能传达**状态变化**：
- 菜单项悬停时平滑高亮，比瞬间切换更直观
- 伤害数字上飘淡出，比直接消失更有反馈感
- 窗口弹出/收起动画，让界面层次更清晰

RmlUi 提供三套动画机制：**过渡（Transition）**、**关键帧动画（Animation）**、**变换（Transform）**。

---

## 2. Transition — 属性过渡

当元素属性值发生变化时（通常由伪类触发），`transition` 会让变化平滑过渡而非瞬间跳变。

### 语法

```css
transition: <property-names> <duration> [timing-function] [delay];
```

| 参数 | 说明 | 示例 |
|------|------|------|
| property-names | 空格分隔的属性名，或 `all` | `background-color padding-left color` |
| duration | 过渡时长（秒） | `0.3s` |
| timing-function | 缓动函数（可选，默认 linear） | `cubic-out` |
| delay | 延迟时间（可选） | `0.5` |

### 示例

```css
.menu-item {
    padding-left: 8dp;
    background-color: transparent;
    color: #c0caf5;

    transition: background-color padding-left color 0.3s cubic-out;
}
.menu-item:hover {
    padding-left: 16dp;
    background-color: #2d335480;
    color: #7aa2f7;
}
```

悬停时三个属性同时平滑过渡，移出时自动反向过渡。

> **重要**：RmlUi 的 `transition` 语法与标准 CSS 不同！
> CSS 用逗号分隔多个过渡（每个有独立时长），
> RmlUi 用空格列出属性名、共享一个时长和缓动。

### 可过渡属性

| 类别 | 属性 |
|------|------|
| 尺寸 | `width`, `height`, `margin-*`, `padding-*`, `border-*-width`, `top`, `right`, `bottom`, `left` |
| 颜色 | `color`, `background-color`, `border-*-color`, `image-color` |
| 视觉 | `opacity` |
| 变换 | `transform` |

---

## 3. @keyframes — 关键帧动画

关键帧动画可以定义多阶段的复杂动画序列。

### 定义关键帧

```css
@keyframes damage-popup {
    from {
        transform: translateY(0);
        opacity: 1;
    }
    to {
        transform: translateY(-40dp);
        opacity: 0;
    }
}
```

可以使用百分比定义中间帧：

```css
@keyframes bounce {
    from { transform: translateY(0); }
    25%  { transform: translateY(-8dp); }
    50%  { transform: translateY(0); }
    75%  { transform: translateY(-4dp); }
    to   { transform: translateY(0); }
}
```

### 播放动画

```css
animation: <duration> [timing-function] [iterations] [direction] <name>;
```

| 参数 | 说明 | 示例 |
|------|------|------|
| duration | 动画时长（秒） | `2s` |
| timing-function | 缓动函数（可选） | `cubic-in-out` |
| iterations | 播放次数或 `infinite` | `3` / `infinite` |
| direction | `alternate` 交替方向 | `alternate` |
| name | @keyframes 名称 | `bounce` |

```css
.spin-box {
    animation: 2s linear infinite spin;
}
.pulse-box {
    animation: 1s cubic-in-out infinite pulse;
}
```

> 注意：`animation` 中的参数顺序比较灵活，RmlUi 会自动识别数值（时长）、
> 关键字（timing/infinite/alternate）和名称。

---

## 4. 缓动函数（Timing Functions）

RmlUi 提供丰富的预定义缓动函数，格式为 `type-direction`：

| 类型 | In | Out | InOut |
|------|-----|------|-------|
| linear | linear-in | linear-out | linear-in-out |
| quadratic | quadratic-in | quadratic-out | quadratic-in-out |
| cubic | cubic-in | cubic-out | cubic-in-out |
| quartic | quartic-in | quartic-out | quartic-in-out |
| quintic | quintic-in | quintic-out | quintic-in-out |
| sine | sine-in | sine-out | sine-in-out |
| circular | circular-in | circular-out | circular-in-out |
| exponential | exponential-in | exponential-out | exponential-in-out |
| back | back-in | back-out | back-in-out |
| bounce | bounce-in | bounce-out | bounce-in-out |
| elastic | elastic-in | elastic-out | elastic-in-out |

### 常用缓动对照

| 用途 | 推荐缓动 |
|------|----------|
| 通用 UI 过渡 | `cubic-out` |
| 弹性效果 | `elastic-out` |
| 弹跳效果 | `bounce-out` |
| 平滑循环 | `cubic-in-out` |
| 淡出 | `cubic-in` |
| 淡入 | `cubic-out` |

> **注意**：RmlUi 不支持 CSS 的 `cubic-bezier()` 自定义曲线，
> 也不支持 `ease` / `ease-in` / `ease-out` 关键字。
> 使用 RmlUi 特有的命名缓动函数。

---

## 5. Transform — 变换

`transform` 属性可以对元素应用 2D/3D 几何变换，不影响文档流。

### 2D 变换函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `translateX(v)` | 水平平移 | `translateX(100dp)` |
| `translateY(v)` | 垂直平移 | `translateY(-40dp)` |
| `translate(x, y)` | 平移 | `translate(10dp, 20dp)` |
| `rotate(a)` | 旋转 | `rotate(45deg)` |
| `scaleX(s)` | 水平缩放 | `scaleX(1.5)` |
| `scaleY(s)` | 垂直缩放 | `scaleY(0.8)` |
| `scale(s)` | 等比缩放 | `scale(1.2)` |
| `scale(sx, sy)` | 非等比缩放 | `scale(1.5, 0.8)` |
| `skewX(a)` | 水平倾斜 | `skewX(30deg)` |
| `skewY(a)` | 垂直倾斜 | `skewY(-15deg)` |
| `skew(ax, ay)` | 倾斜 | `skew(30deg, 0deg)` |

### 3D 变换函数

| 函数 | 说明 |
|------|------|
| `perspective(d)` | 设置透视距离 |
| `translateZ(v)` / `translate3d(x,y,z)` | 深度平移 |
| `rotateX(a)` / `rotateY(a)` / `rotateZ(a)` | 轴向旋转 |
| `rotate3d(x,y,z,a)` | 任意轴旋转 |
| `scaleZ(s)` / `scale3d(sx,sy,sz)` | 深度缩放 |

### 组合变换

多个变换函数空格分隔，按从左到右的顺序依次应用：

```css
transform: translateY(-40dp) rotate(10deg) scale(1.2);
```

---

## 6. transform-origin — 变换原点

默认变换原点在元素中心（`50% 50%`）。可以改变原点位置：

```css
transform-origin: 50% 0%;    /* 顶部中心 */
transform-origin: left top;   /* 左上角 */
transform-origin: 30% 80% 0;  /* 自定义位置 */
```

本课的窗口弹出动画使用 `transform-origin: 50% 0%`，
让缩放动画从顶部展开而非中心展开。

---

## 7. opacity — 透明度

`opacity` 控制元素（及其子元素）的透明度，范围 0.0~1.0：

```css
@keyframes fadeout {
    from { opacity: 1; }
    to   { opacity: 0; }
}
```

`opacity` 可以同时参与 transition 和 animation，
是实现淡入淡出效果的核心属性。

---

## 8. 演示场景解析

### 场景结构

```
AnimationScene
├── setupDataModel()     → 绑定 damage_value / window_visible / status_text
├── on_hit callback      → 触发伤害数字弹出（CSS class 切换）
├── toggle_window callback → 窗口打开/关闭动画
└── update(dt)           → 检测关闭动画完成后隐藏元素
```

### 三列演示内容

```
┌─────────────┬──────────────┬──────────────┐
│ Transitions │ @keyframes   │ Interactive  │
│             │              │              │
│ [New Game]  │ ↻ spin       │ 伤害数字区域  │
│ [Continue]  │ ♥ pulse      │ [Attack!]    │
│ [Options]   │ ▲ bounce     │              │
│ [Exit]      │ ! blink      │ [Toggle Win] │
│             │              │ ┌──────────┐ │
│ [Color Btn] │              │ │ popup    │ │
│ [Stretch]   │              │ └──────────┘ │
├─────────────┴──────────────┴──────────────┤
│ status bar                                │
└───────────────────────────────────────────┘
```

### 通过 CSS class 切换重播动画

RmlUi 的 `animation` 在元素创建或 class 变化时开始播放。
要重播动画（如每次点击 Attack 都弹出伤害数字），需要：

```cpp
// 1. 移除动画 class
dmg_el->SetClassNames("dmg-text");
// 2. 重新添加动画 class → 触发重播
dmg_el->SetClassNames("dmg-text dmg-animate");
```

对应 RCSS：

```css
.dmg-text {
    opacity: 0;        /* 默认不可见 */
}
.dmg-animate {
    animation: 0.8s cubic-out damage-popup;
}
```

---

## 9. 窗口打开/关闭动画

本课实现了一个常见的游戏 UI 模式：窗口弹出和收起。

### @keyframes 定义

```css
@keyframes window-open {
    from { transform: scale(0.8); opacity: 0; }
    to   { transform: scale(1.0); opacity: 1; }
}

@keyframes window-close {
    from { transform: scale(1.0); opacity: 1; }
    to   { transform: scale(0.8); opacity: 0; }
}
```

### CSS class 切换

```css
.popup-panel { display: none; transform-origin: 50% 0%; }
.popup-open  { animation: 0.3s cubic-out window-open; }
.popup-close { animation: 0.3s cubic-in window-close; }
```

### C++ 侧控制

打开时设置 `display: block` + 添加 `popup-open` class；
关闭时添加 `popup-close` class，等动画播完后设置 `display: none`。

---

## 10. C++ 程序化动画 API

除了 RCSS 声明式动画，RmlUi 还提供 C++ API 直接驱动动画：

```cpp
// 直接从当前值过渡到目标值
element->Animate("opacity", Rml::Property(0.0f, Rml::Unit::NUMBER),
                 0.5f,                         // 时长
                 Rml::Tween{Rml::Tween::Cubic, Rml::Tween::Out});

// 多关键帧
element->Animate("transform", startTransform, 1.0f,
                 Rml::Tween{Rml::Tween::Elastic, Rml::Tween::InOut},
                 -1,     // iterations: -1 = infinite
                 true);  // alternate direction
element->AddAnimationKey("transform", midTransform, 0.5f);
```

程序化 API 适用于需要根据运行时数据动态计算目标值的场景，
如技能冷却旋转、经验条增长等。

---

## 11. 动画事件

动画/过渡完成时会触发事件：

| 事件 | 说明 |
|------|------|
| `animationend` | 关键帧动画播放结束 |
| `transitionend` | 过渡完成 |

可在 RML 中监听：

```html
<div onanimationend="on_anim_done">...</div>
```

或在 C++ 中通过 `AddEventListener` 监听。

---

## 12. 练习

### 练习 1：菜单选中动画

为菜单项添加 `:focus` 或 `:checked` 伪类样式，
配合 `transition` 实现键盘导航时的选中动画效果。

### 练习 2：连击伤害

修改 `on_hit` 回调，每次点击产生不同位置的伤害数字，
使用 C++ `Animate` API 为每个数字设置随机偏移。

### 练习 3：呼吸灯效果

用 `@keyframes` 实现 `box-shadow` 或 `border-color` 的呼吸灯效果，
应用到选中的面板边框上（提示：`border-color` 可以参与过渡）。

---

## 13. 要点回顾

| 概念 | 要点 |
|------|------|
| `transition` | `property1 property2 duration timing [delay]`，所有属性共享时长 |
| RmlUi vs CSS | RmlUi 用空格列属性、共享时长；CSS 用逗号分隔多个独立过渡 |
| `@keyframes` | `from {} to {}` 或百分比关键帧 |
| `animation` | `duration [timing] [iterations] [alternate] name` |
| 缓动函数 | `type-direction` 格式，如 `cubic-out` / `elastic-in-out` |
| 不支持 | `cubic-bezier()` / `ease` / `ease-in` / `ease-out` |
| `transform` | `translate` / `rotate` / `scale` / `skew` + 3D 变体 |
| `transform-origin` | 变换原点，默认 `50% 50%` |
| `opacity` | 0.0~1.0，可参与过渡和动画 |
| 重播动画 | 移除再添加 CSS class 以触发 `animation` 重播 |
| 程序化 API | `Element::Animate()` / `AddAnimationKey()` |
| 动画事件 | `animationend` / `transitionend` |

---

**上一课 <-** [L09: 精灵表与九宫格装饰器](L09-spritesheet.md)
**下一课 ->** L11: 滤镜、阴影与视觉特效（待完成）
