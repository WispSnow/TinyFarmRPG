# 步骤 2: 构建完整资产索引

- 对应上层计划：`./resource-refactor.md`

## 思路

当前 `resource_mapping.json` 仅覆盖 2 张 title 纹理、11 音效、2 音乐、0 字体。其余所有资源（tileset 纹理、blueprint 纹理、UI preset 纹理、icon 纹理、字体等）完全依赖 `get(id, path)` 的 path 回退懒加载。

为后续切换到严格预加载模式，需要建立一个 **AssetRegistry**——统一的资产注册表，在资源被使用前确保所有 `id → path` 映射已知。

### 核心设计

引入 `AssetRegistry` 类，职责单一：维护 `id → path` 的映射表。

- 它**不负责加载资源**（加载仍由各 Manager 执行）
- 它归 ResourceManager 所有（内部 `unique_ptr`，在 `create()` 中构造）
- ResourceManager 在 `load` / `get` 时可从 AssetRegistry 查询 path，不再需要调用方传入

```
┌─────────────────────────┐
│     AssetRegistry       │  id → path 映射表
│  registerTexture(id,p)  │
│  registerFont(id,sz,p)  │
│  findTexturePath(id)    │
│  findFontPath(id,sz)    │
└────────┬────────────────┘
         │ 查询
         ▼
┌─────────────────────────┐
│    ResourceManager      │  缓存 + 加载（调用各 SubManager）
│  getTexture(id)         │  miss 时从 registry 查 path → load
│  getFont(id, sz)        │
└─────────────────────────┘
```

### 注册方式：集中式，不侵入数据层

**关键设计决策：** BlueprintManager、ItemCatalog、UIPresetManager 等数据层类**不引入 AssetRegistry 依赖**。它们的构造函数和 load 方法保持不变（保护现有测试）。

注册工作统一在 **GameRuntimeAssembler 装配层** 完成——各数据模块加载完成后，由 assembler 遍历其已解析数据，提取 `id → path` 映射并注册到 registry。

```
GameRuntimeAssembler
  │
  ├─ ensureBlueprintManager()  → blueprint_manager->load(...)
  │     ↓ load 完成后
  │  collectBlueprintAssets(blueprint_manager, registry)  ← 新增：遍历蓝图提取纹理路径
  │
  ├─ ensureItemCatalog()  → item_catalog->load(...)
  │     ↓ load 完成后
  │  collectItemCatalogAssets(item_catalog, registry)  ← 新增
  │
  └─ ResourceManager::loadResources()  → 解析 resource_mapping.json
       ↓ 内部同步注册
```

### Tileset 纹理的确定性索引

当前 tileset 纹理注册依赖地图加载链路（`LevelLoader::preloadLevelData()`），但 `map_loading_config.json` 默认 `preload: "off"`，仅 `"all"` 模式才全量预加载。这意味着 registry 覆盖取决于玩家访问路径，不是确定性的。

解决方案：在 `GameRuntimeAssembler` 的 `initMapManager()` 阶段，加载 `.world` 文件获取所有地图路径后，**扫描所有 `.tmj` 引用的 `.tsj` 并提取其纹理路径**注册到 registry。这是轻量操作（只读 JSON、不解码纹理），与 preload 模式无关。

同时，`LevelLoader::preloadLevelData()` 中现有的 `loadTexture()` 调用不变——它负责**实际加载**纹理到 GPU，registry 只负责索引。

### 音频 registry 设计

音频分两张独立映射表（`sound_paths_` / `music_paths_`），与 AudioManager 的 `sounds_` / `music_` 对应，避免同 ID 不同语义的冲突。

### UI preset 中 sounds 字段的约定

当前 `ui_button_presets.json` 中 sounds 使用的是**文件路径**（如 `"assets/audio/UI_button11.wav"`），不是语义 key。本步骤保持现状，后续 strict mode 步骤中再统一考虑是否改用语义 key。

### 与现有代码的兼容策略

