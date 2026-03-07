# L09: 精灵表与九宫格装饰器

> 配套代码：`learn/rmlui_spritesheet/` | 构建目标：`learn_rmlui_spritesheet`

---

## 1. 为什么需要 Spritesheet？

游戏 UI 通常使用大量小尺寸纹理（按钮、边框、图标等）。
如果每个精灵单独加载一张纹理，会导致：
- 大量 GPU 纹理切换，影响渲染性能
- 文件数量激增，资源管理复杂

**精灵表（Spritesheet）** 将多个精灵合并到一张大纹理中，
通过矩形坐标定位每个精灵，一次纹理绑定即可渲染全部 UI 元素。

---

## 2. @spritesheet 声明

RmlUi 使用 `@spritesheet` 在 RCSS 中声明精灵图集：

```css
@spritesheet my-sprites {
    src: path/to/spritesheet.png;
    resolution: 1x;

    /* sprite-name: x y width height; */
    btn-normal:  0px  0px 64px 16px;
    btn-hover:   0px 16px 64px 16px;
    icon-star:  64px  0px 16px 16px;
}
```

**关键属性**：

| 属性 | 说明 |
|------|------|
| `src` | 纹理文件路径（相对于 RCSS 文件位置） |
| `resolution` | 像素密度，`1x` 表示 1:1，`2x` 用于 Retina/高 DPI |
| sprite 定义 | `name: x y width height;` 定义矩形区域 |

> 路径是相对于 RCSS 文件的。本课中 RCSS 在 `ui/rmlui/learn/`，
> 纹理在 `assets/textures/UI/`，所以路径为 `../../../assets/textures/UI/xxx.png`。

---

## 3. 装饰器概览

RmlUi 的 **装饰器（Decorator）** 是元素背景/皮肤的绘制方式。
通过 `decorator` 属性设置，可以组合多种装饰器。

装饰器分为两大类：

### 精灵装饰器（使用 @spritesheet 中的精灵）

| 装饰器                                       | 说明                 |
| ----------------------------------------- | ------------------ |
| `image(sprite)`                           | 单张精灵铺满元素           |
| `tiled-horizontal(l, c, r)`               | 水平三段拉伸：固定左右，拉伸中段   |
| `tiled-vertical(t, c, b)`                 | 垂直三段拉伸：固定上下，拉伸中段   |
| `tiled-box(tl,tc,tr, ml,mc,mr, bl,bc,br)` | 九宫格：四角固定，四边拉伸，中心拉伸 |
| `ninepatch(outer, inner, scale)`          | 自动九宫格：指定外轮廓和内容区即可  |

### 渐变装饰器（纯 CSS 生成）

| 装饰器 | 说明 |
|--------|------|
| `linear-gradient(direction, color1, color2, ...)` | 线性渐变 |
| `radial-gradient(shape, color1, color2, ...)` | 径向渐变 |
| `conic-gradient(...)` | 锥形渐变 |

---

## 4. image() — 单图装饰器

最简单的精灵装饰器，将一个精灵铺满整个元素：

```css
.sprite-btn {
    decorator: image(btn-normal);
    height: 16dp;
}
.sprite-btn:hover {
    decorator: image(btn-hover);
}
```

精灵会被拉伸以填满元素。适用于**固定尺寸**的按钮/图标，
或者**不需要保持精灵纵横比**的场景。

### 状态切换

通过伪类覆盖 `decorator` 属性，可以实现按钮三态：

```css
.btn { decorator: image(btn-n); }     /* 普通 */
.btn:hover { decorator: image(btn-h); }  /* 悬停 */
.btn:active { decorator: image(btn-a); } /* 按下 */
```

---

## 5. tiled-horizontal() — 水平三段拉伸

将精灵分为左端、中段、右端三部分：

```
┌──┬──────────────────┬──┐
│ L│       Center      │ R│
└──┴──────────────────┴──┘
```

左右两端保持原始尺寸，中段水平平铺/拉伸以适应元素宽度。

```css
.tiled-btn {
    decorator: tiled-horizontal(btn-n-l, btn-n-c, btn-n-r);
}
```

**适用场景**：宽度可变的按钮、进度条边框、标签栏。

### @spritesheet 中的切分

对于 64×16 的按钮精灵，左 2px / 中 60px / 右 2px：

```css
@spritesheet ui {
    btn-n-l: 48px 0px  2px 16px;   /* 左端 2px */
    btn-n-c: 50px 0px 60px 16px;   /* 中段 60px */
    btn-n-r:110px 0px  2px 16px;   /* 右端 2px */
}
```

