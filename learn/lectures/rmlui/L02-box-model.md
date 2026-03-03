# L02: 盒模型与定位

> 配套代码：`learn/rmlui_box_model/` | 构建目标：`learn_rmlui_box_model`

---

## 1. RCSS 盒模型

RmlUi 中每个元素都是一个矩形"盒子"，从内到外由四层区域组成：

```
┌───────────────────────────────── margin ─────────────────────────────────┐
│                                                                          │
│  ┌───────────────────────────── border ─────────────────────────────┐    │
│  │                                                                   │    │
│  │  ┌─────────────────────── padding ──────────────────────────┐    │    │
│  │  │                                                           │    │    │
│  │  │  ┌─────────────────── content ──────────────────────┐    │    │    │
│  │  │  │                                                   │    │    │    │
│  │  │  │            实际内容区域                              │    │    │    │
│  │  │  │                                                   │    │    │    │
│  │  │  └───────────────────────────────────────────────────┘    │    │    │
│  │  │                                                           │    │    │
│  │  └───────────────────────────────────────────────────────────┘    │    │
│  │                                                                   │    │
│  └───────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

| 区域 | 作用 | 默认值 |
|------|------|--------|
| **content** | 放置文字、子元素的实际区域 | 由内容撑开或 `width`/`height` 指定 |
| **padding** | content 与 border 之间的内间距 | `0` |
| **border** | 可见边框 | 无 |
| **margin** | 元素与外部元素之间的外间距 | `0`（部分元素如 `<p>` 有默认 margin） |

---

## 2. 尺寸与 box-sizing

### 宽高属性

```css
.box {
    width: 200dp;           /* 固定宽度 */
    height: 100dp;          /* 固定高度 */
    min-width: 80dp;        /* 最小宽度 */
    max-width: 400dp;       /* 最大宽度 */
    min-height: 40dp;       /* 最小高度 */
    max-height: 300dp;      /* 最大高度 */
}
```

不设置 `width` / `height` 时，block 元素宽度默认填满父容器，高度由内容撑开。

### box-sizing

`box-sizing` 决定 `width` / `height` 指定的是哪个区域的尺寸：

```css
/* 默认值：width/height 只算 content 区域 */
.box-content { box-sizing: content-box; }

/* border-box：width/height 包含 padding + border */
.box-border { box-sizing: border-box; }
```

示意：

```
width = 200dp, padding = 20dp, border = 2dp

content-box → 总宽度 = 200 + 20×2 + 2×2 = 244dp
border-box  → 总宽度 = 200dp（content 自动缩减为 156dp）
```

**建议**：对于需要精确对齐的面板，使用 `border-box` 更直观。

---

## 3. Margin 与 Padding

### 语法

```css
/* 四个方向分别设置 */
margin-top: 8dp;
margin-right: 12dp;
margin-bottom: 8dp;
margin-left: 12dp;

/* 简写（顺序：上 右 下 左） */
margin: 8dp 12dp 8dp 12dp;

/* 两值简写（上下, 左右） */
margin: 8dp 12dp;

/* 单值（四个方向相同） */
margin: 8dp;

/* padding 语法完全相同 */
padding: 16dp;
```

### Margin 合并

与 CSS 一样，RmlUi 中**垂直方向相邻的 margin 会合并**（取较大值）：

```xml
<p style="margin-bottom: 20dp;">段落 A</p>
<p style="margin-top: 10dp;">段落 B</p>
<!-- 实际间距 = 20dp（而非 30dp） -->
```

水平 margin 不合并。

### 负 Margin

RmlUi 支持负 margin，可用于元素重叠效果：

```css
.overlap { margin-top: -8dp; }  /* 向上偏移 8dp，覆盖到上方元素 */
```

---

## 4. Border

```css
/* 完整写法 */
border-width: 2dp;
border-color: #7aa2f7;

/* 简写 */
border: 2dp #7aa2f7;

/* 单边设置 */
border-top: 1dp #565f89;
border-bottom: 2dp #9ece6a;

