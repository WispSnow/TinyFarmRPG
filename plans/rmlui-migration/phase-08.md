### Phase 8: 补齐残留迁移 — 摆脱旧 UI 运行时依赖

**目标**：先把仍然活跃的旧 UI 运行时依赖迁走，让 Phase 9 可以只做“删除死代码”。本阶段的重点不是大扫除，而是消除 `GameScene`、Rml wrapper、启动链和测试对旧 `engine::ui` 的最后耦合。

> **前置事实**：Phase 1-7 已完成主要界面迁移，但当前代码仍存在以下残留：
> - `GameScene` 仍在创建 `UIManager`，并把 tooltip / dialogue bubble / menu button 挂在旧 UI 树上
> - `DialogueBubbleView` / `ItemTooltipUI` 仍继承 `UIElement`
> - `HotbarUI` / `InventoryUI` 仍通过 `ui_item_slot.h` 取得 `SlotItem`
> - 启动链、`Context`、`GameRuntimeAssembler` 仍依赖 `UIPresetManager`

**本 Phase 完成后应满足**：

- 运行时场景不再创建 `UIManager`
- `src/game/ui/` 中现存 wrapper 不再 include 将在 Phase 9 删除的旧 UI 框架头文件
- `Context` / `GameApp` / `GameRuntimeAssembler` 不再依赖 `UIPresetManager`
- Phase 9 开始前，旧 UI 框架已经是“无运行时调用的死代码”

#### Step 8.1: 先补测试，再拆残留依赖

本阶段要先把仍然绑定旧框架实现细节的测试改成面向 RmlUi wrapper / 运行时行为的测试。

| 当前测试 | 问题 | 本 Phase 要做的修正 |
|----------|------|--------------------|
| `tests/engine/ui/ui_world_anchor_test.cpp` | 当前直接验证 `UIElement + UIManager` 的世界锚点插值 | 改为新的世界锚点 helper / wrapper 级测试 |
| `tests/game/dialogue_bubble_controller_test.cpp` | 当前仍创建 `UIManager`，并查找 `UILabel` 子节点 | 改为直接验证 `DialogueBubbleView` 的 RML 文档状态与锚点数据 |
| `tests/engine/scene/entry_to_first_frame_safety_test.cpp` | 当前 grep `if (ui_manager_)` | 改为验证 `Scene` 不再依赖 `ui_manager_` |
| `tests/game/ui_layout_integration_test.cpp` | 当前已是 RmlUi 文档级测试，但 fixture 仍受 `ResourceServices` / `UIPresetManager` 变更影响 | 保持文档级测试定位，仅收敛 fixture 与共享 helper 依赖 |

**原则**：

- 先补新测，再移除对旧类型的断言
- 本 Phase 不删除整批旧框架测试；Phase 9 再删除真正失去对象的测试

#### Step 8.2: 提炼仍需保留的共享类型

在删除旧框架前，先把仍被 Rml wrapper 使用的中性数据结构迁出。

**本 Step 明确包括**：

- 将 `SlotItem` 从 `src/engine/ui/ui_item_slot.h` 提炼到独立共享头文件
- 将 `Thickness` 从 `src/engine/ui/ui_element.h` 提炼到独立共享头文件
- 审查 `DEFAULT_UI_FONT_ID` / `DEFAULT_UI_FONT_SIZE_PX` 等默认 UI 字体常量
  - 若 wrapper 仍依赖，则迁到中性头文件
  - 若只是局部实现细节，则就地私有化，不再依附旧 UI 框架

**建议形态**：

- `src/engine/ui/ui_types.h` 或等价的中性共享头
- 不允许新共享头反向 include 将在 Phase 9 删除的旧框架头文件

**结果要求**：

- `HotbarUI` / `InventoryUI` / `DialogueBubbleView` / `ItemTooltipUI` 不再 include 将在 Phase 9 删除的旧头文件

#### Step 8.3: DialogueBubbleView 脱离 UIElement / UIManager

**修改** `src/game/ui/dialogue_bubble_view.h/cpp`

- `DialogueBubbleView` 改为纯 RmlUi wrapper，不再继承 `UIElement`
- 提取轻量世界锚点插值 helper，负责：
  - 保存 `world_anchor / previous_world_anchor / screen_offset`
  - 基于相机与 `interpolation_alpha` 计算屏幕坐标
- `DialogueBubbleController` 的对外契约保持不变：
  - `registerBubble()`
  - `setWorldAnchor()` / `clearWorldAnchor()` 语义不变
- `GameScene::render()` 或专用 helper 负责在渲染前刷新对话气泡文档位置

> 目标是保留“世界锚定插值”行为，但不再借助 `UIManager::render()` 和 `UIElement` 布局树。

**时机要求**：

- 世界锚点屏幕位置必须在 `GameScene::render()` 中刷新
- 刷新时机应位于“相机插值位置已应用”之后
- 并且发生在本帧 `GLRenderer::present()` 调用 `RmlUILayer::update()/render()` 之前
- 不允许把文档位置更新延后到 `RmlUi` 已开始渲染之后

