### Phase 6: 删除兼容壳、清理旧入口、补测试

**目标**：完成最终收口，让新结构成为唯一结构。

#### 本阶段要做的事

1. 执行阶段 B 检查
   - grep `src/` 中的 `getRmlUILayer()`
   - 结果必须为零
   - `tests/` 若保留该字符串，只能用于“应当不存在”的源码断言

2. 删除兼容壳和旧入口
   - 删除 `RmlUILayer`
   - 删除 `GLRenderer::getRmlUILayer()`
   - 删除所有 renderer-backdoor 式访问路径

3. 补测试
   - `rmlui_runtime_access_test.cpp`
   - `game_scene_ui_controller_smoke_test.cpp`

4. 补关键注释
   - runtime 职责
   - backend 职责
   - `initRmlUi()` 初始化顺序
   - render hook 接线意图

#### 涉及文件

- 删除 `src/engine/ui/rmlui/rml_ui_layer.h`
- 删除 `src/engine/ui/rmlui/rml_ui_layer.cpp`
- 修改 `src/engine/render/opengl/gl_renderer.h`
- 修改 `src/engine/render/opengl/gl_renderer.cpp`
- 新建 `tests/engine/ui/rmlui_runtime_access_test.cpp`
- 新建 `tests/game/game_scene_ui_controller_smoke_test.cpp`

#### 验证

- `src/` 中不再存在 `getRmlUILayer()`
- `tests/` 中若保留该字符串，仅用于“应当不存在”的源码断言
- 项目可编译
- 测试可运行
- 文档和代码注释能说明新的职责边界

#### 完成标记

- [x] 阶段 B grep 检查通过：`src/` 中 `getRmlUILayer()` 为零，`tests/` 仅保留否定断言
- [x] 删除 `RmlUILayer`
- [x] 删除 `GLRenderer::getRmlUILayer()`
- [x] 新增 `rmlui_runtime_access_test.cpp`
- [x] 新增 `game_scene_ui_controller_smoke_test.cpp`
