### Phase 9: 清理 — 删除旧 UI 框架

**目标**：在 Phase 8 已清除全部运行时依赖后，删除旧 `engine::ui` 框架、`UIPass`、`drawUI*` API、相关测试与工具链残留。

> **前置约束**：Phase 8 已完成，且满足以下条件：
> - `GameScene` 与其他运行时场景不再创建 `UIManager`
> - `DialogueBubbleView` / `ItemTooltipUI` / `InventoryUI` / `HotbarUI` 不再包含将被删除的旧 UI 头文件
> - 启动链、`Context`、`GameRuntimeAssembler` 已不再依赖 `UIPresetManager`
> - 用 grep 确认游戏运行时代码已无 `UIManager` / `UIElement` / `UIButton` / `UIPresetManager` / `drawUI*` / `drawUIText` 真实调用

#### Step 9.1: 测试收尾删除

Phase 9 只删除已经没有运行时价值的旧框架测试。

| 测试 | 动作 |
|------|------|
| `engine/ui/ui_button_factory_api_test.cpp` | 删除 |
| `engine/ui/ui_preset_manager_*_test.cpp`（4 个） | 删除 |
| `engine/ui/ui_interaction_state_source_test.cpp` | 删除 |
| `engine/ui/ui_interaction_runtime_test.cpp` | 删除 |
| `engine/ui/ui_layout_source_test.cpp` | 删除 |
| `engine/ui/ui_layout_invalidation_test.cpp` | 删除 |
| `engine/ui/ui_stack_layout_test.cpp` | 删除 |
| `engine/ui/ui_grid_layout_test.cpp` | 删除 |
| `engine/factory_visibility_test.cpp` | 更新或删除其中 `UIButton` 工厂可见性断言 |
| `engine/render/rmlui_pipeline_stage_test.cpp` | 保留，但改为校验 `OverlayVfx -> RmlUi -> ImGui` |
| `engine/render/gl_renderer_lifecycle_test.cpp` | 保留，但移除对 `ui_pass_` 的生命周期断言 |

#### Step 9.2: 清理工具链依赖

- 删除 `tools/ui_tester/` 整个目录
- 从 `tools/CMakeLists.txt` 中移除 `ui_tester` 目标
- 修改 `tools/visual_tester/visual_test_cases.cpp`
  - 删除旧 UI pass 覆盖代码和对应说明
  - 删除或重写 `UiVisualTest`
  - `TextRenderingVisualTest` 若仍保留，仅验证世界文本路径；不再调用 `drawUIText()` / `drawUIFilledRect()`
- 修改 `engine/debug/panels/gl_renderer_debug_panel.cpp`
  - 去掉 `UIPass` 统计行

#### Step 9.3: 移除 TextRenderer 的 UI 渲染分支

**修改** `src/engine/render/text_renderer.h/cpp`

- 删除 `drawUIText()` 重载
- 删除 `drawTextInternal(..., bool use_ui_pass)` 的 UI 分支，统一保留世界文字路径
- 删除内部对 `drawUITexture()` 的调用

> 若仍需要屏幕空间调试文字，必须改由 RmlUi 文档承载，而不是恢复 `TextRenderer` UI 通道。

#### Step 9.4: 移除 UIPass + drawUI* API

- 删除 `src/engine/render/opengl/ui_pass.h/cpp`
- 从 `GLRenderer` 中移除：
  - `ui_pass_`
  - `PassType::UI`
  - `drawUIRect` / `drawUITexture` / `drawUIRectGradient` / `drawUITextureGradient`
- 从 `Renderer` 中移除：
  - `drawUIImage`
  - `drawUIFilledRect`
  - `drawUINineSliceInternal`
  - `getDefaultUIColorOptions()` / `getDefaultUITransformOptions()` 及其 set 接口
- 调整 `present()` 顺序为：`... -> OverlayVfx -> RmlUi -> ImGui -> SwapWindow`

> **执行前检查**：确认 `tools/visual_tester`、`TextRenderer`、debug panel、测试代码已全部移除 `drawUI*` 依赖。

#### Step 9.5: 删除旧 Engine UI 框架

在确认无任何 include / type 引用后，删除以下旧框架文件：

- `src/engine/ui/ui_element.h/cpp`
- `src/engine/ui/ui_interactive.h/cpp`
- `src/engine/ui/ui_panel.h/cpp`
- `src/engine/ui/ui_button.h/cpp`
- `src/engine/ui/ui_label.h/cpp`
- `src/engine/ui/ui_image.h/cpp`
- `src/engine/ui/ui_item_slot.h/cpp`
- `src/engine/ui/ui_progress_bar.h/cpp`
- `src/engine/ui/ui_screen_fade.h/cpp`
- `src/engine/ui/ui_input_blocker.h/cpp`
- `src/engine/ui/ui_draggable_panel.h/cpp`
- `src/engine/ui/ui_drag_preview.h/cpp`
- `src/engine/ui/ui_defaults.h`
- `src/engine/ui/ui_manager.h/cpp`
- `src/engine/ui/ui_preset_manager.h/cpp`
- `src/engine/ui/layout/` 整个目录
- `src/engine/ui/behavior/` 整个目录

**保留**：

- `src/engine/ui/rmlui/` 整个目录
- `src/engine/ui/screen_fade_interface.h`

#### Step 9.6: 资产与映射清理

- 从 `assets/data/resource_mapping.json` 中移除 `ui_button_presets` / `ui_image_presets`
- 删除：
  - `assets/data/ui_button_presets.json`
  - `assets/data/ui_image_presets.json`
- 审查 `assets/data/icon_config.json`
  - 仅当 `indicator/cursor` 与 `hand_*` 已无任何运行时用途时才删除
  - 若 `ItemCatalog` 仍使用 `indicator/cursor` 作为 fallback icon，则必须保留或先替换 fallback 规则

#### Step 9.7: CMake 与构建注册清理

- 从 `src/CMakeLists.txt` 中移除已删除源文件
- 从 `tests/CMakeLists.txt` 中移除已删除测试注册
- 从 `tools/CMakeLists.txt` 中移除 `ui_tester`

**验证矩阵**：
1. 全量构建通过，无旧 UI 未解析符号
2. `GameScene`、菜单场景、战斗场景、HUD 全部仍正常工作
3. `tools/visual_tester` 构建运行正常，且不再依赖旧 UI pass
4. 保留测试通过，尤其是：
   - RmlUi 管线测试
   - RmlUi 输入路由测试
   - Inventory / Hotbar / Scene 级集成测试
5. `RmlUILayer` 与文档析构顺序正常，无残留文档或泄漏

---
