# 资源管理系统重构计划

## 设计决策

以下决策已确认，贯穿所有步骤：

- **严格预加载**：最终目标是所有资源在使用前必须完成注册/加载，不再支持按 path 的懒加载回退
- **ENTT_ID_TYPE 升级到 64 位**：降低 hash 碰撞风险
- **旧 API 在重构完成后删除**：不做长期兼容层，但允许分步骤过渡

## 现状分析

当前 `src/engine/resource/` 采用 Facade 模式，`ResourceManager` 持有 5 个子管理器（TextureManager、AudioManager、FontManager、AutoTileLibrary、UIPresetManager），各自用 `std::unordered_map<entt::id_type, ...>` 做缓存。项目使用了 EnTT 的 `id_type` / `hashed_string` 做资源标识，但**没有**使用 EnTT 自带的 `entt::resource_cache` / `entt::resource` 体系。

### 主要问题

| # | 问题 | 影响 |
|---|------|------|
| 1 | Texture/Font 返回裸指针，无引用计数 | 资源卸载后消费方可能持有悬垂指针（TextRenderer 的 `TextLayout` 缓存 `Font*` 已存在此风险：`unloadFont` 先删后 enqueue 事件，下帧才 dispatch，窗口期内 layout 持有悬垂指针） |
| 2 | 每种资源类型有 4 个 overload（id+path / hashed_string × load / get），Facade 膨胀到 40+ 公有方法 | 维护成本高，语义 key 与 path-hash 容易混淆 |
| 3 | `Sprite` 组件逐实体存储 `std::string texture_path_` | 成千上万 tile 实体复制同一路径字符串，浪费内存 |
| 4 | 32 位 hash 无冲突检测 | 两条不同路径产生相同 hash 会静默覆盖 |
| 5 | 子管理器 `friend class ResourceManager` + 全 private | 无法独立单元测试 |
| 6 | AutoTileLibrary / UIPresetManager 和"资源缓存"职责不同，但塞在同一个 Facade 里 | 单一职责被稀释 |
| 7 | `resource_mapping.json` 覆盖不全 | 仅含 2 张 title 纹理、0 个字体。tileset/blueprint/font 全靠 `get(id, path)` 的 path 回退加载，无法直接切换到严格预加载模式 |

### EnTT resource_cache 迁移可行性评估

| 资源类型 | 可行性 | 说明 |
|----------|--------|------|
| **Texture** | ✅ 高 | 编写 `TextureLoader`（`result_type = shared_ptr<GL_Texture>`），替换 `unique_ptr + TextureDeleter`。消费方拿到 `entt::resource<GL_Texture>` 句柄，生命周期安全。**注意 GL 上下文生命周期**：当前 `game_app.cpp:187-192` 依赖 resource_manager 在 gl_renderer 之前析构来保证 `glDeleteTextures` 时上下文有效；引入 shared_ptr 句柄后，必须确保所有句柄在 `GameApp::close()` 资源销毁边界前释放。 |
| **Audio** | ✅ 高 | 已经使用 `shared_ptr<AudioBuffer>`，天然匹配。分别建 `SoundCache` / `MusicCache` 即可。 |
| **Font** | ⚠️ 中（不在第一阶段迁移） | 当前以 `(id, pixel_size)` 复合键索引，与 `resource_cache` 的单 `id_type` 键不直接兼容。即使升级到 64 位，Font 的 2D 键空间用 pair 更自然。`entt::resource<Font>` 的句柄本身支持可变访问（`operator*` 返回 `Font&`），glyph_cache 按需增长**不是**阻塞点。真正的阻塞点是**键模型适配**和 **TextRenderer 缓存失效策略**。建议后续单独评估。 |
| **AutoTileLibrary** | ❌ 不适合 | 它是一组规则配置表，不是可加载/卸载的"资源"。不应纳入 resource_cache。 |
| **UIPresetManager** | ❌ 不适合 | 它是 UI 配置/预设管理器，与资源缓存职责不同。 |

**结论：** Texture 和 Audio 迁移收益最大、成本最低，应优先实施。Font 不在第一阶段迁移，保留现有实现并统一接口风格。AutoTileLibrary 和 UIPresetManager 应从 ResourceManager 中分离，独立管理。

---

## 重构步骤

### Phase A: 基础设施准备

