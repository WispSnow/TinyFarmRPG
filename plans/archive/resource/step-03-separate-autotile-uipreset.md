# 步骤 3: 分离 AutoTileLibrary 与 UIPresetManager

- 对应上层计划：`./resource-refactor.md`

## 思路

本步骤目标是让 `ResourceManager` 只负责“可加载资源缓存”（texture/audio/font + asset registry），把**配置/规则型模块**彻底移出：

- `AutoTileLibrary`：规则表，不是资源缓存
- `UIPresetManager`：UI 预设配置，不是资源缓存

按 `for_agent/design-guide.md` 的要求，本步骤采用**最优方案**，不做兼容层：

- 不保留 `ResourceManager` 的转发 API
- 不加 `[[deprecated]]` 过渡接口
- 一次性迁移全部调用点到 `Context` 直连访问

### 新的职责边界

- `GameApp` 拥有：
  - `std::unique_ptr<engine::resource::AutoTileLibrary>`
  - `std::unique_ptr<engine::ui::UIPresetManager>`
- `Context` 提供直接访问：
  - `context.getAutoTileLibrary()`
  - `context.getUIPresetManager()`
- `ResourceManager` 保留：
  - 纹理/音频/字体加载与缓存
  - `AssetRegistry`
  - 调试信息（texture/font/audio）

### 启动加载策略

`UIPresetManager` 从 `assets/data/resource_mapping.json` 读取预设文件路径并加载（button/image 预设），该流程从 `ResourceManager::loadResources()` 中移出，改为 `GameApp` 负责。

这样可以保证：

- 启动时预设已可用（首帧 UI 不受影响）
- `ResourceManager` 不再依赖 `UIPresetManager`

## 需要新增的文件

无（本步骤以重构现有文件为主）。

## 实现步骤

### 3.1 调整 GameApp 所有权与初始化顺序

- 在 `GameApp` 新增成员：
  - `auto_tile_library_`
  - `ui_preset_manager_`
- 新增初始化函数：
  - `initAutoTileLibrary()`
  - `initUIPresetManager()`
- **成员声明顺序约束**：`auto_tile_library_` / `ui_preset_manager_` 必须声明在 `context_` 之前（建议紧跟在 `resource_manager_` 后），确保析构顺序安全：
  - 先析构 `context_`（不再持有对两者的引用）
  - 后析构 `ui_preset_manager_` / `auto_tile_library_`
- 在 `init()` 中确保两者在场景创建前完成初始化与预设加载。

涉及文件：

- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`

### 3.2 扩展 Context（新增直连访问接口）

- `Context` 构造参数与成员新增：
  - `engine::resource::AutoTileLibrary&`
  - `engine::ui::UIPresetManager&`
- 提供 getter：
  - `getAutoTileLibrary()`
  - `getUIPresetManager()`
- 更新 `Context::create(...)` 调用链。

涉及文件：

- `src/engine/core/context.h`
- `src/engine/core/context.cpp`
- `src/engine/core/game_app.cpp`（`initContext()` 参数对齐）

### 3.3 精简 ResourceManager（删除非缓存职责）

- 删除成员：
  - `auto_tile_library_`
  - `ui_preset_manager_`
- 删除接口：
  - `getAutoTileLibrary()`
  - `getUIPresetManager()`
  - `loadUIButtonPresets()`
  - `loadUIImagePresets()`
  - `getAutoTileDebugInfo()`
- `clear()` 不再清理 AutoTile/UIPreset 数据，并删除当前对应逻辑（`clearRules/clearButtonPresets/clearImagePresets`）。
- 生命周期约定：`AutoTileLibrary` / `UIPresetManager` 由 `GameApp` 持有，按**应用生命周期**管理，不随场景切换清空；若未来需要运行时重载，新增显式重载入口（不复用 `ResourceManager::clear()`）。
- `loadResources()` 只处理 texture/sound/music/font + `AssetRegistry`，去掉 ui preset 字段加载。

涉及文件：

- `src/engine/resource/resource_manager.h`
- `src/engine/resource/resource_manager.cpp`

### 3.4 迁移业务调用点到 Context 直连

将所有 `context.getResourceManager().getAutoTileLibrary()` / `getUIPresetManager()` 替换为 Context 直连：

- `LevelLoader`：构造时直接拿 `context.getAutoTileLibrary()`
- `GameRuntimeAssembler`：Factory/System 组装与资产收集改为 `context.getAutoTileLibrary()` / `context.getUIPresetManager()`
- `SaveService`：读档建图块规则改为 `context.getAutoTileLibrary()`
- UI 调用链（`UIButton`、`UIManager`、各 game ui）改为 `context.getUIPresetManager()`
- tools 调用链同步迁移：
  - `tools/visual_tester/visual_test_cases.cpp`（AutoTileLibrary）
  - `tools/ui_tester/ui_test_scene.cpp`（UIPresetManager，多处）
- `GameRuntimeAssembler` 明确迁移 3 处：
  - `initFactory()` 中 AutoTileLibrary 获取
  - `assembleServices()` 中 UIPresetManager 获取（资产收集）
  - `assembleSystems()` 中 AutoTileLibrary 获取
- 测试特殊处理（不走 GameApp/Context）：
  - `tests/game/ui_layout_integration_test.cpp` 当前直接创建 `ResourceManager`
  - 迁移后改为测试内独立创建 `UIPresetManager`（或测试夹具持有），并通过测试场景/构造参数注入使用，不再依赖 `ResourceManager::getUIPresetManager()`

涉及文件（按当前调用点）：

- `src/engine/loader/level_loader.cpp`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/game/save/save_service.cpp`
- `src/engine/ui/ui_button.cpp`
- `src/engine/ui/ui_manager.cpp`
- `src/game/ui/inventory_ui.cpp`
- `src/game/ui/hotbar_ui.cpp`
- `src/game/ui/dialogue_bubble.cpp`
- `src/game/ui/time_clock_ui.cpp`
- `src/game/ui/item_tooltip_ui.cpp`
- `tools/visual_tester/visual_test_cases.cpp`
- `tools/ui_tester/ui_test_scene.cpp`
- `tests/game/ui_layout_integration_test.cpp`

