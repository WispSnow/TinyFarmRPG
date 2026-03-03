# L04: 文字排版与视觉样式

> 配套代码：`learn/rmlui_styling/` | 构建目标：`learn_rmlui_styling`

---

## 1. 字体属性

### font-family

指定字体名称（需引擎已加载该字体）：

```css
body {
    font-family: "VonwaonBitmap 16px";
}
```

### font-size

支持 `dp`（密度无关像素）、`%`、`em`：

```css
.title  { font-size: 24dp; }
.small  { font-size: 0.8em; }  /* 相对父元素 */
```

### font-weight

```css
.normal { font-weight: normal; }  /* 等同 400 */
.bold   { font-weight: bold; }    /* 等同 700 */
.w300   { font-weight: 300; }     /* 数值 0~1000 */
```

### font-style

```css
.italic { font-style: italic; }
```

> 注意：位图字体（如 VonwaonBitmap）通常没有 italic 变体，使用 `font-style: italic` 会报
> "No font face defined" 警告。只有提供了 italic 字形文件的字体才能使用此属性。

### font 简写

```css
.text { font: bold 16dp "VonwaonBitmap 16px"; }
/*       weight size  family */
```

### letter-spacing

字符间距：

```css
.spaced { letter-spacing: 2dp; }
```

---

## 2. 文字控制

### text-align

```css
.left    { text-align: left; }      /* 默认 */
.center  { text-align: center; }
.right   { text-align: right; }
.justify { text-align: justify; }
```

### text-decoration

```css
.underline    { text-decoration: underline; }
.overline     { text-decoration: overline; }
.line-through { text-decoration: line-through; }
.none         { text-decoration: none; }
```

> RmlUi 只支持单个值，不能组合（如 `underline overline` 无效）。

### text-transform

```css
.upper { text-transform: uppercase; }
.lower { text-transform: lowercase; }
.cap   { text-transform: capitalize; }
```

### line-height

```css
.loose  { line-height: 1.8; }   /* 相对倍数 */
.tight  { line-height: 1.0; }
.fixed  { line-height: 20dp; }  /* 绝对值 */
```

---

## 3. 文字溢出

### white-space

控制空白字符和换行行为：

| 值 | 空白折叠 | 自动换行 |
|------|----------|----------|
| `normal` | 是 | 是 |
| `nowrap` | 是 | 否 |
| `pre` | 否 | 否 |
| `pre-wrap` | 否 | 是 |
| `pre-line` | 是 | 是（保留显式换行） |

### word-break

```css
.break-all  { word-break: break-all; }   /* 任意位置断行 */
.break-word { word-break: break-word; }   /* 单词边界优先，长词强制断 */
```

### text-overflow

当文本溢出容器时的显示方式（需配合 `white-space: nowrap` + `overflow-x: hidden`）：

```css
.truncate {
    white-space: nowrap;
    overflow-x: hidden;
    text-overflow: ellipsis;   /* 显示 ... */
}
```

> RmlUi 完整支持 `text-overflow: ellipsis`，也支持自定义省略字符串。

---

## 4. 颜色值格式

RmlUi 支持多种颜色格式：

| 格式 | 示例 | 说明 |
|------|------|------|
| 命名色 | `red`, `white`, `transparent` | HTML 标准色名 |
| `#RGB` | `#f00` | 3 位十六进制 |
| `#RRGGBB` | `#ff0000` | 6 位十六进制 |
| `#RGBA` | `#f00f` | 4 位十六进制（带 alpha） |
| `#RRGGBBAA` | `#ff000080` | 8 位十六进制（带 alpha） |
| `rgb()` | `rgb(255, 0, 0)` | RGB 函数 |
| `rgba()` | `rgba(255, 0, 0, 0.5)` | RGBA 函数 |
| `hsl()` | `hsl(0deg, 100%, 50%)` | HSL 函数 |
| `hsla()` | `hsla(0, 100%, 50%, 0.5)` | HSLA 函数 |

```css
.hex6    { color: #7aa2f7; }
.hex8    { color: #7aa2f780; }           /* 50% 透明 */
.rgb     { color: rgb(122, 162, 247); }
.rgba    { color: rgba(122, 162, 247, 0.5); }
.hsl     { color: hsl(225deg, 90%, 72%); }
.named   { color: white; }
```

---

## 5. 边框与圆角

### border