> 类似地，`tiled-vertical(top, center, bottom)` 用于垂直三段拉伸。

---

## 6. tiled-box() — 手动九宫格

九宫格是游戏 UI 最常用的面板皮肤方案，将精灵分为 9 个区域：

```
┌────┬───────────────┬────┐
│ TL │      TC       │ TR │
├────┼───────────────┼────┤
│    │               │    │
│ ML │      MC       │ MR │
│    │               │    │
├────┼───────────────┼────┤
│ BL │      BC       │ BR │
└────┴───────────────┴────┘
```

- **四角（TL/TR/BL/BR）**：保持原始尺寸
- **四边（TC/BC/ML/MR）**：沿一个方向拉伸
- **中心（MC）**：双向拉伸

```css
.tb-panel {
    decorator: tiled-box(
        win-tl, win-tc, win-tr,
        win-ml, win-mc, win-mr,
        win-bl, win-bc, win-br
    );
    padding: 6dp;
}
```

**需要在 @spritesheet 中手动定义 9 个矩形**，这是 tiled-box 的缺点。

---

## 7. ninepatch() — 自动九宫格

`ninepatch` 是 tiled-box 的简化版，只需指定两个矩形：

```css
.np-panel {
    decorator: ninepatch(window-outer, window-inner, 1.0);
}
```

| 参数 | 说明 |
|------|------|
| `outer` | 整个精灵的外轮廓矩形 |
| `inner` | 内容区矩形（九宫格的中心区域） |
| `scale` | 边框缩放因子（1.0 = 原始尺寸） |

RmlUi 会根据 outer 和 inner 的差值自动计算 9 个区域的切分。

### 对比 tiled-box

以 32×32、4px 边框的面板为例：

**ninepatch 只需 2 个精灵定义**：
```css
window-outer: 0px 0px 32px 32px;
window-inner: 4px 4px 24px 24px;
```

**tiled-box 需要 9 个精灵定义**：
```css
win-tl: 0px 0px 4px 4px;
win-tc: 4px 0px 24px 4px;
/* ... 还有 7 个 ... */
```

> 推荐优先使用 ninepatch，除非需要对各区域单独控制纹理映射。

---

## 8. 渐变装饰器

RmlUi 支持 CSS 风格的渐变，无需精灵纹理：

### linear-gradient

```css
.grad-panel {
    decorator: linear-gradient(to bottom, #1a203580, #2d4a7a80);
}
```

方向可以是：`to top` / `to bottom` / `to left` / `to right` 或角度值。

### radial-gradient

```css
.radial-badge {
    decorator: radial-gradient(circle, #7aa2f7, #1a2035);
    border-radius: 24dp;
}
```

`circle` 指定圆形渐变。也可用 `ellipse` 或省略（默认 ellipse）。

### 渐变按钮状态

渐变同样可以配合伪类实现状态切换：

```css
.gradient-btn {
    decorator: linear-gradient(to bottom, #3d6aaa, #2d4a7a);
}
.gradient-btn:hover {
    decorator: linear-gradient(to bottom, #4d8acc, #3d6aaa);
}
.gradient-btn:active {
    decorator: linear-gradient(to bottom, #2d4a7a, #3d6aaa);
}
```

---

## 9. image-color 染色

`image-color` 属性可以给精灵装饰器覆盖一层颜色：

```css
.tinted-btn {
    decorator: image(btn-n);
}
.tint-red {
    image-color: #ff6b6b;
}
.tint-green {
    image-color: #9ece6a;
}
```

**用途**：用同一套精灵生成不同颜色的按钮/图标，
避免为每种颜色制作独立的纹理。

> `image-color` 是乘法混合——白色精灵 × 色彩 = 该色彩，
> 深色精灵 × 色彩 = 更深的色彩。所以适合用在浅色/灰色基底的精灵上。

---

## 10. 装饰器与伪类组合

装饰器最强大的用法之一是配合伪类实现**状态驱动**的外观切换：

| 伪类 | 触发条件 |
|------|----------|
| `:hover` | 鼠标悬停 |
| `:active` | 鼠标按下 |
| `:focus` | 获得焦点 |
| `:checked` | 复选框/单选框选中 |
| `:disabled` | 元素被禁用 |

```css
.btn {
    decorator: image(btn-n);
    color: #c0caf5;
}
.btn:hover {
    decorator: image(btn-h);
    color: #ffffff;
}
.btn:active {
    decorator: image(btn-a);
    color: #1a1b26;
}
```

