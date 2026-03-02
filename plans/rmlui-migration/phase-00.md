### Phase 0: RmlUi 应用层基础设施

**目标**：为后续迁移建立可复用的基础设施。

**完成状态（2026-03-02）**：

- [x] Step 0.1: 多文档管理 + 场景归属
- [~] Step 0.2: Data Model 辅助层（已提供 `create/markDirty/markAllDirty` 基础能力；`bindScalar/bindArray/bindStruct` 模板工具待补齐）
- [x] Step 0.3: Event Bridge（`data-command` 事件桥接基础能力已落地）
- [x] Step 0.4: RCSS 基础主题
- [x] Step 0.5: Scene 集成接口（Scene 便捷方法与 clean 自动卸载已完成；当前通过 `Context -> GLRenderer -> RmlUILayer` 访问，未单独在 Context 增加 `RmlUILayer*` 字段）
- [x] Step 0.6: ScreenFade 抽象接口

#### Step 0.1: 多文档管理 + 场景归属

**修改** `src/engine/ui/rmlui/rml_ui_layer.h/cpp`

- 新增 `loadDocument(path, owner_scene_name)` 返回 `Rml::ElementDocument*`（不再替换唯一文档）
- 新增 `unloadDocument(Rml::ElementDocument*)`
- 新增 `unloadDocumentsByOwner(scene_name)` — 场景 `clean()` 时批量卸载
- 新增 `showDocument(doc)` / `hideDocument(doc)` 便捷方法
- 新增 `setActiveScene(scene_name)` — SceneManager 在 push/pop/replace 时调用：
  - 活跃场景的文档：恢复事件响应（`pointer-events: auto`）
  - 非活跃场景的文档：禁止事件响应（`pointer-events: none`），但保持可见（支持全栈渲染）
- 内部维护 `struct DocumentEntry { Rml::ElementDocument* doc; std::string owner; }` 列表
- 移除 `current_document_` 单文档限制

**修改** `src/engine/scene/scene_manager.cpp`

- 在 `pushScene()` / `popScene()` / `replaceScene()` 执行后，调用 `rmlui_layer->setActiveScene(top_scene->getName())`
- 确保场景 `clean()` 前先调用 `rmlui_layer->unloadDocumentsByOwner(scene_name)`

#### Step 0.2: Data Model 辅助层

**新建** `src/engine/ui/rmlui/rml_data_bridge.h/cpp`

- 封装 `Rml::DataModelConstructor` 使用模式
- 提供模板工具：`bindScalar<T>(name, getter, setter)` / `bindArray(name, ...)` / `bindStruct(...)`
- 提供 `markDirty(variable_name)` 便捷方法
- 为常见模式（物品列表、标量值显示）提供辅助函数

#### Step 0.3: Event Bridge

**新建** `src/engine/ui/rmlui/rml_event_bridge.h/cpp`

- `RmlEventBridge` 持有 `entt::dispatcher&` 引用
- 注册为 `Rml::EventListener`，解析 RML 事件参数并分发对应的游戏命令
- 支持通过 RML 属性配置事件映射（例如 `data-command="use_item"` `data-slot="3"`）

#### Step 0.4: RCSS 基础主题

**新建** `ui/rmlui/theme/`

- `base.rcss`：全局字体、颜色变量、通用 class（`.panel`、`.button`、`.label`、`.slot`）
- `spritesheet.rcss`：统一的 sprite sheet 定义（引用现有 UI 图集）
- `animation.rcss`：通用动画/过渡定义（fade、slide 等）

#### Step 0.5: Scene 集成接口

在 `engine::core::Context` 中注册 `RmlUILayer*` 引用，供 Scene 直接访问。Scene 基类新增便捷方法：

- `loadRmlDocument(path)` → 自动以 `scene_name_` 为 owner 调用 `rmlui_layer->loadDocument(path, scene_name_)`
- Scene::clean() 中自动调用 `rmlui_layer->unloadDocumentsByOwner(scene_name_)`

#### Step 0.6: ScreenFade 抽象接口

**新建** `src/engine/ui/screen_fade_interface.h`

- 纯接口 `IScreenFade`，声明 `Phase` 枚举 + `fadeOut()` / `fadeIn()` / `phase()` 方法
- `MapTransitionSystem` 改为持有 `IScreenFade*`（此步仅改接口引用，旧 `UIScreenFade` 实现 `IScreenFade` 作为过渡）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 修改 | `src/engine/ui/rmlui/rml_ui_layer.h/cpp` |
| 新建 | `src/engine/ui/rmlui/rml_data_bridge.h/cpp` |
| 新建 | `src/engine/ui/rmlui/rml_event_bridge.h/cpp` |
| 新建 | `src/engine/ui/screen_fade_interface.h` |
| 新建 | `ui/rmlui/theme/base.rcss` |
| 新建 | `ui/rmlui/theme/spritesheet.rcss` |
| 新建 | `ui/rmlui/theme/animation.rcss` |
| 修改 | `src/engine/core/context.h/cpp`（注册 RmlUILayer） |
| 修改 | `src/engine/scene/scene.h/cpp`（RML 便捷方法 + clean 自动卸载） |
| 修改 | `src/engine/scene/scene_manager.cpp`（场景切换时通知 setActiveScene） |
| 修改 | `src/game/system/map_transition_system.h/cpp`（UIScreenFade* → IScreenFade*） |

**验证**：
- 加载多个 RML 文档并行显示 + 隐藏
- data binding 驱动简单文本标签更新
- 场景 push/pop 后，仅栈顶场景的 RML 文档可交互（被覆盖场景文档不响应输入）
- 场景 clean 后其文档自动卸载
- MapTransitionSystem 通过 IScreenFade 接口仍正常工作（旧实现兼容）

---

