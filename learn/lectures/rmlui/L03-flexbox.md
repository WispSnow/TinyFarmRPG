# L03: Flexbox 布局

> 配套代码：`learn/rmlui_flexbox/` | 构建目标：`learn_rmlui_flexbox`

---

## 1. 什么是 Flexbox

Flexbox（弹性盒布局）是一种**一维布局模型**——在一个方向（水平或垂直）上排列子元素，同时灵活控制它们的对齐和空间分配。

在 JRPG UI 中，几乎所有布局都依赖 Flexbox：
- 快捷栏（水平等分按钮）
- 菜单列表（垂直排列选项）
- 背包格子（换行的网格）
- 战斗 UI 底栏（多角色状态并排）

---

## 2. Flex 容器与项目

```css
.container {
    display: flex;    /* 变成 flex 容器 */
}
```

容器内的**直接子元素**自动成为 flex 项目（flex item），按主轴方向排列。

```
                          主轴（main axis）
容器 ┌─────────────────────────────────────────┐
     │ [item A]  [item B]  [item C]  [item D]  │ ← flex 项目
     └─────────────────────────────────────────┘
                                          │
                                   交叉轴（cross axis）
```

---

## 3. flex-direction — 主轴方向

```css
.row           { flex-direction: row; }            /* → 水平，从左到右（默认） */
.row-reverse   { flex-direction: row-reverse; }    /* ← 水平，从右到左 */
.column        { flex-direction: column; }         /* ↓ 垂直，从上到下 */
.column-reverse{ flex-direction: column-reverse; } /* ↑ 垂直，从下到上 */
```

| 值 | 主轴方向 | 典型场景 |
|------|----------|----------|
| `row` | → 水平 | 快捷栏、按钮组、标签页 |
| `column` | ↓ 垂直 | 菜单列表、表单字段、聊天消息 |
| `row-reverse` | ← 反向水平 | 右对齐的操作栏 |
| `column-reverse` | ↑ 反向垂直 | 从底部向上堆叠的消息 |

---

## 4. flex-wrap — 换行

默认情况下，所有 flex 项目都挤在一行/一列中。`flex-wrap` 允许它们换行：

```css
.no-wrap  { flex-wrap: nowrap; }        /* 默认：不换行，可能溢出 */
.wrap     { flex-wrap: wrap; }          /* 放不下时自动换行 */
.wrap-rev { flex-wrap: wrap-reverse; }  /* 换行但交叉轴反向 */
```

`flex-flow` 是 `flex-direction` + `flex-wrap` 的简写：

```css
.grid { flex-flow: row wrap; }  /* 水平排列 + 自动换行 = 网格效果 */
```

---

## 5. justify-content — 主轴对齐

控制 flex 项目在**主轴**上如何分布：

```
flex-start（默认）        center                space-between
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│[A][B][C]         │  │     [A][B][C]    │  │[A]    [B]    [C] │
└──────────────────┘  └──────────────────┘  └──────────────────┘

space-around             space-evenly           flex-end
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ [A]  [B]  [C]    │  │  [A]  [B]  [C]  │  │         [A][B][C]│
└──────────────────┘  └──────────────────┘  └──────────────────┘
```

| 值 | 行为 |
|------|------|
| `flex-start` | 靠主轴起始端（默认） |
| `flex-end` | 靠主轴结束端 |
| `center` | 居中 |
| `space-between` | 首尾贴边，中间等分间距 |
| `space-around` | 每项两侧等距（首尾间距为中间的一半） |
| `space-evenly` | 所有间距完全相等 |

---

## 6. align-items / align-self — 交叉轴对齐

### align-items（容器级别）

控制所有 flex 项目在**交叉轴**上如何对齐：

```css
.container { align-items: center; }  /* 所有子项垂直居中 */
```

| 值 | 行为 |
|------|------|
| `stretch` | 拉伸填满交叉轴（默认） |
| `flex-start` | 靠交叉轴起始端 |
| `flex-end` | 靠交叉轴结束端 |
| `center` | 交叉轴居中 |
| `baseline` | 按文字基线对齐 |

### align-self（项目级别）

单个项目覆盖容器的 `align-items`：

```css
.special-item { align-self: flex-end; }  /* 只有这个项目靠底部 */
```

### align-content（多行时）

当 `flex-wrap: wrap` 产生多行时，`align-content` 控制**行与行之间**的分布：

```css
.grid {
    flex-wrap: wrap;
    align-content: flex-start;  /* 多行紧凑排列在顶部 */
}
```

支持的值与 `justify-content` 相同，额外支持 `stretch`（默认）。

---

## 7. flex-grow / flex-shrink / flex-basis — 弹性分配

### flex-basis

项目在分配多余空间前的**初始尺寸**：

```css
.item { flex-basis: 100dp; }  /* 初始宽度 100dp（水平排列时） */
.item { flex-basis: auto; }   /* 使用 width/height 或内容尺寸（默认） */
```

### flex-grow

多余空间如何**分配**给各项：

