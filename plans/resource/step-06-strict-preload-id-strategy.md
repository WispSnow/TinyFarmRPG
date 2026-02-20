# 步骤 6: 切换到严格预加载，统一资源 ID 策略

- 对应上层计划：`plans/resource-refactor.md`

## 思路

本步骤目标是把资源访问从“可懒加载”切到“严格预加载”：

- `get*()` 只做缓存读取，不再触发加载
- 所有运行时资源访问统一走 `id`，移除 `hashed_string` 与 `get(id, path)` 这类双语义入口
- `Sprite` 去掉 `texture_path_`，运行时渲染只依赖 `texture_id_`
- `TextRenderer` 去掉 `font_path` 透传，字体访问只依赖 `(font_id, font_size)`

按 `for_agent/design-guide.md`，本步骤直接收敛到最终方案，不保留兼容层。

## 关键设计

1. 严格预加载契约

- `ResourceManager::getTexture/getSound/getMusic/getFont` 未命中缓存时直接失败并记录错误（Debug 必报错）。
- `AssetRegistry` 在本步骤只负责“索引与预加载输入”，不再作为 `get*()` 的兜底路径来源。
- 失败策略明确化：加载失败后，缓存中不保留无效条目；`AssetRegistry` 注册项保留（用于诊断与后续重试）。

2. 统一 API 形态（ID-only 读取）

- 保留：`load*(id, path)`（显式加载入口）。
- 收敛：`get*(id)`（只读缓存，不带 path）。
- 删除：`load*(hashed_string)`、`get*(hashed_string)`、`get*(id, path)` 以及对应注释/测试。
- `getTextureSize` 同步改为 `getTextureSize(id)`。

3. 预加载执行点前移

- 需要新增“注册后批量预加载”入口，保证 UI/蓝图/地图链路在首次使用前已加载。
- 预加载分层：
- GameApp 启动期：`resource_mapping.json` + UI preset 资源（必须在 TitleScene 创建前完成）。
- GameRuntimeAssembler 装配期：blueprint/item/world 扫描资源。
- LevelLoader 地图期：当前地图 tileset/imagelayer 纹理在建实体前显式 load。

4. UIPreset 运行时数据模型改为 ID-only

- `UIButtonSkin::sound_events`：`std::unordered_map<event_id, std::string(path)>` 改为 `std::unordered_map<event_id, entt::id_type(sound_id)>`。
- `UIButtonLabelStyle`：`font_path` 改为 `font_id`（运行时不再携带字体路径）。
- `UIPresetManager` 解析 JSON 时完成 path->id 映射：
- sounds: path hash 为 `sound_id`
- label.font_path: path hash 为 `font_id`
- image.path: path hash 为 `texture_id`
- JSON 仍允许 path 字段作为输入格式，但运行时结构统一为 ID。

5. Sprite 与 UI 图片访问统一为 ID-only

- `engine::component::Sprite` 删除 `texture_path_` 字段和 path 构造语义。
- `Renderer`、`UIInteractive`、`UIManager`、`UIButton`、`LevelLoader` 访问纹理统一为 `getTexture(id)` / `getTextureSize(id)`。
- `engine::render::Image` / `UIImage` / nine-slice 渲染链路同步迁移为 ID-only 调用（必要时可保留 path 作为调试元数据，但不参与 `ResourceManager` 访问）。

6. TextRenderer 字体数据流改为 ID-only

- `TextRenderer::buildLayout/getTextSize` 移除 `font_path` 参数。
- `UILabel` 构造与 `setFont*` 接口改为直接接收/设置 `font_id`（不再从 path 派生）。
- `UIButton`、`DialogueBubble`、`ItemTooltipUI`、`TimeClockUI`、`UIDragPreview` 等调用链统一传 `font_id`。
- 本步骤不处理 Font unload 事件时序问题（该项在步骤 7 收敛）。

7. 音频调用链同步收敛

- `AudioPlayer` 改为只接受 `id`（移除 path/hash 重载），避免绕过严格预加载。
- `UIInteractive::setSoundEvent(event_id, path)` 路径版删除，统一为 `setSoundEvent(event_id, sound_id)`。

## 执行顺序（调整后）

为保证每个子步骤完成后都可编译，执行顺序调整为：

1. `6.1`（预加载基础设施）
2. `6.2`（接入启动期与装配期预加载时序）
3. `6.3`（Sprite + UI 图片访问迁移到 ID-only）
4. `6.4`（字体调用链迁移到 ID-only）
5. `6.5`（音频调用链迁移到 ID-only）
6. `6.6`（删除旧 API）
7. `6.7`（测试验证）

## 需要新增的文件

| 文件 | 说明 |
|------|------|
| `tests/engine/resource/resource_manager_strict_preload_api_test.cpp` | 编译期与运行时语义测试：验证旧 overload 已移除、`get*` 不再隐式加载 |

> 需要同步更新（无新增）：`src/engine/resource/*.h/.cpp`、`src/engine/render/*`、`src/engine/ui/*`、`src/engine/audio/*`、`src/engine/loader/*`、`src/game/*`、`tests/CMakeLists.txt`、现有资源与音频 API 测试。

## 实现步骤

### 6.1 为严格预加载补齐可枚举与批量加载能力

