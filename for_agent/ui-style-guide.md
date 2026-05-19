# 弹出场景 UI 风格指引

> 本指引面向游戏场景中的**弹出/覆盖式 UI**（InventoryMenu、Shop、Quest、Recruit、Pause、SaveSlot、Appearance、Battle 内层窗口等）。
> HUD、对话气泡、Title 等非"窗口式"UI 不在本指引强约束范围, 但配色与字体规范同样适用。

基准场景: `ui/rmlui/scenes/inventory_menu.{rml,rcss}` / `shop_menu.{rml,rcss}` / `appearance_customize.{rml,rcss}`。
共享样式: `ui/rmlui/theme/overlay_scene.rcss` + 已有 `theme/{base,reset,modal,nav,menu_widgets,slot_widgets,spritesheet}.rcss`。
编写 RmlUi/RCSS 前**先读** `for_agent/rmlui-guide.md`（RCSS 与标准 CSS 的差异、避坑要点）。

---

## 1. 调色板 (Tokyo Night 风)

### 1.1 中性色 (背景 / 文字)

| 用途 | Token / Hex | 备注 |
|------|------------|------|
| 标题文字 | `#ffffff` + `shadow(1dp 1dp #000000cc)` | 配 `tf-scene-title` |
| 副标题 / 元信息 | `#a9b1d6` | 配 `tf-scene-subtitle` / `.tf-scene-meta` |
| 描述文本 | `#7888bd` | 配 `tf-scene-description` |
| 列表行默认 | `#f7f7f7` | |
| 禁用文字 | `#666666` (opacity 0.6) | |
| 浅面板上的暗色文字 | `#2a1f17` + `shadow(1dp 1dp #ffffff44)` | 配 `tf-scene-text-dark` |
| 浅面板上的次级暗色 | `#6f4f37cc` | 配 `tf-scene-text-dark-muted` |

### 1.2 强调色 (语义)

| 用途 | Hex | 配套 class |
|------|-----|----------|
| 金币 / 数值奖励 | `#e0af68` | `tf-scene-accent-gold` |
| 高亮 / 选中文字 | `#fce97f` | `tf-scene-title-gold` |
| 主要交互 (蓝) | `#7aa2f7` (focus `#8db8ff`) | `tf-focus-ring-blue` |
| 成功 / 友军 | `#9ece6a` | `tf-scene-accent-success` |
| 提示 / 信息 (青) | `#7dcfff` | `tf-scene-accent-info` |
| 危险 / 破坏性 | `#f7768e` (focus `#f78fa3`) | `tf-focus-ring-danger` / `tf-scene-accent-danger` |
| 分隔线 (暖灰) | `#d9a88a` (透明度 33/44/66 三档) | `tf-scene-divider-h*` |

> ⚠️ **禁止**在新代码中引入未在此表中出现的灰/蓝/金色变体。需要变体时请扩展本表。

---

## 2. 字体与字号

字体: `VonwaonBitmap 16px` (位图字体, **不要使用 italic**, 详见 `rmlui-guide.md`)。

字号统一用 **rem** (1rem = 16dp 基准):

| rem | px | 用途 |
|-----|----|----|
| `0.5rem`    | 8dp  | 计数徽标、超小注脚 |
| `0.5625rem` | 9dp  | 状态/小注、置中提示 |
| `0.625rem`  | 10dp | 菜单常规正文、列表项 |
| `0.6875rem` | 11dp | 标准 label / 选项文字 |
| `0.75rem`   | 12dp | 子面板标题 |
| `1rem`      | 16dp | 主标题 (默认 body 字号) |

> 不要使用 `font-size: 12dp` 直接写死, 使用 rem 让 `tf-font-small/normal/large` 全局缩放生效 (`body.tf-font-*` 由 `UserSettingsService` 控制)。

文字阴影统一两种:
- 深色面板上: `font-effect: shadow(1dp 1dp #000000cc);`
- 浅色 (木纹) 面板上: `font-effect: shadow(1dp 1dp #ffffff44);` 或 `#ffffff66`

---

## 3. 面板 (Panel)

### 3.1 两种主面板

