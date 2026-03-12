# JRPG 风格物品栏菜单重构计划

## Context

当前物品栏正在从旧的分页小窗整理为独立的全屏菜单场景。
现阶段代码已经落地为：居中大面板、10×4 背包网格、菜单内 hotbar 区域、
详情区、角色信息区占位、标签页占位，以及键盘/手柄焦点导航 + 鼠标拖拽交互。

详细线框图见 `jrpg-inventory-menu-wireframe.md`。

### 目标风格特征

- 图标网格展示（10×4 = 40 slot，16×16 图标 + 数量）
- 菜单内 hotbar 区域（显示当前快捷栏绑定，支持背包/hotbar 联动）
- 顶部标签页切换（背包/装备/任务/地图/设置，初期只实现背包）
- 详情区（名称 / 分类 / 描述）
- 角色信息区（64×64 头像占位 + 名称 + 8 装备槽占位）
- 全屏半透明遮罩 + 居中大面板（440×310dp）
- 键盘/手柄方向键导航，鼠标拖拽保留
- 选中/悬浮 slot 时浮动 Tooltip 显示物品详情
- 操作子菜单（Phase 3 补完）
- **独立场景** — InventoryMenuScene push 到场景栈，游戏暂停

### 架构决策

**物品菜单 = 独立场景 (InventoryMenuScene)**

遵循现有 PauseMenuScene / RestDialogScene / BattleScene 的模式：
- Push 到场景栈顶，GameScene 冻结在底层继续渲染
- `State::Paused` 暂停游戏逻辑
- `InputContext::Menu` 切换输入映射
- `instance_id_` 隔离 RML 文档输入焦点
- `popScene()` 关闭菜单，恢复 Playing 状态

**保留 Hotbar**

- 探索态 `HotbarUI` 保留
- `HotbarComponent` / `HotbarSystem` 保留
- 新菜单也保留独立的 hotbar 区域，与背包共享同一套运行时数据
- Phase 3 只清理旧 `InventoryUI` 路径，不移除 hotbar 运行时

### 涉及文件

| 层级 | 文件 | 改动程度 |
|------|------|----------|
| **新增场景** | `src/game/scene/inventory_menu_scene.h/cpp` | **新建** |
| UI 标记 | `ui/rmlui/scenes/inventory_menu.rml/rcss` | **新建** |
| 游戏场景 | `src/game/scene/game_scene.h/cpp` | 修改（inventory 键触发 pushScene） |
| 菜单逻辑 | `src/game/scene/inventory_menu_scene.h/cpp` | 持续扩展（Phase 3 补动作子菜单） |
| 数据组件 | `src/game/component/inventory_component.h` | Phase 3 去除分页字段 |
| 领域服务 | `src/game/domain/inventory_domain_service.h/cpp` | 调整（去除分页字段） |
| ECS 系统 | `src/game/system/inventory_system.h/cpp` | 扩展（排序命令） |
| 探索态 Hotbar | `src/game/ui/hotbar_ui.h/cpp` | 保留，去除对旧 InventoryUI 的耦合 |
| Tooltip | `src/game/ui/item_tooltip_ui.h/cpp` | 复用 |
| 数据定义 | `src/game/defs/commands.h` / `src/game/defs/events.h` | 调整（分页移除、排序命令） |
| 存档 | `src/game/save/save_data.h/cpp` / `src/game/save/save_service.cpp` | 调整（移除 active_page） |
| 构建配置 | `src/CMakeLists.txt` | 移除旧 inventory UI 文件，保留 hotbar 文件 |
| **移除** | `src/game/ui/inventory_ui.h/cpp` | Phase 3 删除 |
| **移除** | `ui/rmlui/hud/inventory.rml/rcss` | Phase 3 删除 |

---

## Phase 1: 设计与数据层准备

- [x] 确定菜单布局线框图 → `jrpg-inventory-menu-wireframe.md`
- [x] 确定 Hotbar 去留 → 保留探索态 HotbarUI，并在菜单内保留 hotbar 区域
- [x] 确定菜单架构 → 独立场景 InventoryMenuScene
- [x] 评估 ItemCategory → 暂不引入真实 Equipment 后端，Phase 3 只按当前物品分类补齐交互
- [x] 确定物品存储模型 → 菜单按 40 slot 平铺显示，`active_page_` 清理延后到 Phase 3
- [x] 确定排序规则 → 默认获取顺序（slot index 自然序），后续加可选"按分类→按名称"排序
- [x] 确认素材资源区域 → `jrpg-inventory-menu-sprites.md`（全部已确认）

## Phase 2: 场景与 UI 层实现 → `jrpg-inventory-menu-phase2.md`

- [x] Step 1: InventoryMenuScene 骨架 (h/cpp，仿 PauseMenuScene 模式)
- [x] Step 2: inventory_menu.rml/rcss (面板布局 + spritesheet 定义)
- [x] Step 3: backpack / menu hotbar ViewModel 数据绑定 + syncFromInventory
- [x] Step 4: GameScene 集成 (inventory 键 → pushScene)
- [x] Step 5: 键盘/手柄网格导航 + 选中态同步
- [x] Step 6: Tooltip + detail panel 集成
- [x] Step 7: 菜单内 hotbar 拖拽联动 + 标签页占位
- [x] 更新 CMakeLists.txt + 构建验证

## Phase 3: 交互逻辑与清理 → `jrpg-inventory-menu-phase3.md`

- [ ] 为 `InventoryMenuScene` 增加操作子菜单状态机
- [ ] 接通 backpack slot 的 `Use / Equip(Bind) / Discard / Cancel`
- [ ] 接通 hotbar slot 的 `Activate / Use / Unbind / Cancel`
- [ ] 实装角色信息区数据绑定（名称 + 占位头像/装备槽）
- [ ] 移除旧 `InventoryUI` 与旧 inventory RML/RCSS
- [ ] 清理 `GameScene` 中旧 inventory UI 集成，保留探索态 `HotbarUI`
- [ ] 精简 `HotbarUI` 对旧 `InventoryUI` 的耦合
- [ ] 调整 `InventoryComponent` 去除分页 (`active_page_`)
- [ ] 实现物品自动排序 Command，并同步修复 hotbar 映射
- [ ] 更新 `CMakeLists.txt`（移除旧 inventory UI 文件，保留 hotbar 文件）
- [ ] 整体测试
