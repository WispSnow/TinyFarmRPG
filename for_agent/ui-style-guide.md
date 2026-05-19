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

### 3.1 主面板 vs 子卡片画框

**所有**弹出场景最外层都用同一种主面板。需要在主面板内部分组时, 再叠加"子卡片画框"。

| 名称 | class | sprite | 视觉 | 用途 |
|------|-------|--------|------|------|
| **主面板** | `tf-scene-panel` | `inventory-panel-bg` | 米色木板填充 + 深褐色 ninepatch 边框 | 所有弹出场景的最外层窗口 |
| **子卡片画框** | `tf-scene-card-frame` | `menu-party-card-bg` | **透明填充** + 深褐色 ninepatch 边框 (仅边框) | 主面板内的分组容器: party-card / portrait card / 信息卡 / 透明 body 场景的控制区 |
| 实心子卡 | `tf-scene-subcard` | — (纯色) | `#24283bcc` 半透明蓝紫 + `1dp #414868` | 列表条目 / quest-entry / action-menu 等需要明确"压色块"的场合 |

> 子卡片画框是**仅边框**, 看到的填充色取决于其父容器 (通常是 `tf-scene-panel` 的米色木板)。它适合在大面板内画"分区线", 不会引入第二种底色。
> 当场景使用透明 body (例: AppearanceCustomize, `background-color: transparent`), `tf-scene-card-frame` 直接漂浮在 3D 场景之上, 此时框内文字要改用 `tf-scene-text-dark` 才能清晰。

```html
<!-- 标准结构: dim 层 + 主面板 + (可选) 内部子卡片 -->
<body class="tf-screen-root tf-nav-root" data-model="my_scene">
    <div id="my-overlay" class="tf-modal-overlay"></div>

    <div id="my-panel" class="tf-scene-panel">
        <div id="my-header">{{ title_text }}</div>

        <div id="my-card-row">
            <div class="tf-scene-card-frame">
                <!-- 一组分项: portrait + 名字 + meta -->
            </div>
        </div>
    </div>
</body>
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

`tf-modal-overlay` 默认是 `#00000099`。需要更暗时, 在自己的 rcss 里覆写 `background-color`, 但保留 class 以维持全屏布局。

> **不要**再使用 `tf-modal-panel` / `tf-modal-panel-strong` (那是无背景图的扁平黑底, 现已视为旧风格)。

### 3.3 文字色 / 面板配色搭配

| 底 | 默认文字 | 备注 |
|----|---------|------|
| `tf-scene-panel` (米色木板) | 白 + 黑阴影 (`tf-scene-title`) | 三个基准场景的标准选择 |
| `tf-scene-card-frame` 内部 (背后是主面板) | 白 + 黑阴影 | 透明框看到的还是米色木板, 沿用白字 |
| `tf-scene-card-frame` 内部 (背后是透明 body) | 暗色 + 白阴影 (`tf-scene-text-dark`) | 仅 AppearanceCustomize 这类场景适用 |
| `tf-scene-subcard` (蓝紫色块) | 白 + 黑阴影 | |

### 3.4 几何 (panel 尺寸)

逻辑分辨率 **640×360 dp**。一些已落地的常见尺寸供参考:

- 大型菜单 (Inventory 双栏): 360×238dp，左上锚定 `140, 68`
- 商店双栏: 580×308dp，左上锚定 `30, 26`
- 中型卡片对话 (Quest offer): 384×250dp，居中
- 小型确认 (Rest / Save 确认): 232-272dp 宽，居中

主面板默认 `padding: 10dp`, 信息更密集时可降至 `8dp`, 卡片式对话用 `16dp`。
`tf-scene-card-frame` 默认 `padding: 8dp`, 视内容可覆写。

---

## 4. 按钮 (Button)

**永远不要手写按钮配色**。已有四种统一按钮, 直接 class 复用 (定义在 `theme/menu_widgets.rcss`):

| class | 视觉 | 用途 |
|-------|------|------|
| `tf-button-primary`   | 蓝色 ninepatch | 主要操作 (Confirm/Accept/Save) |
| `tf-button-secondary` | 灰色 ninepatch | 次要操作 (Cancel/Decline/Leave) |
| `tf-button-light`     | 木牌 ninepatch (米色, 深色字) | **`tf-scene-panel` 主面板上的所有按钮** (Appearance / Shop 动作区) |
| `tf-icon-button`      | 16dp 图标按钮 | Stepper、关闭、排序、垃圾桶等 |

> 主面板本身就是米色木板, 蓝色 `tf-button-primary` 与之冲突。**`tf-scene-panel` 内默认全用 `tf-button-light`**, 视觉与基准场景一致。
> `tf-button-primary/secondary` 当前主要在旧风格扁平黑底场景里出现, 迁移时一并替换成 `tf-button-light`。

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