/* 圆角 */
border-radius: 8dp;                   /* 四角相同 */
border-top-left-radius: 12dp;         /* 单独设置某角 */
border-top-right-radius: 0dp;
```

> 注意：RmlUi 的 `border` 简写**不支持** `solid` 等边框样式关键字（与 CSS 不同）。语法为 `border: <width> <color>;`。

---

## 5. 尺寸单位

| 单位 | 含义 | 示例 |
|------|------|------|
| `dp` | 密度无关像素，根据 DPI 缩放 | `width: 200dp` |
| `px` | 物理像素（1:1 映射到屏幕像素） | `border: 1px #fff` |
| `%` | 相对于父元素对应维度 | `width: 50%`（父元素宽度的一半） |
| `em` | 相对于当前元素的字号 | `padding: 0.5em`（半个字符宽） |

**推荐**：布局和间距用 `dp`，它在高 DPI 屏幕上能自动缩放。`%` 用于响应式尺寸。

---

## 6. Display 模式

### 关键差异：默认 display 是 inline

> **RmlUi 与浏览器最大的区别之一**：所有元素的默认 `display` 是 `inline`，而不是 `block`。
> 这意味着 `<div>`、`<p>`、`<h1>` 等元素**默认会横向排列**，和浏览器行为完全不同。

因此，每个 RCSS 文件的开头都应该加一条重置规则：

```css
/* 重置块级元素 */
body, div, h1, h2, h3, h4, p, hr {
    display: block;
}
```

### display 属性值

| 值 | 行为 |
|------|------|
| `block` | 独占一行，宽度默认填满父容器 |
| `inline` | 行内排列，宽高由内容决定（不可设置 width/height） |
| `inline-block` | 行内排列，但可以设置 width/height |
| `none` | 从布局中完全移除，不占空间 |
| `flex` | 弹性布局容器（L03 详解） |

```css
.tag   { display: inline-block; padding: 2dp 8dp; }   /* 行内块：可排一行 + 可设尺寸 */
.hidden { display: none; }                              /* 隐藏 */
```

---

## 7. 定位模式

### position 属性

| 值 | 行为 | 参考系 |
|------|------|--------|
| `static` | 默认值，正常文档流 | — |
| `relative` | 正常流中占位，但可用 top/left 偏移 | 相对于自身原始位置 |
| `absolute` | 脱离文档流，相对于最近的**非 static 祖先**定位 | 祖先的 padding 区域 |
| `fixed` | 脱离文档流，相对于窗口定位 | 视口 |

```css
/* 相对定位：微调位置，不影响其他元素 */
.nudge {
    position: relative;
    top: 4dp;      /* 向下偏移 4dp */
    left: -2dp;    /* 向左偏移 2dp */
}

/* 绝对定位：脱离文档流，自由放置 */
.tooltip {
    position: absolute;
    right: 0dp;    /* 贴着父容器右边 */
    top: -32dp;    /* 在父容器上方 */
}

/* 固定定位：始终在屏幕某个位置 */
.hud-clock {
    position: fixed;
    right: 16dp;
    top: 16dp;
}
```

### z-index

控制堆叠顺序（仅对定位元素生效），数值越大越靠前：

```css
.popup   { z-index: 100; }
.overlay { z-index: 200; }  /* 覆盖在 popup 之上 */
```

---

## 8. Overflow 与滚动

当子元素超出父容器尺寸时，`overflow` 决定如何处理：

```css
.container {
    width: 200dp;
    height: 150dp;
    overflow: auto;     /* 内容超出时显示滚动条 */
}
```

| 值 | 行为 |
|------|------|
| `visible` | 默认，超出内容照常显示 |
| `hidden` | 裁切超出内容 |
| `auto` | 超出时显示滚动条 |
| `scroll` | 始终显示滚动条 |

也可以分轴设置：

```css
.panel {
    overflow-x: hidden;   /* 水平不滚动 */
    overflow-y: auto;     /* 垂直自动滚动 */
}
```

---

## 9. 配套代码

### 新增 learn target

在 `learn/CMakeLists.txt` 中已添加：

```cmake
add_learn_target(rmlui_box_model)
```

### 场景代码

`learn/rmlui_box_model/main.cpp` — 与 L01 相同的启动模式，只是场景类不同。