本步骤**不修改**现有 `get(id, path)` 的 path 回退机制，path 回退暂时保留作为安全网。变更限定为：
1. 新增 `AssetRegistry` 类
2. ResourceManager 内部持有并集成 registry
3. 在装配层（GameRuntimeAssembler）追加注册调用
4. LevelLoader 中追加注册调用（已有 ResourceManager 引用，无新依赖）

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/engine/resource/asset_registry.h` | AssetRegistry 类声明 |
| `src/engine/resource/asset_registry.cpp` | AssetRegistry 实现 |

注意：需将新 `.cpp` 文件加入 `src/CMakeLists.txt` 的 `target_sources()`。

## 实现步骤

### 2.1 实现 AssetRegistry 类

- 内部维护四张映射表：
  - `texture_paths_`: `unordered_map<entt::id_type, std::string>` — 纹理 ID → 文件路径
  - `font_paths_`: `unordered_map<FontKey, std::string, FontKeyHash>` — (字体ID, 像素大小) → 文件路径
  - `sound_paths_`: `unordered_map<entt::id_type, std::string>` — 音效 ID → 文件路径
  - `music_paths_`: `unordered_map<entt::id_type, std::string>` — 音乐 ID → 文件路径
- 提供注册接口：`registerTexture(id, path)`, `registerFont(id, pixel_size, path)`, `registerSound(id, path)`, `registerMusic(id, path)`
- 提供查询接口：`findTexturePath(id) -> std::string_view`（返回空 = 未注册）, 同理 `findFontPath`, `findSoundPath`, `findMusicPath`
- Debug 模式下：注册时检查同一 ID 是否映射到不同路径，若冲突则 `spdlog::warn` 报警
- 将新文件加入 `src/CMakeLists.txt`

### 2.2 集成到 ResourceManager

- ResourceManager 新增 `unique_ptr<AssetRegistry>` 成员，在 `create()` 中构造
- 对外增加 `AssetRegistry& getAssetRegistry()` 访问接口
- `loadResources()` 解析 `resource_mapping.json` 时，同步向 registry 注册每条资源
- 各 `getXxx(id, path)` 方法中：缓存未命中时，若 `path` 为空则先查 registry 获取 path，再执行加载

### 2.3 注册引擎硬编码资源和默认字体

- 在 `loadResources()` 末尾显式注册：
  - 光照 circle 纹理：`registerTexture(entt::hashed_string("assets/textures/UI/circle.png"), "assets/textures/UI/circle.png")`
  - 默认字体：`registerFont(entt::hashed_string(DEFAULT_UI_FONT_PATH.data()), DEFAULT_UI_FONT_SIZE_PX, DEFAULT_UI_FONT_PATH)`

### 2.4 LevelLoader 注册 tileset / imagelayer 纹理

- `preloadLevelData()` 中已有 `loadTexture()` 调用处，追加 `registry.registerTexture(id, path)`
- `parseSingleImageSprite()` / `parseMultiImageSprite()` / WangSet 处同理
- LevelLoader 已持有 `ResourceManager&` 引用，通过 `resource_manager_.getAssetRegistry()` 访问，无需新增构造参数

### 2.5 GameRuntimeAssembler 集中注册数据层资源

新增三个静态/自由函数（放在 `game_runtime_assembler.cpp` 的匿名命名空间中）：

**`collectBlueprintAssets(BlueprintManager&, AssetRegistry&)`**
- 遍历 `actorBlueprints()` — 提取 `sprite_.path_` 和每个 `AnimationBlueprint::texture_path_` 注册纹理
- 遍历 `animalBlueprints()` — 同理
- 遍历 `cropBlueprints()` — 遍历每个 stage 的 `sprite_.path_` 注册纹理

**`collectItemCatalogAssets(ItemCatalog&, AssetRegistry&)`**
- ItemCatalog 只提供 `findIcon(id)` 和 `listItems()` 接口，需确认是否有遍历 icon 的接口
- 若无，可临时在 ItemCatalog 增加 `forEachIcon(callback)` 方法（轻量变更，不引入 AssetRegistry 依赖）
- 从每个 `Image` 提取 `getTextureId()` 和 `getTexturePath()` 注册纹理

**`collectUIPresetAssets(UIPresetManager&, AssetRegistry&)`**
- UIPresetManager 目前有遍历接口（`button_presets_` / `image_presets_` 是 private）
- 若无公有遍历接口，在 UIPresetManager 增加 `forEachButtonPreset(callback)` / `forEachImagePreset(callback)` 方法
- 从每个 `UIButtonSkin` 的 Image 字段提取 texture id/path 注册纹理
- 从 `UIButtonSkin::sound_events` 提取音效路径注册音效
- 从 `UIButtonLabelStyle::font_path` 提取字体注册

调用时机：在各 `ensure*()` 完成后、`loadScene()` 之前。

### 2.6 确定性扫描全部 tileset 纹理

在 `GameRuntimeAssembler::initMapManager()` 中，`WorldState::loadFromWorldFile()` 完成后：
- 遍历 world 中所有地图条目，获取 `.tmj` 路径
- 解析每个 `.tmj` 的 `tilesets` 数组，获取 `.tsj` 路径
- 解析每个 `.tsj` 的 `"image"` 字段，resolve 为绝对路径
- 将所有 tileset 纹理的 `(hash(path), path)` 注册到 registry
- 这是只读 JSON 解析，不触发 GPU 纹理加载，轻量且确定性

### 2.7 验证

- 在 debug 模式下，`get(id)` 缓存未命中且 registry 也查不到时，输出 `spdlog::error` 报警
- 运行游戏，确认无新增报警，所有资源均可通过 registry 查到路径
- 运行现有测试，确认 BlueprintManager / ItemCatalog 的 smoke test 和 unit test 全部通过（构造函数未变，无 regression）
- 确认现有 path 回退路径仍然工作

## 待办

- [x] 2.1 实现 AssetRegistry 类（asset_registry.h / .cpp），加入 CMakeLists.txt
- [x] 2.2 集成到 ResourceManager，loadResources() 同步注册
- [x] 2.3 注册引擎硬编码资源和默认字体
- [x] 2.4 LevelLoader 追加 registry 注册调用
- [x] 2.5 GameRuntimeAssembler 集中注册 blueprint/item/UI preset 资源
- [x] 2.6 确定性扫描 world 中全部 tileset 纹理并注册
- [x] 2.7 验证：运行游戏 + 运行测试，无新增报警，现有测试全通过