主面板内的分组方式 (按"分隔强度"从弱到强):

| 强度 | class | 视觉 |
|-----|-------|------|
| 最弱 | `tf-scene-divider-h-faint` | 1dp 暖灰 (44 透明), 仅分行 |
| 弱 | `tf-scene-divider-h` | 1dp 暖灰 (66 透明), 头部底边常用 |
| 中 | `tf-scene-card-frame` | 透明填充 + 深褐 ninepatch 边框, 不改变底色 |
| 强 | `tf-scene-subcard` | `#24283bcc` 半透明蓝紫色块 + `1dp #414868` 边框, 明显压色 |

> 在米色主面板里画"分组容器"优先用 `tf-scene-card-frame`; 只有当条目需要明确高亮 (quest entry / action menu / 列表行的实心选中态背景) 才上 `tf-scene-subcard`。

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

    <div id="my-panel" class="tf-scene-panel">
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

            <!-- 分组容器 (透明边框, 主面板米色透出) -->
            <div class="tf-scene-card-frame">
                <div class="tf-scene-info-row">
                    <div class="tf-scene-info-label">Total</div>
                    <div class="tf-scene-info-value-gold">{{ total_text }}</div>
                </div>
            </div>
        </div>

        <div id="my-actions">
            <button class="tf-button-light tf-nav-auto tf-focus-ring-blue"
                    data-event-click="confirm">Confirm</button>
            <button class="tf-button-light tf-nav-auto tf-focus-ring-blue"
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
| 弹出场景最外层一律 `tf-scene-panel` (米色木板主面板) | 用 `tf-modal-panel*` + 自己拼 `background-color` 黑底 |
| 内部分组用 `tf-scene-card-frame` (透明边框) | 嵌一个二次填充的纯色面板, 让画面出现两层底色 |
| `tf-scene-panel` 上的按钮一律 `tf-button-light` | 主面板里塞蓝色 `tf-button-primary` (与米色木板冲突) |
| 颜色用本指引 §1 中的值 | 写新的灰/蓝色变体 (`#1a2535` / `#46546a` 等) |
| 字号用 rem | 写死 `font-size: 12dp` |
| 按钮组合 `tf-button-* + tf-nav-auto + tf-focus-ring-*` | 自己拼一套按钮 hover/active 样式 |
| 透明 body 场景在 `tf-scene-card-frame` 里用 `tf-scene-text-dark` | 主面板内嵌的 `tf-scene-card-frame` 也用暗色字 (背后是米色, 反而读不清) |
| 滚动容器加 `tf-scene-scroll` | 单独写 `scrollbarvertical` |
| 信息行用 `tf-scene-info-row` | 用 table 或手写 label/value 各自 absolute |
| panel 几何 (left/top/w/h) 写在场景自己的 rcss | 在 utility class 里写死尺寸 |

---

## 12. 待迁移场景清单 (参考)

以下场景**仍是旧扁平黑底风格**, 后续重构时统一改成"`tf-scene-panel` 主面板 + `tf-scene-card-frame` 内嵌分组 + `tf-button-light` 按钮":

- `scenes/quest_offer.{rml,rcss}` — 外框换 `tf-scene-panel`, 中间的"任务卡 `#quest-offer-card`"换 `tf-scene-card-frame`, speaker 用 `tf-scene-title-gold`, 按钮改 `tf-button-light`。
- `scenes/recruit_offer.{rml,rcss}` — 同上, portrait + copy 整组放进 `tf-scene-card-frame`。
- `scenes/rest_dialog.{rml,rcss}` — `tf-scene-panel`, stepper 周围加 `tf-scene-card-frame` 或保持极简, 按钮 `tf-button-light`。
- `scenes/pause_menu.{rml,rcss}` — `tf-scene-panel`, 三组 stepper 行各自放进 `tf-scene-card-frame`, 按钮 `tf-button-light`。
- `scenes/save_slot_select.{rml,rcss}` — `tf-scene-panel`, 每个 save slot 按钮可保留 `tf-button-light` 现状; 确认弹窗同样 `tf-scene-panel`。
- `scenes/battle.rcss` 中的 `#battle-victory-panel` 等内嵌弹窗 — `tf-scene-panel`, 替换 `#fff2a6` → `#fce97f`、`#46546a` → `#414868`, victory-row 用 `tf-scene-card-frame` 或 `tf-scene-subcard`。

---

## 13. 遇到拿不准的场景

1. 先在 3 个基准场景里找最接近的对应组件;
2. 没有的话, 在本指引 §1-§9 找最近似的 utility, 优先复用 + 在场景 rcss 里只调几何;
3. 仍解决不了, 再回到本指引修订 §1 调色板或扩 `overlay_scene.rcss`, 让规则比例外更廉价。
