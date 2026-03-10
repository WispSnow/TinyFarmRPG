# L11: 滤镜、阴影与视觉特效

> 配套代码：`learn/rmlui_filters/` | 构建目标：`learn_rmlui_filters`

---

## 1. 为什么需要滤镜与阴影？

上一课我们学习了 transition 和 @keyframes 实现动态效果。本课进入视觉增强的最后一环——**后处理级别的画面效果**：

- **模糊/变色**：中毒时角色头像变绿，死亡时灰度化
- **毛玻璃**：半透明面板叠加在彩色背景上，营造深度感
- **阴影与发光**：面板浮起感、选中高亮外发光
- **遮罩渐变**：文本渐隐、圆形裁切

RmlUi 6.2 的 GL3 渲染后端完整支持这些特性。

---

## 2. filter — 滤镜属性

`filter` 对元素及其子元素的渲染结果施加图像处理效果。

### 语法

```css
filter: <filter-function> [<filter-function>]*;
```

多个滤镜函数用**空格**分隔，按顺序依次应用。

### 支持的滤镜函数（共 10 种）

| 函数 | 参数 | 说明 | 默认值 |
|------|------|------|--------|
| `blur(radius)` | 长度值 | 高斯模糊 | 0dp |
| `brightness(n)` | 数值/百分比 | 亮度（0=黑,1=原始,>1=更亮） | 1 |
| `contrast(n)` | 数值/百分比 | 对比度（0=灰,1=原始） | 1 |
| `grayscale(n)` | 数值/百分比 | 灰度化（0=彩色,1=全灰） | 0 |
| `saturate(n)` | 数值/百分比 | 饱和度（0=去饱和,1=原始,>1=过饱和） | 1 |
| `sepia(n)` | 数值/百分比 | 棕褐色调（0=无,1=全棕） | 0 |
| `hue-rotate(angle)` | 角度值 | 色相旋转 | 0deg |
| `invert(n)` | 数值/百分比 | 反色（0=无,1=完全反色） | 0 |
| `opacity(n)` | 数值/百分比 | 透明度（0=透明,1=不透明） | 1 |
| `drop-shadow(...)` | 见下文 | 阴影投射 | — |

### drop-shadow 语法

```css
filter: drop-shadow(<color> <offset-x> <offset-y> <blur-radius>);
```

示例：

```css
filter: drop-shadow(#00000099 3dp 3dp 2dp);
```

### 多滤镜链

```css
filter: brightness(1.2) hue-rotate(90deg) drop-shadow(#000 2dp 2dp 3dp);
```

---

## 3. filter 动画

**`filter` 属性可以参与 transition 和 @keyframes 动画**，这是实现许多游戏效果的关键。

### hover 过渡

```css
.swatch-blur {
    filter: blur(2dp);
    transition: filter 0.5s cubic-out;
}
.swatch-blur:hover {
    filter: blur(0dp);
}
```

### @keyframes 动画

```css
@keyframes poison-pulse {
    from { filter: hue-rotate(80deg) saturate(2.0); }
    50%  { filter: hue-rotate(130deg) saturate(3.0); }
    to   { filter: hue-rotate(80deg) saturate(2.0); }
}
.avatar.poisoned {
    animation: 2s cubic-in-out infinite poison-pulse;
}
```

角色头像在中毒时，色相和饱和度持续脉动，视觉反馈清晰。

### 色相环绕

```css
@keyframes hue-cycle {
    from { filter: hue-rotate(0deg); }
    to   { filter: hue-rotate(360deg); }
}
.rainbow-box {
    animation: 3s linear infinite hue-cycle;
}
```

---

## 4. backdrop-filter — 背景滤镜

`backdrop-filter` 对元素**下方**（已渲染的内容）施加滤镜，是实现毛玻璃效果的核心。

### 语法

```css
backdrop-filter: <filter-function> [<filter-function>]*;
```

支持的滤镜函数与 `filter` 完全相同。

### 毛玻璃效果

```css
.glass-panel {
    background-color: #1a1b2680;  /* 半透明背景 */
    backdrop-filter: blur(5dp);    /* 模糊下层内容 */
    border: 1dp #c0caf540;
    border-radius: 4dp;
}
```

效果：面板下方的彩色方块被模糊，面板本身保持半透明，产生磨砂玻璃质感。