- `AssetRegistry` 增加可枚举接口（`forEachTexture/Font/Sound/Music` 或等价快照 API）。
- `ResourceManager` 增加批量预加载入口（按 registry 枚举调用 `load*`）。
- 预加载失败保持“无效条目不留缓存垃圾”的现有策略（延续步骤 4/5 约束）。
- 明确语义：加载失败不清理 `AssetRegistry` 对应注册项。

### 6.2 打通启动期与装配期的预加载时序

- `GameApp`：在 UI preset 读取完成后，将 preset 资源注册到 `AssetRegistry` 并执行预加载，确保 Title 场景可直接渲染与播音。
- `GameRuntimeAssembler`：在 `collectBlueprintAssets/collectItemCatalogAssets/collectWorldMapAssets` 后执行一次批量预加载。
- `LevelLoader`：保证当前地图纹理在创建实体前显式 `loadTexture(id, path)`；移除对 `getTexture(..., path)` 语义的依赖。

### 6.3 删除 `Sprite::texture_path_`，并补齐 UI 图片链路迁移

- 修改 `sprite_component.h` 与构造函数。
- 迁移文件（Sprite 主链）：`renderer.cpp`、`animation_system.cpp`、`level_loader.cpp`、`basic_entity_builder.cpp`、`game/loader/entity_builder.cpp`、`entity_factory.cpp`、`crop_system.cpp`、`map_snapshot_serializer.cpp`、`save_service.cpp`。
- 迁移文件（UI 图片链）：`renderer.cpp`、`image.h/.cpp`（若保留 path 则仅改访问语义）、`ui_image.cpp`、`ui_button.cpp`、`ui_interactive.cpp`、`ui_manager.cpp`、`title_scene.cpp`（排查 `getTextureSize` 调用）。
- 保持蓝图层 `SpriteBlueprint/AnimationBlueprint` 的 path 字段，用于注册与数据可读性。

### 6.4 TextRenderer/UILabel/UIButton 改为 ID-only 字体读取

- `TextRenderer::buildLayout/getTextSize` 移除 `font_path` 参数。
- `ResourceManager::getFont` 改为 `(id, pixel_size)`。
- `UILabel`：构造与 `setFontPath` 重构为 `font_id` 语义（接口名按实现可重命名为 `setFontId`）。
- `UIButtonLabelStyle` 由 `font_path` 改为 `font_id`，`UIButton` 不再从 path 二次 hash。
- `UILabel/UIButton/ItemTooltip/DialogueBubble/TimeClockUI/UIDragPreview` 等调用点去掉 `font_path` 透传，仅传 `font_id/font_size`。
- 调整日志文案，删除“先调用 getTextSize(..., font_path) 预加载”的旧提示。

### 6.5 AudioPlayer 与 UI 音效 API 收敛（含 UIPreset sounds 转换）

- `AudioPlayer` 删除 path/hash 重载，仅保留 `playSound(id)` / `playSound2D(id, ...)` / `playMusic(id, ...)`。
- `UIButtonSkin::sound_events` 改为 `(event_id, sound_id)`，`UIPresetManager` 在加载 JSON 时完成 path->id。
- `UIInteractive` 删除路径版 `setSoundEvent`，统一为事件到 `sound_id` 的映射。
- 更新相关测试与调用点（`audio_player_test.cpp` 等）为“先 preload，再按 id 播放”。

### 6.6 收敛并删除旧 API（消费方完成迁移后执行）

- `TextureManager/AudioManager/FontManager` 删除 `hashed_string` load 重载。
- `ResourceManager` 删除 `get*(id, path)`、所有 `hashed_string` load/get 重载，以及 `getTextureSize` 的 path/hash 版本。
- `get*()` 改为“只查缓存，不加载”，未命中直接返回空句柄/nullptr。

### 6.7 测试与回归验证

- 更新 `resource_manager_texture_handle_api_test.cpp`、`resource_manager_audio_handle_api_test.cpp` 为新签名断言。
- 新增 `resource_manager_strict_preload_api_test.cpp`：编译期断言旧 overload 不可调用，运行时断言 `get*` 未命中不触发加载且返回空结果。
- 构建与测试：`cmake --build build -j 8`；`ctest --test-dir build --output-on-failure`。
- 手动回归：Title 场景背景/Logo/UI 按钮；UI hover/click 音效与 BGM；Game 场景地图纹理、角色动画、作物阶段贴图。

## 待办

- [ ] 6.1 为 AssetRegistry 增加可枚举能力，并在 ResourceManager 实现批量预加载
- [ ] 6.2 在 GameApp / GameRuntimeAssembler / LevelLoader 接入严格预加载时序（含 Title 场景前的 UIPreset 预加载）
- [ ] 6.3 删除 `Sprite::texture_path_` 并完成 Sprite + UI 图片访问迁移到 ID-only
- [ ] 6.4 移除 TextRenderer 的 `font_path` 参数，并完成 UIFont 数据流迁移（UIPreset/UILabel/UIButton/UI调用点）
- [ ] 6.5 收敛 AudioPlayer 与 UI 音效 API 到 ID-only，并完成 UIPreset sounds path->id
- [ ] 6.6 删除 ResourceManager/子管理器的 path-hash 与 `get(id,path)` 入口
- [ ] 6.7 完成编译、自动化测试与关键场景手动回归

## 需要澄清

暂无。
