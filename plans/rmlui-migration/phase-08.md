### Phase 8: 清理 — 移除旧 UI 框架

**目标**：移除所有不再使用的 `engine::ui` 代码和相关资源。

> **前置约束**：Phase 1-7 的每个阶段已将对应的旧测试替换为新测试（见下方 Step 8.1）。此处仅做最终清扫。

#### Step 8.1: 测试迁移（贯穿 Phase 1-7，此处汇总要求）

采用 **旧删 ↔ 新补 成对推进** 策略：每个 Phase 删除旧 UI 代码时，同步删除对应旧测试并补建 RmlUi 等价测试。

| 旧测试 | 删除时机 | 对应新测试 |
|--------|----------|-----------|
| `engine/ui/ui_button_factory_api_test.cpp` | Phase 8 | N/A（框架移除，无需替代） |
| `engine/ui/ui_preset_manager_*_test.cpp`（4 个） | Phase 8 | N/A（PresetManager 移除） |
| `engine/ui/ui_interaction_state_source_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_interaction_runtime_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_layout_source_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_layout_invalidation_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_world_anchor_test.cpp` | Phase 4 | 对话气泡世界锚定测试（RML 版） |
| `engine/ui/ui_stack_layout_test.cpp` | Phase 8 | N/A |
| `engine/ui/ui_grid_layout_test.cpp` | Phase 8 | N/A |
| `game/ui_layout_integration_test.cpp` | Phase 7 | inventory/hotbar RML 集成测试 |
| `game/pause_menu_scene_async_save_ui_test.cpp` | Phase 3 | pause menu RML 版异步保存测试 |
| `engine/render/rmlui_pipeline_stage_test.cpp` | **保留** | 更新以匹配新渲染顺序 |
| `engine/input/input_manager_rmlui_routing_test.cpp` | **保留** | 不变 |

**原则**：
- 框架类测试（布局/交互/preset）在 Phase 8 统一删除——因为被测代码已不存在，无需替代
- 行为类测试（世界锚定/集成/场景交互）在对应迁移阶段 **先补新测再删旧测**
- RmlUi 管线和输入路由测试保留并更新

#### Step 8.2: 清理工具链依赖

**删除** `tools/ui_tester/` 整个目录

**修改** `tools/visual_tester/visual_test_cases.cpp`：
- 移除 `RenderPassCoverageVisualTest` 中的 `drawUIFilledRect()` / `drawUIImage()` 调用
- 该测试用例的 UI pass 覆盖部分改为注释标记或直接移除（visual_tester 的核心价值在场景/光照/特效通道验证，UI pass 验证已由 RmlUi 管线测试覆盖）

#### Step 8.3: 移除 TextRenderer UI 通道

**修改** `src/engine/render/text_renderer.h/cpp`：
- 移除 `use_ui_pass` 参数及相关分支
- 移除 `drawScreenText()` 方法（所有屏幕文字已由 RmlUi 承担）
- 仅保留世界文字渲染路径（scene pass）

#### Step 8.4: 移除 Game UI 残留

- 确认 `src/game/ui/` 目录下所有文件已被删除或替换
- 移除 `DialogueBubbleController` 对旧 View 的依赖（如已在 Phase 4 完成则跳过）

#### Step 8.5: 移除 Engine UI 框架

**删除整个目录**（RmlUi 子目录和 `screen_fade_interface.h` 除外）：

- `src/engine/ui/ui_element.h/cpp`
- `src/engine/ui/ui_interactive.h/cpp`
- `src/engine/ui/ui_panel.h/cpp`
- `src/engine/ui/ui_button.h/cpp`
- `src/engine/ui/ui_label.h/cpp`
- `src/engine/ui/ui_image.h/cpp`
- `src/engine/ui/ui_item_slot.h/cpp`
- `src/engine/ui/ui_progress_bar.h/cpp`
- `src/engine/ui/ui_screen_fade.h/cpp`（旧实现，IScreenFade 接口保留）
- `src/engine/ui/ui_input_blocker.h/cpp`
- `src/engine/ui/ui_draggable_panel.h/cpp`
- `src/engine/ui/ui_drag_preview.h/cpp`
- `src/engine/ui/ui_defaults.h`
- `src/engine/ui/ui_manager.h/cpp`
- `src/engine/ui/ui_preset_manager.h/cpp`
- `src/engine/ui/layout/` 整个目录
- `src/engine/ui/behavior/` 整个目录

#### Step 8.6: 移除 UIPass + drawUI* API

- 删除 `src/engine/render/opengl/ui_pass.h/cpp`
- 从 `GLRenderer` 中移除 `ui_pass_` 成员及所有 `drawUIRect` / `drawUITexture` / `drawUIRectGradient` / `drawUITextureGradient` 方法
- 从 `Renderer` 中移除 `drawUIImage` / `drawUIFilledRect` / `drawUINineSliceInternal` 方法
- 调整 `present()` 渲染顺序：`... → RmlUI → ImGui → SwapWindow`（移除 UIPass flush 步骤）

> **执行前检查**：用 grep 确认无任何文件仍引用 `drawUI*` 方法。Step 8.2/8.3 应已消除 visual_tester 和 TextRenderer 的引用。

#### Step 8.7: 移除 UIPresetManager

- 从 `engine::core::Context` 的 `ResourceServices` 中移除 `UIPresetManager`
- 删除 UI preset 调试面板（`ui_preset_debug_panel.h/cpp`）

#### Step 8.8: 移除 Scene 基类 UIManager 引用

- 从 `engine::scene::Scene` 中移除 `ui_manager_` 成员
- 各场景不再持有 `UIManager`

#### Step 8.9: 清理 CMakeLists + 测试注册

- 从 `src/CMakeLists.txt` 中移除已删除的源文件
- 从 `tests/CMakeLists.txt` 中移除已删除的旧 UI 测试注册（Step 8.1 表中标记删除的项）
- 从 `tools/CMakeLists.txt` 中移除 `ui_tester` 目标

#### Step 8.10: 清理资产引用

- 审查 `assets/data/icon_config.json` 中 `indicator` 分类（cursor/hand 图标），如仅被旧 UIManager 使用则可移除
- 删除 `assets/data/ui_button_presets.json` 和 `assets/data/ui_image_presets.json`（已由 RCSS spritesheet 替代）

**验证矩阵**：
1. 构建通过（无编译错误、无未解析符号）
2. 全部游戏 UI 功能正常（逐一回归测试 Phase 1-7 验证项）
3. ImGui 调试面板正常工作
4. `tools/visual_tester` 构建运行正常（UI 部分已移除）
5. 全部测试通过（新 RmlUi 测试 + 保留的管线/输入路由测试）
6. 无内存泄漏（RML 文档正确卸载）

---