| 风格 | 适用 | class | 来源 sprite |
|------|------|-------|------------|
| **深色 (蓝紫)** | 信息密度高的菜单 (Inventory / Shop / Quest list / Battle window) | `tf-scene-panel-dark` | `inventory-panel-bg` |
| **浅色 (木纹)** | 卡片式弹窗 / 个人化界面 (Appearance / Recruit / Quest offer 卡片 / Rest) | `tf-scene-panel-light` | `menu-party-card-bg` |

> 浅色面板内的文字**必须**用 `tf-scene-text-dark` 系列 (`#2a1f17` + 白阴影), 否则在木纹底色上读不清。深色面板用白/灰文字。

```html
<!-- 推荐: 用 utility 而不是再写一份 ninepatch -->
<div id="my-panel" class="tf-scene-panel-dark">
    <!-- panel 几何 (left/top/width/height) 在该场景自己的 rcss 里写 -->
</div>
```

```css
/* my_scene.rcss: 只声明几何, 不要重复 decorator */
#my-panel {
    position: absolute;
    left: 30dp;
    top: 26dp;
    width: 580dp;
    height: 308dp;
}
```

### 3.2 全屏 dim 层

固定模板:
```html
<body class="tf-screen-root tf-nav-root" data-model="...">
    <div id="my-overlay" class="tf-modal-overlay"></div>
    <div id="my-panel" class="tf-scene-panel-dark">...</div>
</body>
```

> `tf-modal-overlay` 默认是 `#00000099`。需要更暗时, 在自己的 rcss 里覆写 `background-color`, 但保留 class 以维持全屏布局。
> **不要**再使用 `tf-modal-panel` / `tf-modal-panel-strong` (那是无背景图的扁平黑底, 现已视为旧风格)。

### 3.3 几何 (panel 尺寸)

逻辑分辨率 **640×360 dp**。一些已落地的常见尺寸供参考:

- 大型菜单 (Inventory 双栏): 360×238dp，左上锚定 `140, 68`
- 商店双栏: 580×308dp，左上锚定 `30, 26`
- 中型卡片对话 (Quest offer): 384×250dp，居中
- 小型确认 (Rest / Save 确认): 232-272dp 宽，居中

内边距 `padding: 10dp` 是默认值, 信息更密集时可降至 `8dp`, 卡片式对话用 `16dp`。

---

## 4. 按钮 (Button)

**永远不要手写按钮配色**。已有四种统一按钮, 直接 class 复用 (定义在 `theme/menu_widgets.rcss`):

| class | 视觉 | 用途 |
|-------|------|------|
| `tf-button-primary`   | 蓝色 ninepatch | 主要操作 (Confirm/Accept/Save) |
| `tf-button-secondary` | 灰色 ninepatch | 次要操作 (Cancel/Decline/Leave) |
| `tf-button-light`     | 木牌 ninepatch (浅色, 深色字) | **木纹面板上的所有按钮** (Appearance/Shop 动作区) |
| `tf-icon-button`      | 16dp 图标按钮 | Stepper、关闭、排序、垃圾桶等 |

> **木纹面板里不要混用 primary/secondary**, 视觉会割裂。Appearance/Shop detail 都统一用 `tf-button-light`。

按钮尺寸由场景自己定 (`width: 100dp` 之类), 高度由 utility 决定 (32dp), 不要覆写。

`tf-icon-button` 必须组合一个 icon class (例如 `icon-arrow-left-light` / `icon-arrow-right-light`)。新图标请加进 `theme/spritesheet.rcss` 的 `@spritesheet ui-buttons`。

### 4.1 必备 nav / focus class

只要按钮**可被键盘/手柄聚焦**（默认情况都需要！）:

```html
<button class="tf-button-primary tf-nav-auto tf-focus-ring-blue"
        data-event-click="confirm">Confirm</button>
```

- `tf-nav-auto`: 给按钮启用 `tab-index/nav-*` (RmlUi 的 button **不会**自动可聚焦, 见 `rmlui-guide.md`)。
- `tf-focus-ring-*`: focus/hover 时统一 translateY + 改色。
  - `tf-focus-ring-blue`: 常规
  - `tf-focus-ring-gold`: 选择类 (tab / mode toggle / 列表项)
  - `tf-focus-ring-danger`: 破坏性 (删除 / 卸下装备)

