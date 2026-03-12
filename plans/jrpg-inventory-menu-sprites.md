# 物品栏菜单 — 素材资源表

> 记录菜单重构所需的所有 spritesheet 区域，供 RCSS `@spritesheet` 定义使用。
> 坐标格式: `(x, y, w, h)` 像素值。

---

## 1. Slot 背景 — `inventory.png` (112×112)

18×18 的物品槽背景，有两种色调（深色 / 浅色），各 3 个变体。选用变体 A。

```
  inventory.png (112×112)

  Row 0 (深色 slot):
  ┌──────┐ ┌──────┐ ┌──────┐
  │(7,9) │ │(39,9)│ │(71,9)│       18×18 each
  │ dark │ │ dark │ │ dark │       center #ab6659, border #d9a88a
  │  A ✓ │ │  B   │ │  C   │
  └──────┘ └──────┘ └──────┘

  Row 1 (浅色 slot):
  ┌──────┐ ┌──────┐ ┌──────┐
  │(7,41)│ │(39,41│ │(71,41│       18×18 each
  │light │ │light │ │light │       center #f7dbc6, border #d9a88a
  │  A ✓ │ │  B   │ │  C   │
  └──────┘ └──────┘ └──────┘

  大面板背景 (ninepatch):
  ┌─────────────────────┐
  │ (0, 64) 48×48       │           center #f7dbc6, border #d9a88a
  │ panel-bg        ✓   │
  │                     │
  └─────────────────────┘

  小 slot (14×14), 两排各 4 个:
  Row 0 (浅色): (49,65) (65,65) (81,65) (97,65)    14×14, center #d9a88a
  Row 1 (深色): (49,81) (65,81) (81,81) (97,81)    14×14, center #ab6659
```

### 选用方案

| 用途 | sprite 名称 | 区域 (x,y,w,h) | ninepatch inner | 说明 |
|------|-------------|----------------|-----------------|------|
| 背包 slot 普通 | `menu-slot-bg` | (7, 9, 18, 18) | (9, 11, 14, 14) | 深色 A |
| 背包 slot 选中 | `menu-slot-selected` | (7, 41, 18, 18) | (9, 43, 14, 14) | 浅色 A |
| 菜单面板背景 | `menu-panel-bg` | (0, 64, 48, 48) | — | 大面板 ninepatch |
| 菜单面板背景 inner | `menu-panel-bg-inner` | (10, 74, 28, 28) | — | ninepatch 内区域 |

---

## 2. 装备槽占位图标 — `Slot Armor.png` (384×64)

使用 **Row 1 浅色 (y=32)**，作为空装备槽的占位剪影。
每个图标在 32×32 的网格 cell 中（图标本身小于 16×16，截取中间的16×16区域进行绘制）。
仅使用前 7 列（col 0-6），后面的无用。

```
  Slot Armor.png (384×64)

  Row 1 浅色 (y=32, 使用此行):
  col:  0       1       2       3       4       5       6
       帽子    上衣    裤子    手套    鞋子    项链    戒指
       hat    shirt   pants   glove   boots   neck    ring
```

### 选用方案

| 装备部位 | sprite 名称 | 区域 (x,y,w,h) | 说明 |
|---------|-------------|----------------|------|
| Hat 帽子 | `equip-hat` | (8, 40, 16, 16) | 浅色占位 |
| Shirt 上衣 | `equip-shirt` | (40, 40, 16, 16) | 浅色占位 |
| Pants 裤子 | `equip-pants` | (72, 40, 16, 16) | 浅色占位 |
| Gloves 手套 | `equip-gloves` | (104, 40, 16, 16) | 浅色占位 |
| Boots 鞋子 | `equip-boots` | (136, 40, 16, 16) | 浅色占位 |
| Necklace 项链 | `equip-necklace` | (168, 40, 16, 16) | 浅色占位 |
| Ring 戒指 | `equip-ring` | (200, 40, 16, 16) | 浅色占位（ring1/ring2 共用） |

