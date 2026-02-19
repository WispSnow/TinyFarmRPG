# 步骤 5: Audio 迁移到 `entt::resource_cache`

- 对应上层计划：`plans/resource-refactor.md`

## 思路

本步骤聚焦 Audio 缓存迁移：把当前 `AudioManager` 的
`unordered_map<id, AudioResourceEntry>`（sound/music 各一份）替换为 EnTT 资源缓存体系。

目标：

- 使用 `entt::resource_cache<AudioBuffer, AudioLoader>` 统一 Sound/Music 缓存模型
- 对外改为句柄语义（`entt::resource<const AudioBuffer>`），与 Texture 步骤保持一致
- 保持步骤 2 的 `AssetRegistry` 回退语义不变（严格预加载切换放在步骤 6）

按 `for_agent/design-guide.md`，本步骤不保留旧缓存实现兼容层，直接切换到新实现。

## 关键设计

1. 音频资源类型与 Loader 独立化

- 将 `AudioBuffer` 从 `AudioManager` 内嵌类型提升到 `audio_loader.h`（独立资源类型）。
- 新增 `AudioLoader`，`result_type = std::shared_ptr<AudioBuffer>`。
- 将当前 `AudioManager::decodeAudio()` 解码逻辑迁移到 `AudioLoader::operator()(std::string_view file_path)`。
- 失败语义约束：`AudioLoader` 在空路径/解码失败/零帧等场景返回空 `shared_ptr`，不返回“非空但逻辑空”的缓冲对象。
- 句柄统一：
  - 内部缓存句柄：`entt::resource<AudioBuffer>`
  - 对外只暴露只读句柄：`entt::resource<const AudioBuffer>`
- `ResourceManager` 与 `AudioPlayer` 不再依赖 `AudioManager::AudioBufferHandle`，改为使用 `audio_loader.h` 中的统一句柄别名。

2. AudioManager 改为 cache 编排层

- `sound_cache_`：`entt::resource_cache<AudioBuffer, AudioLoader>`
- `music_cache_`：`entt::resource_cache<AudioBuffer, AudioLoader>`
- `load/find/unload/clear` 统一走 cache API（`load`/`operator[]`/`erase`/`clear`）。
- 与 Texture 步骤一致处理 EnTT 行为：`load()` 后若句柄无效，立即 `erase(id)`，避免残留空条目。
- `findSound/findMusic` 保持 `const`：由于目标句柄就是 `resource<const AudioBuffer>`，可直接使用 `resource_cache::operator[] const`，不需要像 Texture 一样去掉 `const`。

3. 生命周期策略

- Audio 句柄允许跨帧长期持有（`AudioPlayer` 播放期间持有是合法场景）。
- 与 Texture 不同，不做 `clear()` 阶段的 `use_count` 断言；clear 后资源是否立即释放由外部句柄引用计数决定。

4. `miniaudio` 实现单元约束（重要）

- 当前工程由 `audio_player.cpp` 持有 `MINIAUDIO_IMPLEMENTATION`（单一实现单元）。
- 新增 `audio_loader.cpp` 仅 `#include <miniaudio.h>` 并调用解码 API，**不得**再次定义 `MINIAUDIO_IMPLEMENTATION`。
- 该约束需要在文件注释中明确，避免后续误加宏导致重复定义/链接冲突。

5. 调试信息职责分层

- `AudioManager` 的调试采集仅输出基础统计：`channels/sample_rate/frame_count/sample_count/duration`。
- `source` 与 `memory_bytes` 由 `ResourceManager` 在 `getSoundDebugInfo()/getMusicDebugInfo()` 阶段统一回填：
  - `source` 来自 `AssetRegistry`
  - `memory_bytes = sample_count * sizeof(float)`
- 避免 cache 与 metadata 双写同步问题（与步骤 4 的 Texture 策略一致）。

6. 调用方迁移策略

- `ResourceManager` 的 Sound/Music 接口统一返回音频句柄类型（保留现有函数名）。
- `AudioPlayer` 内部持有和传递类型切换为新句柄类型，播放行为（2D、淡入淡出、loop、音量）不改语义。

## 需要新增的文件

| 文件 | 说明 |
|------|------|
| `src/engine/resource/audio_loader.h` | `AudioBuffer` 类型定义、音频句柄类型别名、`AudioLoader`、`SoundCache/MusicCache` 类型别名 |
| `src/engine/resource/audio_loader.cpp` | `AudioLoader` 实现（基于 miniaudio 解码；不定义 `MINIAUDIO_IMPLEMENTATION`） |
| `tests/engine/resource/resource_manager_audio_handle_api_test.cpp` | 编译期 + 运行时语义测试（句柄返回类型、失败加载无残留） |

