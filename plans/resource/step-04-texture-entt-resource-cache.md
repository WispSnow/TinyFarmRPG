# 步骤 4: Texture 迁移到 `entt::resource_cache`

- 对应上层计划：`plans/resource-refactor.md`

## 思路

本步骤只处理 Texture 缓存迁移，目标是把当前 `TextureManager` 的手写 `unordered_map<id, unique_ptr<GL_Texture>>` 替换为 EnTT 资源缓存：

- 使用 `entt::resource_cache<GL_Texture, TextureLoader>` 管理纹理生命周期
- 使用 `entt::resource<GL_Texture>` 句柄替代 `GL_Texture*` 返回值
- 保持 `AssetRegistry` 的查找/回退逻辑不变（严格预加载切换留到步骤 6）

按 `for_agent/design-guide.md` 要求，本步骤不保留兼容层：

- `ResourceManager` 的 Texture 接口直接改为句柄类型
- `Renderer` 等调用方一次性迁移到句柄语义
- 不新增“旧指针 API”过渡入口

### 关键设计

1. `TextureLoader` 独立化
- 将 `stb_image` 解码 + OpenGL 纹理创建逻辑从 `TextureManager::loadTexture` 提取到 `TextureLoader::operator()`。
- `TextureLoader::result_type = std::shared_ptr<engine::utils::GL_Texture>`。
- 通过 `shared_ptr` 自定义删除器执行 `glDeleteTextures`，替代当前 `TextureDeleter + unique_ptr`。

2. `TextureManager` 只做缓存编排
- 内部持有 `TextureCache`（`entt::resource_cache`）而不是手写 map。
- `load/find/getSize/unload/clear` 都围绕 cache API（`load`/`operator[]`/`erase`/`clear`）重写。
- 调试信息不再在 `TextureManager` 维护 `source_path/memory_bytes` 副本：
  - `memory_bytes` 由 `width * height * 4` 现场计算
  - `source_path` 在 `ResourceManager::getTextureDebugInfo()` 阶段通过 `AssetRegistry::findTexturePath(id)` 回填
- 这样可避免 cache 与 metadata 双写同步。

3. GL 生命周期约束显式化
- `GameApp::close()` 中，在销毁 `resource_manager_` 前显式 `clear()`，确保纹理释放发生在 GL 上下文仍有效的阶段。
- Debug 模式下在 `TextureManager::clearTextures()` 增加句柄引用检查（`use_count == 1`），发现跨边界持有立即报警/断言。
- 约束：句柄仅用于帧内临时访问，不允许放入长期存活结构（组件/缓存/单例成员）。

4. 步骤边界
- 本步骤不处理 `Sprite::texture_path_` 去除（该项属于步骤 6）。
- 本步骤不处理 Audio/Font 缓存迁移（分别在步骤 5/7）。

## 需要新增的文件

| 文件 | 说明 |
|------|------|
| `src/engine/resource/texture_loader.h` | `TextureLoader` 声明、`TextureHandle/TextureCache` 类型别名 |
| `src/engine/resource/texture_loader.cpp` | `TextureLoader` 实现（stb 解码 + OpenGL 上传 + shared_ptr 删除器） |
| `tests/engine/resource/resource_manager_texture_handle_api_test.cpp` | 编译期 API 测试：`getTexture/loadTexture` 返回句柄类型，防止回退到裸指针 |

注意：需要同步更新 `src/CMakeLists.txt` 与 `tests/CMakeLists.txt`。

## 实现步骤

### 4.1 引入 `TextureLoader` 与句柄类型

- 新增 `texture_loader.h/.cpp`。
- 定义：
  - `using TextureHandle = entt::resource<engine::utils::GL_Texture>;`
  - `using TextureCache = entt::resource_cache<engine::utils::GL_Texture, TextureLoader>;`
- `TextureLoader::operator()(std::string_view file_path)`：
  - 路径校验
  - `stbi_load` 解码
  - `glGenTextures/glTexImage2D` 上传（保留当前 `GL_SRGB8_ALPHA8` 与 unpack alignment）
  - 失败返回空 `shared_ptr`（无异常路径）