> 注意: cell 高度为 32px（y=32~63），图标内容集中在 y=40-55 区域。

---

## 3. 标签页 & HUD 图标 — `HUD.png` (416×96)

16×16 网格，26 列 × 6 行。偶数行 = 正常态，奇数行 = 按下态（成对）。

### 标签页图标（单态，选中/未选中通过 RCSS opacity 或 border 区分）

| 标签 | sprite 名称 | 区域 (x,y,16,16) | 说明 |
|------|-------------|-------------------|------|
| Inventory | `tab-inventory` | **(48, 32, 16, 16)** | 背包图标 |
| Equipment | `tab-equipment` | **(96, 16, 16, 16)** | 剑+盾图标 |
| Quests | `tab-quests` | **(112, 32, 16, 16)** | 书本图标 |
| Map | `tab-map` | **(64, 32, 16, 16)** | 地图图标 |
| Options | `tab-options` | **(144, 0, 16, 16)** | 齿轮图标 |

> 标签页图标只有单态，选中效果通过 RCSS 实现（opacity / box-shadow / border）。

### 垃圾桶图标（双态）

| 状态 | sprite 名称 | 区域 (x,y,16,16) | 说明 |
|------|-------------|-------------------|------|
| 关闭 | `trash-closed` | **(64, 0, 16, 16)** | 默认状态 |
| 开启 | `trash-open` | **(80, 0, 16, 16)** | 选中/悬浮时 |

### 信封图标（双态，备用）

| 状态 | sprite 名称 | 区域 (x,y,16,16) | 说明 |
|------|-------------|-------------------|------|
| 普通 | `mail` | **(96, 0, 16, 16)** | 信件图标 |
| 新消息 | `mail-new` | **(112, 0, 16, 16)** | 带感叹号，提示有新信息 |

---

## 4. 角色头像 — `Portrait/Premade/1.png` (320×192)

5 列 × 3 行 = 15 个 64×64 表情。

```
  1.png (320×192) — 64×64 grid

         col0     col1     col2     col3     col4
  row0  (0,0)   (64,0)  (128,0) (192,0) (256,0)
  row1  (0,64)  (64,64) (128,64)(192,64) (256,64)
  row2  (0,128) (64,128)(128,128)(192,128)(256,128)
```

### 选用方案

| 用途 | sprite 名称 | 区域 (x,y,w,h) | 说明 |
|------|-------------|----------------|------|
| 默认头像 | `portrait-default` | **(0, 0, 64, 64)** | 第一帧，正面标准表情 |

---

## 5. 资源路径汇总（供 RCSS @spritesheet 使用）

| spritesheet 名称 | 文件路径 (相对 ui/rmlui/menu/) | 说明 |
|------------------|-------------------------------|------|
| `ui-menu-inventory` | `../../../assets/farm-rpg/UI/Inventory/inventory.png` | slot + 面板 |
| `ui-menu-armor` | `../../../assets/farm-rpg/UI/Inventory/Slot Armor.png` | 装备槽占位图标 |
| `ui-menu-hud` | `../../../assets/farm-rpg/UI/HUD.png` | 标签页 + 垃圾桶 |
| `ui-menu-portrait` | `../../../assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png` | 角色头像 |

---

## 6. 所有问题已确认 ✓

- [x] Slot 变体 → 选用 A (col 0)
- [x] Slot Armor 部位映射 → 浅色行 col 0-6: hat/shirt/pants/gloves/boots/necklace/ring
- [x] 项链图标 → col 5 (80, 32)
- [x] Equipment 标签 → HUD.png (96, 16, 16, 16)
- [x] 垃圾桶 → 关闭 (64, 0)，开启 (80, 0)
- [x] 浅色行用作空槽占位 → 是
- [x] Tab Inventory → (48, 32, 16, 16) 背包图标