### 关键点

- 元素必须有**半透明背景**（alpha < 1.0），否则看不到被模糊的下层
- 可与 `filter` 同时使用（`filter` 作用于元素本身，`backdrop-filter` 作用于下层）
- `backdrop-filter` 也支持动画插值

---

## 5. box-shadow — 盒阴影

`box-shadow` 为元素添加投影效果。

### 语法

```css
box-shadow: [inset]? <color> <offset-x> <offset-y> [<blur-radius>]? [<spread>]?;
```

### 参数

| 参数 | 说明 |
|------|------|
| `inset` | 可选，生成内阴影 |
| `color` | 阴影颜色（支持 alpha） |
| `offset-x` | 水平偏移 |
| `offset-y` | 垂直偏移 |
| `blur-radius` | 模糊半径（可选，默认 0） |
| `spread` | 扩展距离（可选，默认 0） |

### 示例

```css
/* 外阴影 */
box-shadow: #00000080 3dp 3dp 5dp 0dp;

/* 内阴影 */
box-shadow: #00000080 2dp 2dp 4dp 0dp inset;

/* 多重阴影（逗号分隔） */
box-shadow: #f7768e60 -2dp -2dp 4dp 0dp, #7aa2f760 2dp 2dp 4dp 0dp;

/* 彩色外发光（零偏移 + spread） */
box-shadow: #7aa2f780 0dp 0dp 8dp 2dp;
```

### 重要限制

> **`box-shadow` 不支持动画！** 不能参与 transition 或 @keyframes。
> 需要可动画的发光效果，请用 `filter: drop-shadow()` 代替。

### drop-shadow vs box-shadow 选择

| 特性 | `box-shadow` | `filter: drop-shadow()` |
|------|-------------|------------------------|
| 形状 | 始终矩形（跟随 border-radius） | 跟随元素实际形状（含透明区域） |
| 多重阴影 | 支持（逗号分隔） | 不支持 |
| 内阴影 | 支持（`inset`） | 不支持 |
| 动画 | **不支持** | **支持** |
| 性能 | 缓存纹理，较快 | 每帧渲染，较慢 |

---

## 6. mask-image — 遮罩

`mask-image` 用装饰器的 alpha 通道裁切元素的可见区域。

### 语法

```css
mask-image: <decorator>;
```

### 支持的装饰器

- `image()` — 图片遮罩
- `linear-gradient()` — 线性渐变遮罩
- `radial-gradient()` — 径向渐变遮罩
- `conic-gradient()` — 锥形渐变遮罩

### 渐变淡出效果

```css
.mask-demo {
    background-color: #7aa2f7;
    mask-image: linear-gradient(180deg, #000000ff, #00000000);
}
```

从上到下，元素从完全可见渐变为完全透明。遮罩的**黑色=可见，透明=隐藏**。

### 多重遮罩

```css
mask-image: image("icon.png" scale-none 15px 50%),
            linear-gradient(#0000, #000f);
```

多个遮罩的 alpha 通道相乘。

---

## 7. 演示场景解析

### 场景结构

```
FilterScene
├── setupDataModel()     → 绑定 poisoned / status_text
├── toggle_poison        → 切换中毒状态（SetClass + animation）
└── clean()              → 卸载文档 + 移除数据模型
```

### 三列演示内容

```
┌──────────────┬──────────────┬──────────────┐
│ filter       │ Interactive  │ box-shadow   │
│ (hover demo) │              │              │
│              │ [Frosted    ]│ ■ outer      │
│ ■ blur       │ [  Glass    ]│ ■ inset      │
│ ■ brightness │              │ ■ multi      │
│ ■ contrast   │ [A] Hero     │ ■ colored    │
│ ■ grayscale  │     HP:85    │              │
│ ■ sepia      │  POISONED    │ [Hover me]   │
│ ■ hue-rotate │ [Tog Poison] │  glow effect │
│ ■ invert     │              │              │
│ ■ saturate   │ [Rainbow]    │ ▒▒▒ mask     │
│ ■ drop-shadow│  hue-cycle   │ ▒▒▒ fade     │
├──────────────┴──────────────┴──────────────┤
│ info bar                                    │
└─────────────────────────────────────────────┘
```

### 左列：9 种 Filter 函数