- `STB_IMAGE_IMPLEMENTATION` 从 `texture_manager.cpp` 迁移到 `texture_loader.cpp`，保证全工程仅一个实现编译单元。
- 明确约束：`resource_cache` 会接受 loader 返回值并插入条目，即使该返回值为空。因此“返回空 `shared_ptr`”必须与 4.2 的“插入后无效句柄立刻擦除”配套实现。

### 4.2 重写 `TextureManager` 为 cache 驱动

- 删除 `TexturePtr/TextureResource/textures_` 老结构。
- 新成员：
  - `TextureCache texture_cache_`
- 方法调整：
  - `loadTexture(id, path)`：调用 `texture_cache_.load(id, path)`，若返回句柄无效（`!handle`）则立即 `texture_cache_.erase(id)` 并返回空句柄
  - `findTexture(id)`：返回 `texture_cache_[id]`
  - `getTextureSize(id)`：从句柄读 `width/height`
  - `unloadTexture/clearTextures`：仅清理 cache
  - `collectDebugInfo`：遍历 cache 句柄并拼装基础调试信息（`id/texture/width/height`）

### 4.3 调整 `ResourceManager` 的 Texture API

- 在 `resource_manager.h` 暴露 `using TextureHandle = ...`。
- 下列接口改为句柄返回：
  - `loadTexture(id, path)`
  - `loadTexture(hashed_string)`
  - `getTexture(id, path="")`
  - `getTexture(hashed_string)`
- `getTextureSize` 保持现有签名，但内部改为基于 `TextureHandle`。
- 维持当前步骤 2 的 registry fallback 语义（空 path 时查 `AssetRegistry`）。
- 同步调整 `ResourceManager` 内部纹理消费方：
  - `getTextureSize(...)`：基于句柄判空后读取尺寸
  - `getTextureDebugInfo()`：基于 `TextureManager` 的基础调试信息回填 `source` 与 `memory_bytes`

### 4.4 迁移渲染消费方到句柄语义

- 重点文件：`src/engine/render/renderer.cpp` / `src/engine/render/renderer.h`。
- 所有纹理访问改为：
  - `auto handle = resource_manager_->getTexture(...);`
  - `if (!handle) { ... }`
  - 获取 GL 句柄使用 `handle->texture`，获取尺寸/引用使用 `*handle`
- 保证句柄只在函数局部变量中存在，不向成员变量扩散。

### 4.5 生命周期收口与防护

- 在 `GameApp::close()` 中，`resource_manager_.reset()` 前显式调用 `resource_manager_->clear()`。
- 在 `TextureManager::clearTextures()`（Debug）检查句柄引用计数，发现外部持有时输出明确错误信息（包含资源 id/path）。

### 4.6 测试补充（自动化）

- 新增编译期 API 测试：`resource_manager_texture_handle_api_test.cpp`
  - 校验 `loadTexture/getTexture` 返回 `TextureHandle`，防止签名回退到裸指针。
- 新增运行时语义测试（无 GL 上下文依赖）：
  - 使用不存在路径调用 `loadTexture/getTexture`，触发 `TextureLoader` 返回空 `shared_ptr`
  - 断言 `ResourceManager::getTextureDebugInfo()` 中不会残留该 id（验证“无效句柄会被 erase”）
  - 断言 `getTextureSize(id)` 返回零尺寸

### 4.7 回归验证（构建 + 手动）

- 编译：`cmake --build build -j 8`
- 测试：`ctest --test-dir build --output-on-failure`
- 启动验证：
  - 场景与 UI 纹理渲染正常
  - 资源调试面板纹理信息（id/source/size/memory）正常
  - 退出流程无 GL 相关错误日志
  - Debug 构建下验证未出现“clear 阶段外部句柄仍被持有”的告警/断言

## 待办

- [ ] 4.1 新增 `TextureLoader` 并完成 OpenGL 纹理加载逻辑迁移
- [ ] 4.2 用 `entt::resource_cache` 重写 `TextureManager` 缓存实现
- [ ] 4.3 将 `ResourceManager` Texture 接口改为 `TextureHandle`
- [ ] 4.4 迁移 `Renderer` 到句柄访问，清理裸指针使用
- [ ] 4.5 增加 `GameApp::close()` 清理收口与 Debug 引用计数检查
- [ ] 4.6 新增并接入 Texture 句柄编译期测试 + 失败路径运行时语义测试
- [ ] 4.7 执行全量构建、测试与启动回归验证

## 需要澄清

暂无。