```css
/* 简写：<width> <color>（RmlUi 不支持 solid 等 style 关键字！） */
.box { border: 2dp #7aa2f7; }

/* 单边设置 */
.box {
    border-top: 2dp #7aa2f7;
    border-bottom: 1dp #414868;
}

/* 分开设置宽度和颜色 */
.box {
    border-width: 2dp;
    border-color: #7aa2f7;
}
```

### border-radius

```css
.rounded   { border-radius: 6dp; }       /* 四角相同 */
.pill      { border-radius: 100dp; }     /* 药丸形 */
.custom    { border-radius: 10dp 0 10dp 0; } /* 左上 右上 右下 左下 */
```

---

## 6. 背景与透明度

### background-color

```css
.panel { background-color: #1a1b26; }
.glass { background-color: #1a1b26d0; }  /* 带 alpha */
.tint  { background-color: rgba(0, 0, 0, 0.6); }
```

> RmlUi **没有** `background-image` 属性。背景图片通过 `decorator` 实现（见下节）。

### opacity

影响元素及其所有子元素的透明度：

```css
.dim     { opacity: 0.5; }
.ghost   { opacity: 0.3; }
.visible { opacity: 1; }    /* 默认 */
```

---

## 7. 装饰器入门

RmlUi 使用 `decorator` 属性代替 CSS 的 `background-image` / `background`。

### image 装饰器

```css
/* 引用图片文件 */
.bg { decorator: image("/textures/panel_bg.png"); }

/* 引用 spritesheet 中的精灵（L09 详解） */
.icon { decorator: image(icon-sword); }

/* 适配模式 */
.cover   { decorator: image("/textures/bg.png" cover); }
.contain { decorator: image("/textures/bg.png" contain); }
.fill    { decorator: image("/textures/bg.png" fill); }
```

### linear-gradient 装饰器

```css
/* 从上到下 */
.grad1 { decorator: linear-gradient(180deg, #ff6600, #cc3300); }

/* 从左到右 */
.grad2 { decorator: linear-gradient(90deg, #7aa2f7, #bb9af7); }

/* 带方向关键字 */
.grad3 { decorator: linear-gradient(to right, red, blue); }

/* 多色带色标 */
.grad4 { decorator: linear-gradient(180deg, #ff0000 0%, #ffff00 50%, #00ff00 100%); }

/* 重复渐变 */
.stripe { decorator: repeating-linear-gradient(45deg, #333 0%, #333 10%, #555 10%, #555 20%); }
```

### radial-gradient 装饰器

```css
/* 基础径向 */
.radial { decorator: radial-gradient(circle, white, transparent); }

/* 带位置和大小 */
.spot { decorator: radial-gradient(circle 40dp at 50% 30%, #ff660080, transparent); }
```

### conic-gradient 装饰器

```css
/* 色轮 */
.wheel { decorator: conic-gradient(red, yellow, lime, aqua, blue, magenta, red); }
```

### 多层装饰器叠加

装饰器可以逗号分隔叠加，按声明顺序绘制：

```css
.layered {
    decorator:
        radial-gradient(circle 60dp at 80% 20%, #ff660040, transparent),
        linear-gradient(180deg, #1a1b26, #24283b);
}
```

### 绘制区域

通过关键字指定装饰器绘制范围：

```css
.pad-only { decorator: linear-gradient(#333, #555) padding-box; }
.border-area { decorator: image(bg) border-box; }
```

---

## 8. 文字特效（font-effect）

RmlUi 提供独有的 `font-effect` 属性，在字形纹理层面渲染特效，不同于 CSS 的 `text-shadow`。

### shadow — 文字阴影

```css
.shadow { font-effect: shadow(2dp 2dp #000); }
/*                      shadow(偏移x 偏移y 颜色) */
```

### glow — 外发光

glow 是描边（outline）+ 高斯模糊的组合，参数从简到全：

```css
/* 最简：描边宽度 + 颜色 */
.glow1 { font-effect: glow(2dp #ed5); }

/* 描边 + 模糊 */
.glow2 { font-effect: glow(3dp 2dp #ed5); }

/* 完整：描边 模糊 偏移x 偏移y 颜色 */
.glow3 { font-effect: glow(2dp 4dp 1dp 1dp #644); }
```

### outline — 文字描边

```css
.outline { font-effect: outline(2dp #f00); }
/*                       outline(宽度 颜色) */
```

### blur — 文字模糊