### 3.5 调试面板依赖重构

- `UIPresetDebugPanel` 改为直接依赖 `UIPresetManager&`，不再依赖 `ResourceManager&`
- `ResMgrDebugPanel` 中 AutoTile 调试数据改为直接依赖 `AutoTileLibrary&`（或拆分为独立面板，二选一；优先前者以最小改动落地）
- 更新 `GameApp::registerDebugPanels()` 的构造参数。

涉及文件：

- `src/engine/debug/panels/ui_preset_debug_panel.h`
- `src/engine/debug/panels/ui_preset_debug_panel.cpp`
- `src/engine/debug/panels/res_mgr_debug_panel.h`
- `src/engine/debug/panels/res_mgr_debug_panel.cpp`
- `src/engine/core/game_app.cpp`

### 3.6 迁移 UI preset 加载入口（明确读取来源）

- `ResourceManager::loadResources()` 删除对 `ui_button_presets` / `ui_image_presets` 字段的处理。
- 在 `GameApp::initUIPresetManager()` 中解析 `assets/data/resource_mapping.json`：
  - 读取并解析 JSON（沿用项目“无异常”模式：`parse(..., false)`）
  - 提取 `ui_button_presets` / `ui_image_presets`（支持 string 或 array）
  - 对每个路径调用 `UIPresetManager::loadButtonPresets` / `loadImagePresets`
- 保持配置源不变：UI preset 路径仍唯一来源于 `resource_mapping.json`，避免硬编码重复配置。

### 3.7 编译与回归验证

- 全量编译通过
- 全量测试通过
- 启动验证：
  - title 场景 UI 预设可正常显示
  - auto tile 规则功能正常
  - debug 面板正常展示 preset/auto tile 信息

## 待办

- [x] 3.1 在 `GameApp` 增加 `AutoTileLibrary/UIPresetManager` 所有权与初始化流程
- [x] 3.2 在 `Context` 增加 `getAutoTileLibrary()` / `getUIPresetManager()` 访问接口
- [x] 3.3 从 `ResourceManager` 删除 AutoTile/UIPreset 相关成员与 API，收敛 `loadResources()` 职责
- [x] 3.4 迁移 `LevelLoader/GameRuntimeAssembler/SaveService/UI` 调用点到 Context 直连
- [x] 3.5 重构 `UIPresetDebugPanel/ResMgrDebugPanel` 依赖注入
- [x] 3.6 将 UI preset 加载逻辑从 `ResourceManager::loadResources()` 迁移到 `GameApp::initUIPresetManager()`（读取 `resource_mapping.json`）
- [x] 3.7 运行 `cmake --build build -j 8` 与 `ctest --test-dir build --output-on-failure`
- [x] 3.8 手动启动验证：UI 预设、AutoTile、Debug 面板行为正常

## 需要澄清

暂无。