> `tf-button-primary/secondary/light` 内置了 tab-index 与 hover/active 动画, 已经够用; 上面三种 focus-ring 主要给**非标准按钮元素** (tab、列表行 `<button>`、map marker) 用。混合使用也兼容。

---

## 5. Tab / Mode Toggle

参见 `inventory_menu` 的图标 tab 以及 `shop_menu` 的 Buy/Sell + 类别两层 tab。模式:

- 一组 tab 共享一根底边 `tf-scene-header-strip` (1dp `#d9a88a66`)。
- 单个 tab 不选中: 文字 `#d8dee9d0`, `border-color: transparent`。
- 选中: `color: #fce97f`, `border-color: #7aa2f7` (蓝色 2dp 底边)。
- 整组聚焦时再点亮: 用 `tf-scene-section-focus-frame.focused` 框住整组。

---

## 6. 列表行 (Selectable Row)

商店物品行、动作菜单、quest entry 都用同一套规则:

| 状态 | 视觉 |
|------|------|
| 默认 | 透明背景、白文字 |
| `:hover` / `:focus` | `background: #41486888` |
| `.selected` | `background: #24283bcc` + `color: #fce97f` |
| `:disabled` / `.disabled` | `color: #666666; opacity: 0.6` |

直接用 `tf-scene-list-entry`:

```html
<button class="tf-scene-list-entry tf-nav-auto tf-focus-ring-gold"
        data-for="entry : entries"
        data-class-selected="entry.is_selected"
        data-class-disabled="entry.is_disabled"
        data-attrif-disabled="entry.is_disabled">
    <div class="entry-icon"></div>
    <div class="entry-name">{{ entry.name }}</div>
</button>
```

> Drag-drop 槽位 (背包/快捷栏) 是另一套规则, 用 `tf-slot-*` 系列, 详见 `theme/slot_widgets.rcss`。

---

## 7. 信息行 (Label : Value)

商店详情、装备总览、招募信息均同款:

```html
<div class="tf-scene-info-row">
    <div class="tf-scene-info-label">Price</div>
    <div class="tf-scene-info-value-gold">{{ price_text }}</div>
</div>
```

label 宽度建议 80-100dp, 由场景给固定 `width`。

---

## 8. 分隔线 / 子卡片

- 1dp 暖灰横线: `tf-scene-divider-h` (66 透明) / `tf-scene-divider-h-faint` (44 透明)。
- 主面板内嵌"子方块" (quest entry / action menu 等): `tf-scene-subcard` (`#24283bcc` + `1dp #414868`)。

---

## 9. 滚动条

只要容器需要滚动, **必须**加 `tf-scene-scroll`:

```html
<div id="my-list" class="tf-scene-scroll">
    <button class="tf-scene-list-entry" data-for="...">...</button>
</div>
```

- 宽度统一 4dp, 暖色调 (track `#d9a88a33`, thumb `#b56f5ddd`)。
- 不要再单独写 `scrollbarvertical` 规则。

---

## 10. 新场景模板 (复制即用)

### 10.1 RML 骨架