#### 步骤 1: 升级 ENTT_ID_TYPE 到 64 位 (已完成)

**目标：** 降低资源 ID 的 hash 碰撞风险，为后续统一 ID 策略奠定基础。

**实现思路：**
- 在 CMakeLists.txt 中添加 `ENTT_ID_TYPE=std::uint64_t` 编译定义
- 排查代码中对 `entt::id_type` 做 32 位假设的地方（序列化、printf format、位运算等），逐一修正
- 确保编译通过并运行正常

---

#### 步骤 2: 构建完整资产索引 (已完成)

**目标：** 引入 `AssetRegistry` 类（`id → path` 映射表），使所有运行时所需资源在使用前均已注册。为后续切换到严格预加载模式做前置准备。

**实现思路：**
- 新增 `AssetRegistry` 类，归 ResourceManager 所有（`unique_ptr`），维护纹理/音频/字体的 `id → path` 映射
- 数据层类（BlueprintManager / ItemCatalog / UIPresetManager）**不引入 AssetRegistry 依赖**，构造函数不变
- 注册工作集中在 GameRuntimeAssembler 装配层：各模块 load 完成后，由 assembler 遍历已解析数据提取映射并注册
- tileset 纹理通过解析 `.world → .tmj → .tsj` 链路确定性扫描，不依赖 preload 模式
- 现有 `get(id, path)` 的 path 回退仍保留作为安全网
- 详见 `plans/resource/step-02-asset-registry.md`

---

#### 步骤 3: 分离 AutoTileLibrary 和 UIPresetManager (已完成)

**目标：** 让 ResourceManager 专注于"可加载资源的缓存管理"，将非资源缓存职责移出。

**实现思路：**
- 将 `AutoTileLibrary` 和 `UIPresetManager` 的所有权移到 `GameApp` 层，并在 `Context` 中添加直接访问接口（`context.getAutoTileLibrary()`、`context.getUIPresetManager()`）
- **过渡期**：在 ResourceManager 中保留转发方法（标记 `[[deprecated]]`），内部委托到 Context 持有的实例，避免一次性修改所有调用方
- 已确认的调用方（需最终迁移）：
  - `LevelLoader::LevelLoader()` — 取 AutoTileLibrary 引用
  - `GameRuntimeAssembler` — 构造 EntityFactory 时取 AutoTileLibrary
  - `SaveService` — 序列化时取 AutoTileLibrary
  - `UIButton::getPreset()` — 取 UIPresetManager
  - 其他 UI 组件通过 `context_.getResourceManager().getUIPresetManager()` 的访问链路
- 过渡完成后删除 ResourceManager 中的转发方法

---

### Phase B: 缓存迁移

#### 步骤 4: Texture 迁移到 entt::resource_cache

**目标：** 使用 `entt::resource_cache<GL_Texture, TextureLoader>` 替换手写的 `unordered_map` 缓存，消费方获得 `entt::resource<GL_Texture>` 句柄，消除裸指针悬垂风险。

**实现思路：**
- 实现 `TextureLoader`，其 `result_type = std::shared_ptr<GL_Texture>`，在 `operator()` 中完成 stb_image 解码 + OpenGL 纹理创建（将现有 `TextureManager::loadTexture()` 中的逻辑提取出来）
- 在 `shared_ptr` 的自定义删除器中调用 `glDeleteTextures`，替代当前的 `TextureDeleter`
- `TextureManager` 内部改用 `entt::resource_cache<GL_Texture, TextureLoader>` 作为存储
- 对外返回 `entt::resource<GL_Texture>`（定义 `TextureHandle` 类型别名），替代裸指针 `GL_Texture*`
- **GL 生命周期策略**：
  - `TextureHandle` 仅作为帧内临时引用使用，**不得**存储在跨帧存活的数据结构中（如组件、缓存）
  - `Sprite` 组件仍只存 `texture_id_`，不存句柄；Renderer 在每帧渲染时通过 ID 获取临时句柄
  - `ResourceManager::clear()` / `GameApp::close()` 时，cache 清空即释放所有 GL 纹理（此时无外部句柄持有引用，shared_ptr 引用计数为 1）
  - 在 debug 模式下可添加断言：cache 析构时检查所有 shared_ptr 的 `use_count() == 1`
- 更新 Renderer 等消费方

