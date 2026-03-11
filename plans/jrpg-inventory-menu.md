# JRPG 风格物品栏菜单重构计划

## Context

当前物品栏为星露谷风格（5×4 网格、拖拽操作、浮动小窗 + 底部快捷栏），需重构为经典 JRPG 菜单风格（列表式、键盘/手柄驱动、分类标签页、操作子菜单）。

现有架构分层清晰（DomainService → System → UI），改动主要集中在 UI 层，底层数据流和领域逻辑可复用。

### 目标风格特征

- 列表式物品展示（每行 = 图标 + 名称 + 数量），非网格
- 分类标签页（道具 / 装备 / 消耗品 / 重要物品）
- 键盘/手柄方向键导航，Enter 确认，无拖拽
- 选中物品时底部或侧边固定显示描述
- 选中后弹出操作子菜单（使用 / 装备 / 丢弃）
- 大面板覆盖（半屏或全屏），非小浮窗

### 涉及文件

| 层级 | 文件 | 改动程度 |
|------|------|----------|
| 数据组件 | `src/game/component/inventory_component.h` | 可能调整（去除分页，改为分类过滤） |
| 数据组件 | `src/game/component/hotbar_component.h` | 待定（是否保留快捷栏） |
| 领域服务 | `src/game/domain/inventory_domain_service.h/cpp` | 基本不动 |
| ECS 系统 | `src/game/system/inventory_system.h` | 小幅扩展（排序/过滤命令） |
| ECS 系统 | `src/game/system/hotbar_system.h` | 待定 |
| UI 控制器 | `src/game/ui/inventory_ui.h/cpp` | **重写** |
| UI 控制器 | `src/game/ui/hotbar_ui.h/cpp` | 待定 |
| UI 控制器 | `src/game/ui/item_tooltip_ui.h/cpp` | 改为内嵌描述面板 |
| UI 标记 | `ui/rmlui/hud/inventory.rml/rcss` | **重写** |
| UI 标记 | `ui/rmlui/hud/hotbar.rml/rcss` | 待定 |
| UI 标记 | `ui/rmlui/hud/item_tooltip.rml/rcss` | 可能合并到菜单中 |
| 数据定义 | `src/game/defs/commands.h` | 新增命令 |
| 数据定义 | `src/game/defs/events.h` | 可能新增事件 |
| 物品目录 | `src/game/data/item_catalog.h` | 小幅扩展（分类查询） |
| 物品配置 | `assets/data/item_config.json` | 可能扩展分类 |

---

## Phase 1: 设计与数据层准备

- [ ] 确定菜单布局线框图（分类列表 + 物品列表 + 描述区的具体位置与尺寸）
- [ ] 评估 ItemCategory 是否需要扩展（KeyItem、Equipment 等新分类）
- [ ] 决定 Hotbar 的去留（JRPG 通常无快捷栏，但农场工具切换可能仍需要）
- [ ] 确定物品存储模型调整方案（去除固定分页，改为分类动态过滤 or 保持底层 slot 不变仅改 UI 展示）
- [ ] 确定排序规则（按分类 → 按名称 or 按获取顺序）

## Phase 2: UI 层重写

- [ ] 新建 JRPG 风格菜单 RML/RCSS（列表布局、分类标签、描述面板）
- [ ] 重写 InventoryUI C++ 控制器（网格 ViewModel → 列表 ViewModel，分类过滤逻辑）
- [ ] 实现键盘/手柄导航（`tab-index: auto` + `nav-up/down`，方向键浏览物品列表）
- [ ] 实现操作子菜单（选中物品后弹出"使用 / 装备 / 丢弃"选项）
- [ ] 将 tooltip 改为菜单内嵌描述面板（选中即显示，无需悬浮）
- [ ] 菜单开关集成到输入系统（Menu 键打开/关闭）

## Phase 3: 交互逻辑调整

- [ ] 移除拖拽相关逻辑（drag & drop handler、drag proxy 元素）
- [ ] 实现物品自动排序（按分类/名称排列，无需手动调整位置）
- [ ] 新增排序/过滤 Command 并接入 InventorySystem
- [ ] 调整 Hotbar 交互方式（若保留，改为菜单内"设置快捷栏"操作）
- [ ] 整体测试：键盘导航流畅性、物品增删同步、分类切换、边界情况