```html
<rml>
<head>
    <link type="text/rcss" href="../theme/base.rcss"/>
    <link type="text/rcss" href="../theme/reset.rcss"/>
    <link type="text/rcss" href="../theme/modal.rcss"/>
    <link type="text/rcss" href="../theme/nav.rcss"/>
    <link type="text/rcss" href="../theme/spritesheet.rcss"/>
    <link type="text/rcss" href="../theme/menu_widgets.rcss"/>
    <link type="text/rcss" href="../theme/overlay_scene.rcss"/>
    <link type="text/rcss" href="my_scene.rcss"/>
</head>
<body class="tf-screen-root tf-nav-root" data-model="my_scene">
    <div id="my-overlay" class="tf-modal-overlay"></div>

    <div id="my-panel" class="tf-scene-panel-dark">
        <div id="my-header" class="tf-scene-header-strip">
            <div class="tf-scene-title">{{ title_text }}</div>
        </div>

        <div id="my-body">
            <!-- 列表 -->
            <div id="my-list" class="tf-scene-scroll">
                <button class="tf-scene-list-entry tf-nav-auto tf-focus-ring-gold"
                        data-for="entry : entries"
                        data-class-selected="entry.is_selected"
                        data-event-click="entry_select">
                    <div class="entry-name">{{ entry.name }}</div>
                </button>
            </div>

            <!-- 信息行 -->
            <div class="tf-scene-info-row">
                <div class="tf-scene-info-label">Total</div>
                <div class="tf-scene-info-value-gold">{{ total_text }}</div>
            </div>
        </div>

        <div id="my-actions">
            <button class="tf-button-primary tf-nav-auto tf-focus-ring-blue"
                    data-event-click="confirm">Confirm</button>
            <button class="tf-button-secondary tf-nav-auto tf-focus-ring-blue"
                    data-event-click="cancel">Cancel</button>
        </div>
    </div>
</body>
</rml>
```

### 10.2 RCSS 骨架

```css
body, div, h1, h2, h3, h4, p, hr {
    display: block;
}

/* 只声明几何 / 布局, 颜色与字号交给 utility class */

#my-panel {
    position: absolute;
    left: 60dp;
    top: 40dp;
    width: 520dp;
    height: 280dp;
}

#my-list {
    width: 100%;
    height: 180dp;
}

#my-actions {
    margin-top: 8dp;
    display: flex;
    flex-direction: row;
    gap: 8dp;
}
```

---

## 11. Do / Don't

| ✅ Do | ❌ Don't |
|------|---------|
| 用 `tf-scene-panel-dark/light` 套面板 | 用 `tf-modal-panel` + 自己拼 `background-color` 黑底 |
| 颜色用本指引 §1 中的值 | 写新的灰/蓝色变体 (`#1a2535` / `#46546a` 等) |
| 字号用 rem | 写死 `font-size: 12dp` |
| 按钮组合 `tf-button-* + tf-nav-auto + tf-focus-ring-*` | 自己拼一套按钮 hover/active 样式 |
| 浅色面板用 `tf-button-light` 与 `tf-scene-text-dark` | 浅面板里塞蓝色 `tf-button-primary` |
| 滚动容器加 `tf-scene-scroll` | 单独写 `scrollbarvertical` |
| 信息行用 `tf-scene-info-row` | 用 table 或手写 label/value 各自 absolute |
| panel 几何 (left/top/w/h) 写在场景自己的 rcss | 在 utility class 里写死尺寸 |

---

## 12. 待迁移场景清单 (参考)

以下场景**仍是旧扁平风格**, 后续重构时按本指引迁移即可:

- `scenes/quest_offer.{rml,rcss}` — 改为 `tf-scene-panel-light`, speaker 用 `tf-scene-title-gold`, 内嵌 card 换 `tf-scene-subcard`。
- `scenes/recruit_offer.{rml,rcss}` — 同上, 卡片改 `tf-scene-panel-light`。
- `scenes/rest_dialog.{rml,rcss}` — `tf-scene-panel-light`, 按钮 `tf-button-light`。
- `scenes/pause_menu.{rml,rcss}` — `tf-scene-panel-dark`。
- `scenes/save_slot_select.{rml,rcss}` — `tf-scene-panel-dark` + 确认弹窗 `tf-scene-panel-light`。
- `scenes/battle.rcss` 中的 `#battle-victory-panel` 等内嵌弹窗 — `tf-scene-panel-dark`, 替换 `#fff2a6` → `#fce97f`、`#46546a` → `#414868`。

---

## 13. 遇到拿不准的场景

1. 先在 3 个基准场景里找最接近的对应组件;
2. 没有的话, 在本指引 §1-§9 找最近似的 utility, 优先复用 + 在场景 rcss 里只调几何;
3. 仍解决不了, 再回到本指引修订 §1 调色板或扩 `overlay_scene.rcss`, 让规则比例外更廉价。