---

#### 步骤 5: Audio 迁移到 entt::resource_cache

**目标：** 统一 Sound 和 Music 的缓存实现，利用 `entt::resource_cache` 替换两个 `unordered_map`。

**实现思路：**
- 实现 `AudioLoader`，将现有 `AudioManager::decodeAudio()` 封装为 loader 的 `operator()`
- 分别创建 `entt::resource_cache<AudioBuffer, AudioLoader>` 的 `sound_cache_` 和 `music_cache_`
- `AudioBufferHandle`（当前为 `shared_ptr<const AudioBuffer>`）可映射到 `entt::resource<const AudioBuffer>`
- 注意：Audio 的句柄生命周期约束比 Texture 宽松，AudioPlayer 播放期间持有句柄是合理的（无 GL 上下文依赖）
- 更新 AudioPlayer 的消费方式

---

### Phase C: API 整理

#### 步骤 6: 切换到严格预加载，统一资源 ID 策略

**目标：** 在步骤 2 构建的完整资产索引基础上，切换到严格预加载模式——所有资源在使用前必须已加载。移除 path 回退机制，简化 API。

**实现思路：**
- 确认步骤 2 的资产索引已覆盖所有资源（tileset/blueprint/UI/font），所有资源在 `loadResources()` 或场景加载阶段完成预加载
- 移除每个子管理器上的 `hashed_string` overload 和 `get(id, path)` 的 path 回退逻辑，只保留 `load(id, path)` 和 `get(id)`
- `get(id)` 未命中时返回空句柄/nullptr + `spdlog::error`，不再尝试加载
- `Sprite` 组件去掉 `texture_path_` 字段，仅保留 `texture_id_`；更新所有 Sprite 构造点（EntityFactory、LevelLoader 等）
- TextRenderer 类似处理：去掉 `font_path` 参数

---

#### 步骤 7: Font 接口统一（保留现有缓存实现）

**目标：** Font 不迁移到 `entt::resource_cache`，但统一接口风格，修复 TextRenderer 的缓存失效隐患。

**实现思路：**
- 保留 FontManager 的 `unordered_map<FontKey, unique_ptr<Font>>` 实现和 `(id, pixel_size)` 复合键
- 接口风格与 Texture/Audio 对齐：`load(id, pixel_size, path)` + `get(id, pixel_size)`（无 path 回退）
- **修复 TextRenderer 缓存失效问题**：
  - 当前风险：`unloadFont` 先从 map 删除 Font → `TextLayout` 中的 `Font*` 悬垂 → enqueue 事件 → 下帧才 dispatch 通知 TextRenderer 清理
  - 修复方案：改为 trigger 立即分发（而非 enqueue），或在 TextRenderer 中将 `Font*` 改为按 `(font_id, pixel_size)` 重新查找的间接引用
- 移除 `hashed_string` overload

---

#### 步骤 8: 精简 ResourceManager Facade，删除旧 API

**目标：** 在前述步骤完成后，精简 ResourceManager 的公有接口，删除所有过渡期兼容代码。

**实现思路：**
- 删除步骤 3 中 ResourceManager 上标记 `[[deprecated]]` 的 AutoTileLibrary / UIPresetManager 转发方法
- 统一 `load` / `get` / `unload` / `clear` 的命名模式，确保三类资源（Texture/Audio/Font）接口对称
- 解除子管理器的 `friend class ResourceManager` + 全 private 约束，改用 public 接口，方便单元测试
- 评估是否仍需 Facade：如果各 cache 已足够自包含，可考虑让消费方直接从 Context 获取对应的 cache
- 保留或重构调试接口 `getXxxDebugInfo()`

---

### Phase D: 可选

#### 步骤 9（可选）: Debug 模式 hash 冲突检测

**目标：** 在 debug 构建中检测资源 ID 冲突，防止不同路径的 hash 碰撞导致静默覆盖。

**实现思路：**
- 维护一个 `debug_only` 的 `unordered_map<entt::id_type, std::string>`（id -> 原始路径）
- 每次 `load` 时检查是否已存在不同路径映射到相同 id，如果是则 `spdlog::error` 报警
- 仅在 `#ifndef NDEBUG` 下编译，零运行时开销
- 升级到 64 位后碰撞概率极低，此步骤优先级最低