> 需要同步更新：`src/CMakeLists.txt`、`tests/CMakeLists.txt`。

## 实现步骤

### 5.1 引入 `AudioLoader` 与音频句柄类型

- 新增 `audio_loader.h/.cpp`。
- 将 `AudioBuffer` 从 `AudioManager` 移到 `audio_loader.h`，作为 Resource 层公共音频资源类型。
- 抽离并迁移 `decodeAudio` 解码逻辑到 `AudioLoader::operator()`。
- 定义音频句柄与 cache 别名，作为 Resource 层统一类型入口。
- `audio_loader.cpp` 仅引用 `miniaudio` 声明，不新增实现宏（沿用 `audio_player.cpp` 的单实现单元）。

### 5.2 用 `entt::resource_cache` 重写 `AudioManager`

- 删除 `sounds_` / `music_` 两个 `unordered_map` 缓存。
- 替换为 `sound_cache_` / `music_cache_`。
- 重写：
  - `loadSound/loadMusic`：`cache.load(id, path)`，无效句柄立即 `erase`
  - `findSound/findMusic`：`cache[id]`
  - `unloadSound/unloadMusic`：`cache.erase(id)`
  - `clearSounds/clearMusic/clearAudio`：`cache.clear()`
- 将 `collectAudioDebugInfo` 辅助函数从“`unordered_map` 参数签名”改为基于 `AudioCache`（或等价模板 helper）的签名。

### 5.3 调整 `ResourceManager` 的 Audio API

- `resource_manager.h` 改为显式包含 `audio_loader.h`（用于句柄类型），不再通过 `AudioManager::AudioBufferHandle` 暴露 API。
- `loadSound/getSound/loadMusic/getMusic` 的返回类型切换为新的音频句柄类型。
- 保持当前阶段 `AssetRegistry` fallback 语义（`file_path` 为空时查 registry）。
- `unload/clear` 逻辑保持语义不变。

### 5.4 更新 `AudioPlayer` 消费方式

- 将 `AudioPlayer::Impl` 内部 `AudioBufferHandle` 切换为新句柄类型。
- `ManagedSound` 持有句柄以保证播放期间资源生命周期。
- 保持现有播放控制行为不变，仅改资源获取/持有语义。

### 5.5 调试接口与统计修正

- `AudioManager::collectSoundDebugInfo/collectMusicDebugInfo` 改为从 cache 读取并计算统计值。
- `ResourceManager::getSoundDebugInfo/getMusicDebugInfo` 回填 `source` 与 `memory_bytes`，并保持排序输出。

### 5.6 测试补充

- 新增 `resource_manager_audio_handle_api_test.cpp`：
  - 编译期 `static_assert`：Sound/Music 的 load/get 返回句柄类型
  - 失败路径语义：不存在文件时返回空句柄，debug info 无残留条目
- 回归现有 `audio_player_test.cpp`，确保行为测试不退化。

### 5.7 回归验证

- 编译：`cmake --build build -j 8`
- 测试：`ctest --test-dir build --output-on-failure`
- 手动验证：
  - 音效播放正常（普通/2D）
  - 音乐播放与淡入淡出正常
  - Debug 面板音频统计（id/source/duration/memory）正常

## 待办

- [ ] 5.1 新增 `AudioLoader` 并迁移解码逻辑
- [ ] 5.1A 提升 `AudioBuffer` 到 `audio_loader.h` 并完成句柄类型统一
- [ ] 5.1B 明确并落实 `miniaudio` 单实现单元约束（`audio_player.cpp` 独占实现宏）
- [ ] 5.2 用 `entt::resource_cache` 重写 `AudioManager` Sound/Music 缓存
- [ ] 5.2A 重构 `collectAudioDebugInfo` helper 签名以匹配新 cache 类型
- [ ] 5.3 切换 `ResourceManager` Audio API 到句柄类型
- [ ] 5.4 迁移 `AudioPlayer` 到句柄语义并保持行为不变
- [ ] 5.5 修正音频调试信息采集与 `AssetRegistry` 回填
- [ ] 5.6 新增并接入 Audio 句柄 API 与失败路径测试
- [ ] 5.7 执行全量构建、测试与手动回归

## 需要澄清

暂无。