每个伪类可以完全替换 `decorator`，也可以只改变 `image-color` 或其他属性。

---

## 11. 演示场景解析

本课的 `SpritesheetScene` 非常简洁——纯展示，无需数据绑定：

```cpp
bool SpritesheetScene::init() {
    if (!Scene::init()) return false;
    doc_ = loadRmlDocument("ui/rmlui/learn/learn_spritesheet.rml");
    return doc_ != nullptr;
}

void SpritesheetScene::clean() {
    unloadAllRmlDocuments();
    doc_ = nullptr;
    Scene::clean();
}
```

所有视觉效果完全由 RCSS 装饰器驱动，C++ 侧只需加载文档。

### 页面布局

```
┌─────────────────────────────────────────┐
│  L09 Spritesheet & Decorators           │
├────────────────────┬────────────────────┤
│  Nine-patch Panels │  Buttons &         │
│  ┌──────────────┐  │  Decorators        │
│  │ ninepatch()   │  │  [image() btn]    │
│  └──────────────┘  │  [tiled-h btn]    │
│  ┌──────────────┐  │  [red] [grn] [gld]│
│  │ tiled-box()   │  │  [gradient btn]  │
│  └──────────────┘  │  (●) radial       │
│  ┌──────────────┐  │                    │
│  │ gradient      │  │                    │
│  └──────────────┘  │                    │
├────────────────────┴────────────────────┤
│  @spritesheet | ninepatch() | ...       │
└─────────────────────────────────────────┘
```

左列展示三种面板装饰器，右列展示按钮和渐变装饰器。

---

## 12. 精灵表制作要点

### 边距与对齐

精灵之间不需要额外间距（RmlUi 按像素坐标精确裁切），
但建议将相关精灵放在相邻区域，方便维护。

### 九宫格切分规则

对于 W×H 的面板精灵，边框宽度为 B：

```
outer: 0px 0px  W    H
inner: B   B   (W-2B) (H-2B)
```

本课的 32×32 面板、4px 边框：
```css
window-outer: 0px 0px 32px 32px;
window-inner: 4px 4px 24px 24px;
```

### resolution 与高 DPI

```css
@spritesheet ui-2x {
    src: spritesheet@2x.png;
    resolution: 2x;
    /* 坐标仍以实际像素为单位 */
    btn: 0px 0px 128px 32px;  /* 实际 128px，显示为 64dp */
}
```

`resolution: 2x` 表示该纹理是 2 倍分辨率，RmlUi 会自动缩放到逻辑像素。

---

## 13. 练习

### 练习 1：扩展窗口皮肤

在 spritesheet 中添加第二套窗口皮肤（例如红色边框的警告面板），
定义新的 `@spritesheet` 精灵和 `.warn-panel` 类。

### 练习 2：按钮图标

在按钮精灵旁添加小图标精灵（如剑、盾牌），
用 `image()` 装饰器为按钮添加前缀图标。

### 练习 3：渐变动效

结合 L10 将要学习的 `transition` 属性，让渐变按钮在
hover 时平滑过渡（提示：RmlUi 的 decorator 不支持过渡，
但 `image-color` 和 `background-color` 可以）。

---

## 14. 要点回顾

| 概念 | 要点 |
|------|------|
| `@spritesheet` | 在 RCSS 中声明精灵图集，`src` + `resolution` + 精灵矩形 |
| `image()` | 单张精灵铺满元素，最简单的装饰器 |
| `tiled-horizontal()` | 水平三段拉伸，适合可变宽度按钮 |
| `tiled-vertical()` | 垂直三段拉伸，适合可变高度条 |
| `tiled-box()` | 手动九宫格，需定义 9 个精灵矩形 |
| `ninepatch()` | 自动九宫格，只需 outer + inner 两个矩形 |
| `linear-gradient()` | 线性渐变，无需纹理 |
| `radial-gradient()` | 径向渐变，适合徽章/圆形元素 |
| `image-color` | 精灵染色，乘法混合，一套精灵多种颜色 |
| 装饰器 + 伪类 | 不同状态下切换装饰器实现按钮三态等效果 |
| `resolution` | 高 DPI 适配，`2x` 纹理自动缩放 |

---

**上一课 <-** [L08: 自定义元素与文档管理](L08-custom-elements.md)
**下一课 ->** L10: 动画、变换与过渡（待完成）
