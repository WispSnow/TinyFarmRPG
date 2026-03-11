# JRPG 风格物品栏菜单重构计划

## Context

当前物品栏为星露谷风格（5×4 网格、拖拽操作、浮动小窗 + 底部快捷栏），
需重构为 Stardew Valley 图标网格风格的全屏菜单（居中大面板、10×4 背包网格、
底部角色信息与装备槽、标签页切换、键盘/手柄驱动）。

详细线框图见 `jrpg-inventory-menu-wireframe.md`。

### 目标风格特征

- 图标网格展示（10×4 = 40 slot，16×16 图标 + 数量）
- 顶部标签页切换（背包/装备/任务/地图/设置，初期只实现背包）
- 底部角色信息区（64×64 头像 + 名称等级 + 8 装备槽 + 金币）
- 全屏半透明遮罩 + 居中大面板（440×310dp）
- 键盘/手柄方向键导航，Enter 确认，无拖拽
- 选中 slot 时浮动 Tooltip 显示物品详情
- 操作子菜单（使用/装备/丢弃）
- **独立场景** — InventoryMenuScene push 到场景栈，游戏暂停

### 架构决策

**物品菜单 = 独立场景 (InventoryMenuScene)**

遵循现有 PauseMenuScene / RestDialogScene / BattleScene 的模式：
- Push 到场景栈顶，GameScene 冻结在底层继续渲染
- `State::Paused` 暂停游戏逻辑
- `InputContext::Menu` 切换输入映射
- `instance_id_` 隔离 RML 文档输入焦点
- `popScene()` 关闭菜单，恢复 Playing 状态

**不再使用 Hotbar**

移除 HotbarUI / HotbarComponent / HotbarSystem。
工具切换方式待后续设计（可能改为菜单内快速装备或方向键快捷切换）。

### 涉及文件

| 层级 | 文件 | 改动程度 |
|------|------|----------|
| **新增场景** | `src/game/scene/inventory_menu_scene.h/cpp` | **新建** |
| UI 标记 | `ui/rmlui/menu/inventory_menu.rml/rcss` | **新建** |
| UI 控制器 | `src/game/ui/inventory_menu_ui.h/cpp` | **新建**（替代旧 InventoryUI） |
| 游戏场景 | `src/game/scene/game_scene.h/cpp` | 修改（inventory 键触发 pushScene） |
| 数据组件 | `src/game/component/inventory_component.h` | 调整（去除分页逻辑） |
| 领域服务 | `src/game/domain/inventory_domain_service.h/cpp` | 基本不动 |
| ECS 系统 | `src/game/system/inventory_system.h` | 小幅扩展（排序命令） |
| Tooltip | `src/game/ui/item_tooltip_ui.h/cpp` | 复用或改造 |
| 数据定义 | `src/game/defs/commands.h` | 新增 OpenInventoryMenu 等命令 |
| 物品目录 | `src/game/data/item_catalog.h` | 小幅扩展（分类查询） |
| 构建配置 | `src/CMakeLists.txt` | 添加新文件 |
| **移除** | `src/game/ui/hotbar_ui.h/cpp` | 废弃 |
| **移除** | `src/game/component/hotbar_component.h` | 废弃 |
| **移除** | `src/game/system/hotbar_system.h` | 废弃 |
| **移除** | `ui/rmlui/hud/hotbar.rml/rcss` | 废弃 |
| **移除** | `ui/rmlui/hud/inventory.rml/rcss` | 废弃（被新文件替代） |

---

## Phase 1: 设计与数据层准备

- [x] 确定菜单布局线框图 → `jrpg-inventory-menu-wireframe.md`
- [x] 确定 Hotbar 去留 → 移除
- [x] 确定菜单架构 → 独立场景 InventoryMenuScene
- [ ] 评估 ItemCategory 是否需要扩展（Equipment、KeyItem 等新分类）
- [ ] 确定物品存储模型调整方案（去除 InventoryComponent 的分页，改为平铺 40 slot）
- [ ] 确定排序规则（按分类 → 按名称 or 按获取顺序）

## Phase 2: 场景与 UI 层实现

- [ ] 新建 InventoryMenuScene（继承 Scene，push/pop 生命周期）
  - init: State::Paused + InputContext::Menu + 加载 RML
  - clean: 恢复状态 + popContext + 卸载 RML
- [ ] GameScene 中 inventory 键改为 pushScene(InventoryMenuScene)
- [ ] 新建 inventory_menu.rml/rcss（440×310 面板、10×4 网格、标签页、角色信息区）
- [ ] 新建 InventoryMenuUI C++ 控制器（RmlDataBridge 绑定 40 slot ViewModel）
- [ ] 实现键盘/手柄导航（tab-index + nav-up/down/left/right 在 10×4 网格中）
- [ ] 实现 Tooltip（选中 slot 时显示物品名称/分类/描述）
- [ ] 实现操作子菜单（选中物品后弹出 Use/Equip/Discard）
- [ ] 标签页 UI（初期只有 Inventory 页可用，其余灰色占位）

## Phase 3: 交互逻辑与清理

- [ ] 移除旧 InventoryUI + HotbarUI + 相关 RML/RCSS
- [ ] 移除 HotbarComponent / HotbarSystem 及相关 Command/Event
- [ ] 清理 GameScene 中旧 inventory/hotbar 相关代码
- [ ] 调整 InventoryComponent 去除分页（active_page_ 不再需要）
- [ ] 实现物品自动排序 Command
- [ ] 装备槽 UI 占位（8 格锁定状态，后续实装装备系统时激活）
- [ ] 更新 CMakeLists.txt（添加新文件、移除旧文件）
- [ ] 整体测试：场景 push/pop、键盘导航、物品同步、Tooltip、边界情况