```css
.blur { font-effect: blur(3dp #fff); }
/*                    blur(宽度 颜色) */
```

### 叠加多个特效

用逗号分隔即可叠加多个 font-effect：

```css
.fancy { font-effect: glow(3dp #ff6a), outline(2dp #0003); }
```

### 与 box-shadow / filter 的区别

| 属性 | 作用对象 | 原理 |
|------|----------|------|
| `font-effect` | 文字字形 | 在字形纹理生成阶段处理，每个字符独立渲染 |
| `box-shadow` | 整个元素的盒子 | 基于元素矩形区域绘制阴影 |
| `filter: drop-shadow()` | 整个元素 | 对元素渲染结果做后处理 |

> `font-effect` 效果最精准（逐字符），适合 JRPG 中的标题文字、伤害数字、状态提示等。

---

## 9. 伪类样式

通过伪类实现交互状态的视觉反馈：

```css
.button { background-color: #414868; }
.button:hover { background-color: #565f89; }
.button:active { background-color: #7aa2f7; }
.button:focus { border: 2dp #bb9af7; }
```

常用伪类：

| 伪类 | 触发时机 |
|------|----------|
| `:hover` | 鼠标悬停 |
| `:active` | 鼠标按下 |
| `:focus` | 获得焦点 |
| `:checked` | 勾选状态（表单控件） |
| `:disabled` | 禁用状态 |

---

## 10. 配套代码

### 场景代码

`learn/rmlui_styling/styling_scene.cpp` — 加载 `ui/rmlui/learn/learn_styling.rml`。

### RML 文档

文档包含 7 个 demo 区块：

1. **Font** — 字体大小、粗细、字间距
2. **Text** — 对齐、装饰、大小写变换
3. **Overflow** — text-overflow: ellipsis 截断演示
4. **Colors & Opacity** — 多种颜色格式 + 透明度
5. **Decorators** — 线性渐变 / 径向渐变 / 锥形渐变
6. **Font Effect** — shadow / glow / outline / blur 文字特效
7. **Character Card + Hover** — JRPG 角色卡 + 伪类按钮

---

## 11. 构建与运行

```bash
ninja -C build/debug learn_rmlui_styling
./build/debug/learn/learn_rmlui_styling
```

---

## 12. 练习

### 练习：JRPG 角色简介卡

设计一个 JRPG 风格的角色信息卡片：

**布局要求**：
- 圆角边框（`border-radius: 8dp`）
- 半透明深色背景（`background-color: #1a1b26d0`）
- 固定宽度 240dp

**内容要求**：
- 角色名：粗体、亮色（`#e0af68`）、居中、18dp、带 `font-effect: glow()`
- 职业标签：小号（11dp）、灰色（`#565f89`）、居中
- 分隔线 `<hr>`
- 属性列表（Flex 两列）：
  - HP / MP / ATK / DEF，左侧标签右对齐，右侧数值左对齐
  - HP 用绿色（`#9ece6a`），MP 用蓝色（`#7aa2f7`）
- 描述文字：11dp、灰色、两行带 `line-height: 1.6`
- 卡片底部渐变装饰器（`linear-gradient`）

---

## 13. 要点回顾

| 概念 | 要点 |
|------|------|
| 字体 | `font-family` / `font-size` / `font-weight` / `font-style` / `letter-spacing` |
| 文字控制 | `text-align` / `text-decoration` / `text-transform` / `line-height` |
| 文字溢出 | `white-space: nowrap` + `overflow-x: hidden` + `text-overflow: ellipsis` |
| 颜色 | `#RRGGBB` / `#RRGGBBAA` / `rgb()` / `rgba()` / `hsl()` / 命名色 |
| 边框 | `border: <width> <color>`（无 style 关键字）/ `border-radius` |
| 背景 | `background-color`（无 `background-image`，用 `decorator` 代替） |
| 透明度 | `opacity: 0~1`，影响所有子元素 |
| 装饰器 | `decorator: image(...)` / `linear-gradient(...)` / `radial-gradient(...)` / `conic-gradient(...)` |
| 文字特效 | `font-effect: shadow(...)` / `glow(...)` / `outline(...)` / `blur(...)`，可逗号叠加 |
| 伪类 | `:hover` / `:active` / `:focus` / `:checked` / `:disabled` |

---

**上一课 <-** [L03: Flexbox 布局](L03-flexbox.md)
**下一课 ->** [L05: 事件系统](L05-events.md)