`learn/rmlui_box_model/box_model_scene.h` / `.cpp` — 加载 `ui/rmlui/learn/learn_box_model.rml`。

### RML 文档：`ui/rmlui/learn/learn_box_model.rml`

```xml
<rml>
<head>
    <link type="text/rcss" href="learn_box_model.rcss"/>
</head>
<body>
    <!-- ① 盒模型可视化 -->
    <div id="box-demo" class="section">
        <h2>Box Model</h2>
        <div class="box-outer">
            <div class="box-inner">
                content area
            </div>
        </div>
        <p class="note">外层 = margin(粉) + border(蓝) + padding(绿)，内层 = content(暗)</p>
    </div>

    <!-- ② box-sizing 对比 -->
    <div id="sizing-demo" class="section">
        <h2>box-sizing</h2>
        <div class="sizing-row">
            <div class="box-content-box">content-box<br/>总宽更大</div>
            <div class="box-border-box">border-box<br/>总宽 = width</div>
        </div>
    </div>

    <!-- ③ 定位演示 -->
    <div id="position-demo" class="section">
        <h2>Position</h2>
        <div class="pos-container">
            <div class="pos-static">static (默认)</div>
            <div class="pos-relative">relative (偏移)</div>
            <div class="pos-absolute">absolute</div>
        </div>
    </div>

    <!-- ④ overflow 演示 -->
    <div id="overflow-demo" class="section">
        <h2>Overflow</h2>
        <div class="scroll-box">
            <p>第 1 行 — 滚动测试</p>
            <p>第 2 行</p>
            <p>第 3 行</p>
            <p>第 4 行</p>
            <p>第 5 行</p>
            <p>第 6 行</p>
            <p>第 7 行 — 底部</p>
        </div>
    </div>

    <!-- ⑤ 固定定位 HUD 示例 -->
    <div id="hud-badge">FIXED 定位</div>
</body>
</rml>
```

### RCSS 样式：`ui/rmlui/learn/learn_box_model.rcss`

```css
body {
    margin: 0;
    padding: 16dp;
    font-size: 14dp;
    font-family: "VonwaonBitmap 16px";
    color: #c0caf5;
}

/* === 通用区块 === */
.section {
    margin-bottom: 20dp;
    padding: 12dp;
    background-color: #1a1b26d0;
    border: 1dp #414868;
    border-radius: 6dp;
}

h2 {
    margin: 0 0 10dp 0;
    font-size: 16dp;
    color: #7aa2f7;
}

.note {
    margin: 8dp 0 0 0;
    font-size: 12dp;
    color: #565f89;
}

/* === ① 盒模型可视化 === */
.box-outer {
    /* 模拟 margin 区域（用 padding 代替可视化） */
    padding: 16dp;                          /* ← 这是 "padding" 层 */
    background-color: #9ece6a40;            /* 绿色半透明 = padding 可视 */
    border: 3dp #7aa2f7;              /* 蓝色 = border */
    border-radius: 4dp;
    margin: 12dp;                           /* 粉色不可见，但占空间 */
}

.box-inner {
    padding: 12dp;
    background-color: #24283b;              /* 深色 = content */
    text-align: center;
    border-radius: 2dp;
}

/* === ② box-sizing 对比 === */
.sizing-row {
    display: flex;
    flex-direction: row;
    gap: 16dp;
}

.box-content-box {
    box-sizing: content-box;
    width: 160dp;
    padding: 16dp;
    border: 2dp #f7768e;
    background-color: #f7768e20;
    border-radius: 4dp;
    text-align: center;
}

.box-border-box {
    box-sizing: border-box;
    width: 160dp;                           /* 总宽就是 160dp */
    padding: 16dp;
    border: 2dp #9ece6a;
    background-color: #9ece6a20;
    border-radius: 4dp;
    text-align: center;
}

/* === ③ 定位演示 === */
.pos-container {
    position: relative;                     /* 作为 absolute 子元素的参考系 */
    height: 120dp;
    background-color: #24283b;
    border: 1dp #414868;
    border-radius: 4dp;
}

.pos-static {
    padding: 6dp 10dp;
    background-color: #565f8940;
    border: 1dp #565f89;
}

.pos-relative {
    position: relative;
    top: 4dp;
    left: 20dp;
    padding: 6dp 10dp;
    background-color: #e0af6840;
    border: 1dp #e0af68;
}

.pos-absolute {
    position: absolute;
    right: 8dp;
    top: 8dp;
    padding: 6dp 10dp;
    background-color: #bb9af740;
    border: 1dp #bb9af7;
    border-radius: 4dp;
}

/* === ④ overflow 演示 === */
.scroll-box {
    height: 80dp;
    overflow-y: auto;
    padding: 8dp;
    background-color: #24283b;
    border: 1dp #414868;
    border-radius: 4dp;
}

.scroll-box p {
    margin: 4dp 0;
}

/* === ⑤ 固定定位 HUD === */
#hud-badge {
    position: fixed;
    right: 16dp;
    top: 16dp;
    padding: 6dp 14dp;
    background-color: #f7768ec0;
    border-radius: 4dp;
    font-size: 12dp;
    color: #ffffff;
}
```