每种滤镜用一个色块展示效果，hover 时恢复原始值，配合 `transition: filter 0.5s`
实现平滑过渡。这演示了 filter 属性的可过渡性。

### 中列：交互效果

1. **毛玻璃面板**：4 个彩色方块上叠加 `backdrop-filter: blur(5dp)` 的半透明面板
2. **中毒效果**：点击按钮给头像添加/移除 `poisoned` class，触发 hue-rotate + saturate 动画
3. **色相环绕**：`hue-rotate(0deg)` → `hue-rotate(360deg)` 的 infinite 循环

### 右列：阴影与遮罩

1. **4 种 box-shadow**：外阴影、内阴影、多重阴影、彩色发光
2. **可动画发光**：`filter: drop-shadow()` 的 hover 过渡（因为 box-shadow 不可动画）
3. **渐变遮罩**：`mask-image: linear-gradient()` 实现文字渐隐

---

## 8. C++ 侧：中毒效果的实现

```cpp
ctor.BindEventCallback("toggle_poison",
    [this](Rml::DataModelHandle model, Rml::Event&, const Rml::VariantList&) {
        if (!doc_) return;
        auto* avatar = doc_->GetElementById("avatar");
        if (!avatar) return;

        poisoned_ = !poisoned_;
        model.DirtyVariable("poisoned");

        avatar->SetClass("poisoned", poisoned_);

        status_text_ = poisoned_
            ? "Poisoned! hue-rotate + saturate"
            : "Poison cured";
        model.DirtyVariable("status_text");
    });
```

要点：
- `SetClass("poisoned", true/false)` 添加/移除 CSS class
- RCSS 中 `.avatar.poisoned` 声明了 `animation: 2s ... infinite poison-pulse`
- 添加 class 后动画自动开始，移除 class 后动画停止
- `data-if="poisoned"` 控制 "POISONED" 标签的显示/隐藏

---

## 9. 性能注意事项

| 效果 | 开销 | 建议 |
|------|------|------|
| `filter: blur()` | **高** — 每帧重新模糊 | 避免对大面积元素使用 |
| `backdrop-filter: blur()` | **高** — 需要读回下层像素 | 限制使用数量 |
| `box-shadow` | **低** — 渲染后缓存为纹理 | 放心使用 |
| `filter: drop-shadow()` | **中** — 每帧渲染 | 避免大模糊半径 |
| `filter: grayscale/sepia/...` | **低** — 纯色彩矩阵运算 | 放心使用 |
| `mask-image` | **低** — 额外 alpha 乘法 | 放心使用 |

一般原则：模糊类操作开销最大，颜色变换开销最小。

---

## 10. 练习

### 练习 1：死亡灰度化

角色死亡（HP=0）时，对整个状态面板应用 `filter: grayscale(1) brightness(0.5)`，
用 transition 实现平滑变灰效果。

### 练习 2：技能冷却遮罩

用 `mask-image: conic-gradient(from 0deg, black 50%, transparent 50%)` 实现
扇形遮罩，通过 C++ 端动态更新角度模拟技能冷却倒计时。

### 练习 3：菜单毛玻璃

为 L10 的弹出窗口添加 `backdrop-filter: blur(3dp) brightness(0.8)`，
使窗口叠加在游戏画面上时产生毛玻璃效果。

---

## 11. 要点回顾

| 概念 | 要点 |
|------|------|
| `filter` | 10 种滤镜函数，空格分隔多个，**可动画** |
| `backdrop-filter` | 语法同 filter，作用于元素**下方**内容 |
| `box-shadow` | `[inset] color offset-x offset-y [blur] [spread]`，**不可动画** |
| `mask-image` | 用装饰器 alpha 裁切元素，支持 gradient 和 image |
| `drop-shadow` vs `box-shadow` | drop-shadow 可动画但较慢；box-shadow 不可动画但有缓存 |
| 毛玻璃 | `backdrop-filter: blur()` + 半透明 `background-color` |
| 中毒效果 | `@keyframes` + `filter: hue-rotate() saturate()` |
| 性能 | 模糊开销最大，颜色变换开销最小 |

---

**上一课 <-** [L10: 动画、变换与过渡](L10-animation.md)
**下一课 ->** L12: JRPG 窗口框架与键盘导航（待完成）