```css
/* 三个项目，总宽 < 容器宽度时 */
.a { flex-grow: 1; }  /* 拿到 1/3 多余空间 */
.b { flex-grow: 2; }  /* 拿到 2/3 多余空间 */
.c { flex-grow: 0; }  /* 不增长（默认） */
```

### flex-shrink

空间不足时各项如何**收缩**：

```css
.item { flex-shrink: 1; }  /* 默认：等比收缩 */
.fixed { flex-shrink: 0; } /* 不收缩，保持原始尺寸 */
```

### flex 简写

```css
.item { flex: 1; }           /* flex-grow:1, flex-shrink:1, flex-basis:0 */
.item { flex: 0 0 80dp; }    /* 不增不缩，固定 80dp */
.item { flex: 2 1 auto; }    /* grow:2, shrink:1, basis:auto */
```

常用模式：

| 写法 | 含义 | 场景 |
|------|------|------|
| `flex: 1` | 等分所有空间 | 等宽按钮栏 |
| `flex: 0 0 auto` | 由内容决定尺寸 | 图标、固定标签 |
| `flex: 0 0 64dp` | 固定尺寸 | 物品格子 |

---

## 8. gap — 间距

`gap` 设置 flex 项目之间的间距，比用 margin 更简洁：

```css
.container {
    gap: 8dp;               /* 行间距和列间距都是 8dp */
    row-gap: 12dp;          /* 行间距（换行时行与行的距离） */
    column-gap: 8dp;        /* 列间距（同行项目之间的距离） */
}
```

> `gap` 是 `row-gap` + `column-gap` 的简写。单值时两个方向相同。

---

## 9. 实用布局模式

### 水平居中

```css
.center-h {
    display: flex;
    justify-content: center;
}
```

### 完全居中（水平 + 垂直）

```css
.center-both {
    display: flex;
    justify-content: center;
    align-items: center;
    height: 200dp;           /* 需要指定容器高度 */
}
```

### 固定侧栏 + 弹性内容

```css
.layout      { display: flex; flex-direction: row; }
.sidebar     { flex: 0 0 120dp; }  /* 固定 120dp 宽 */
.main-content{ flex: 1; }          /* 占满剩余空间 */
```

### 网格（flex-wrap）

```css
.grid {
    display: flex;
    flex-wrap: wrap;
    gap: 4dp;
}
.grid-cell {
    flex: 0 0 48dp;    /* 每格 48×48 */
    height: 48dp;
}
```

---

## 10. 配套代码

### 场景代码

`learn/rmlui_flexbox/flexbox_scene.cpp` — 加载 `ui/rmlui/learn/learn_flexbox.rml`。

### RML 文档

文档包含 5 个 demo 区块，对应本课核心知识点：

1. **flex-direction** — row / column / row-reverse 对比
2. **justify-content** — 六种对齐方式可视化
3. **align-items** — 不同高度项目的交叉轴对齐
4. **flex-grow** — 弹性分配空间
5. **Grid (flex-wrap)** — 模拟背包物品格子

---

## 11. 构建与运行

```bash
ninja -C build/debug learn_rmlui_flexbox
./build/debug/learn/learn_rmlui_flexbox
```

---

## 12. 练习

### 练习 1：快捷栏

创建一个屏幕底部居中的水平快捷栏：
- 8 个等宽格子（`flex: 0 0 48dp`），每格 48×48dp
- `gap: 4dp`
- 整体水平居中（`justify-content: center`）
- 固定在底部（`position: fixed; bottom: 16dp`）

### 练习 2：菜单列表

创建一个垂直菜单：
- `flex-direction: column`
- 5 个菜单项：Items / Equip / Skills / Status / Save
- 每项 `padding: 8dp 16dp`，宽度填满容器
- 悬停时背景变亮

### 练习 3：背包格子

创建一个 6 列的物品网格：
- 容器固定宽度（容纳 6 个格子 + 5 个间距）
- `flex-wrap: wrap` + `gap: 4dp`
- 24 个格子（4 行 × 6 列）
- 每格显示编号 1~24
- 容器高度限制 + `overflow-y: auto` 实现滚动

---

## 13. 要点回顾

| 概念 | 要点 |
|------|------|
| 启用 Flex | `display: flex` |
| 主轴方向 | `flex-direction`: row / column / row-reverse / column-reverse |
| 换行 | `flex-wrap`: nowrap / wrap / wrap-reverse |
| 主轴对齐 | `justify-content`: flex-start / center / space-between / space-around / space-evenly |
| 交叉轴对齐 | `align-items` / `align-self`: stretch / flex-start / center / flex-end / baseline |
| 弹性分配 | `flex-grow` / `flex-shrink` / `flex-basis`，简写 `flex: 1` |
| 间距 | `gap` / `row-gap` / `column-gap` |
| 网格技巧 | `flex-wrap: wrap` + 固定 `flex-basis` = 网格布局 |

---

**上一课 ←** [L02: 盒模型与定位](L02-box-model.md)
**下一课 →** [L04: 文字排版与视觉样式](L04-styling.md)