---

## 10. 构建与运行

```bash
cmake -B build -G Ninja -DBUILD_LEARN=ON
ninja -C build learn_rmlui_box_model
./build/learn/learn_rmlui_box_model
```

运行后可以看到：
- **Box Model** — 绿色 padding 层 + 蓝色 border 包裹暗色 content
- **box-sizing** — 两个等宽声明的盒子，content-box 的实际总宽更大
- **Position** — static 正常流、relative 偏移、absolute 右上角
- **Overflow** — 7 行文字超出容器，出现垂直滚动条
- **FIXED** — 右上角红色标签，始终在视口固定位置

---

## 11. 练习

### 练习 1：盒模型计算

给定以下样式，计算元素在屏幕上的**实际占用总宽度**：

```css
.quiz {
    box-sizing: content-box;
    width: 120dp;
    padding: 10dp;
    border: 2dp #fff;
    margin: 8dp;
}
```

<details>
<summary>答案</summary>

总宽度 = margin-left + border-left + padding-left + width + padding-right + border-right + margin-right
= 8 + 2 + 10 + 120 + 10 + 2 + 8 = **160dp**

如果改为 `border-box`，则 width 包含 padding+border：
content = 120 - 10×2 - 2×2 = 96dp，总宽度 = 8 + 120 + 8 = **136dp**
</details>

### 练习 2：角色信息卡

创建 `ui/rmlui/learn/learn_char_card.rml` + `.rcss`，实现如下布局：

```
┌──────────────────────────────┐
│  [头像区域]   角色名          │
│   64×64       职业: 战士      │
│               等级: 12        │
├──────────────────────────────┤
│  一段角色描述文字，可能比较    │
│  长需要多行显示……             │
└──────────────────────────────┘
```

要求：
1. 整体面板使用 `position: absolute` 居于画面左上角
2. 头像区域用一个固定尺寸的 `<div>`（背景色占位即可）
3. 名称和属性区域在头像右侧（提示：用 Flex 或 inline-block）
4. 分隔线使用 `<hr/>`，下方描述文字可滚动（`overflow-y: auto`，限高）

### 练习 3：层叠定位

在练习 2 的基础上：
1. 给角色卡右上角添加一个 `position: absolute` 的等级徽章（小圆形）
2. 添加一个 `position: fixed` 的底部状态栏，宽度 100%，显示 "HP: 120 / 150"
3. 调整 `z-index` 确保状态栏永远在最上层

---

## 12. 要点回顾

| 概念 | 要点 |
|------|------|
| 盒模型层次 | content → padding → border → margin |
| box-sizing | `content-box`（默认）vs `border-box`（推荐精确布局） |
| 推荐单位 | 布局用 `dp`，响应式用 `%` |
| display | `block` / `inline` / `inline-block` / `none` / `flex` |
| position | `static`（默认）/ `relative` / `absolute` / `fixed` |
| overflow | `visible` / `hidden` / `auto` / `scroll` |
| z-index | 仅对定位元素生效，数值越大越靠前 |

---

**上一课 ←** [L01: 文档结构与第一个界面](L01-document-structure.md)
**下一课 →** [L03: Flexbox 布局](L03-flexbox.md)