#### Step 8.4: ItemTooltipUI 脱离 UIElement / UIManager

**修改** `src/game/ui/item_tooltip_ui.h/cpp`

- `ItemTooltipUI` 改为纯 wrapper，不再继承 `UIElement`
- 鼠标跟随与边缘钳制逻辑保留，但改为：
  - 自身保存位置与尺寸状态
  - 由 `GameScene::update()` 或显式 `update()` 调用驱动
- 保留现有 API：
  - `showItem()`
  - `hideTooltip()`
  - `setPadding()`
  - `setOffset()`

**要求**：

- Tooltip 行为保持与 Phase 4 一致
- 不再依赖旧布局树的 `setPosition()` / `getRequestedSize()` / `update()`

#### Step 8.5: 迁移 GameScene 残留 HUD 宿主

> **前置**：Step 8.2-8.4 已完成；即 `DialogueBubbleView` / `ItemTooltipUI` 已能脱离旧 UI 树独立存活。

**修改** `src/game/scene/game_scene.h/cpp`

- 删除 `GameScene` 对 `UIManager` 的创建和持有
- 为游戏内固定 HUD 控件提供纯 RmlUi 宿主方案，推荐新增独立文档：
  - `ui/rmlui/hud/game_overlay.rml`
  - `ui/rmlui/hud/game_overlay.rcss`
- 将当前旧 `menu` 按钮改为 RmlUi 按钮 + 事件桥接，不再使用 `UIButton`
- `GameScene::update()` / `render()` / `clean()` 改为直接驱动和析构各个 wrapper，而不是依赖旧 UI 树

**析构顺序要求**：

- 所有持有 `Rml::ElementDocument*` 的 wrapper 必须在 `Scene::clean()` 调用 `unloadAllRmlDocuments()` 之前释放
- 重点包括：
  - `DialogueBubbleView`
  - `ItemTooltipUI`
  - `InventoryUI`
  - `HotbarUI`
  - 其他未来新增的文档持有者
- 若仍保留 `owner_scene_id` 批量卸载机制，也必须避免 wrapper 析构阶段再次对已卸载文档做重复 `unload`

**边界**：

- `TimeClockHud` / `InventoryUI` / `HotbarUI` 继续保留类名与现有公共接口
- 本阶段不删除旧按钮实现文件；只是不再让运行时代码依赖它

#### Step 8.6: 清除运行时对 UIPresetManager 的依赖

本阶段要完成“先替换、后删除”。

**修改范围**：

- `src/engine/core/game_app.cpp`
- `src/engine/core/context.h`
- `src/engine/core/context_modules.h`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/engine/debug/panels/ui_preset_debug_panel.*`

**迁移要求**：

- 启动期不再通过 `ui_button_presets.json` / `ui_image_presets.json` 初始化运行时 UI 资源
- 菜单按钮、鼠标指针等剩余运行时资源改为：
  - RmlUi spritesheet / RCSS
  - 或 `resource_mapping.json` / `AssetRegistry` 的直接注册
- `Context::ResourceServices` 中移除 `UIPresetManager`
- 删除 UI preset debug panel
- 同步修改所有构造 `ResourceServices` / `Context` 的测试 fixture，去除 `UIPresetManager` 字段与构造参数
  - 包括当前游戏测试 fixture
  - 也包括 Phase 9 才会删除的 legacy `engine/ui` 测试，只要它们在本 Phase 仍参与编译，就必须先适配新签名
  - 执行时用 grep 校验所有 `ResourceServices resource_services{...}` 调用点都已收敛

> `UIPresetManager` 的物理文件删除放到 Phase 9；本阶段先保证它不再是运行时依赖。

#### Step 8.7: Scene 基类去除 UIManager 假设

**修改** `src/engine/scene/scene.h/cpp`

- 移除 `Scene::ui_manager_`
- `Scene::update()` / `Scene::render()` 不再隐式更新或渲染旧 UI 树
- 保留 `loadRmlDocument()` / `unloadAllRmlDocuments()` 作为场景级 Rml 文档 owner 辅助

**同步修改**：

- 更新受影响的场景测试
- 当前预期 `GameScene` 是唯一仍在持有 `UIManager` 的运行时场景；执行时仍需用 grep 复核，防止遗漏其他派生类或测试专用场景

#### Step 8.8: 本阶段不做的事

以下内容明确留给 Phase 9：

- 删除 `src/engine/ui/` 旧框架文件
- 删除 `UIPass`
- 删除 `drawUI*` / `drawUIText`
- 删除 `tools/ui_tester`
- 删除 `ui_button_presets.json` / `ui_image_presets.json`

**验证矩阵**：
1. `GameScene` 运行时不再创建 `UIManager`
2. 对话气泡、Tooltip、Inventory、Hotbar、菜单按钮行为保持正常
3. `Context` / `GameApp` / `GameRuntimeAssembler` 不再依赖 `UIPresetManager`
4. 相关测试通过，且不再断言旧 `UIManager` / `UILabel` / `UIElement` 实现细节
5. 通过 grep 确认游戏运行时代码已无 `UIManager` / `UIButton` / `UIPresetManager` 活跃调用

---
