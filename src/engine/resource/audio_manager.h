#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <entt/core/fwd.hpp>
#include "resource_debug_info.h"

namespace engine::resource {

/**
 * @brief 管理音频资源的文件映射。
 * 
 * 负责缓存音效、音乐的解码数据。真正的播放逻辑由 AudioPlayer 负责。
 */
class AudioManager final{
    friend class ResourceManager;

public:
    struct AudioBuffer {
        std::vector<float> samples;
        std::uint32_t channels{0};
        std::uint32_t sample_rate{0};
        std::uint64_t frame_count{0};

        [[nodiscard]] bool empty() const noexcept { return samples.empty() || channels == 0 || frame_count == 0; }
    };

    using AudioBufferHandle = std::shared_ptr<const AudioBuffer>;

private:
    // 音频需要持续播放，必须保证播放期间缓存的音频数据不释放，因此使用shared_ptr
    using AudioBufferPtr = std::shared_ptr<AudioBuffer>;

    struct AudioResourceEntry {
        AudioBufferPtr buffer;
        std::string source_path;
    };

    // 音效存储 (资源ID -> 音频数据)
    std::unordered_map<entt::id_type, AudioResourceEntry> sounds_;
    // 音乐存储 (资源ID -> 音频数据)
    std::unordered_map<entt::id_type, AudioResourceEntry> music_;

public:
    /**
     * @brief 默认构造函数。
     */
    AudioManager();

    ~AudioManager();            ///< @brief 析构时会清理缓存。

    // 当前设计中，我们只需要一个AudioManager，所有权不变，所以不需要拷贝、移动相关构造及赋值运算符
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) = delete;
    AudioManager& operator=(AudioManager&&) = delete;

private:  // 仅供 ResourceManager 访问的方法
    [[nodiscard]] AudioBufferPtr decodeAudio(std::string_view file_path);

    /**
     * @brief 从文件路径加载音效
     * @param id 音效的唯一标识符, 通过entt::hashed_string生成
     * @param file_path 音效文件的路径
     * @return 加载的音效的指针
     * @note 如果音效已经加载，则返回已加载音效的指针
     * @note 如果音效未加载，则从文件路径加载音效，并返回加载的音效的指针
     */
    AudioBufferHandle loadSound(entt::id_type id, std::string_view file_path);

    /**
     * @brief 从字符串哈希值加载音效
     * @param str_hs entt::hashed_string类型
     * @return 加载的音效的指针
     * @note 如果音效已经加载，则返回已加载音效的指针
     * @note 如果音效未加载，则从哈希字符串对应的文件路径加载音效，并返回加载的音效的指针
     */
    AudioBufferHandle loadSound(entt::hashed_string str_hs);

    /**
     * @brief 从文件路径获取音效
     * @param id 音效的唯一标识符, 通过entt::hashed_string生成
     * @return 加载的音效的指针
     * @note 如果音效已经加载，则返回已加载音效的指针
     * @note 如果音效未加载，则从哈希字符串对应的文件路径加载音效，并返回加载的音效的指针
     */
    AudioBufferHandle getSound(entt::id_type id, std::string_view file_path = "");

    /**
     * @brief 从字符串哈希值获取音效
     * @param str_hs entt::hashed_string类型
     * @return 加载的音效的指针
     * @note 如果音效已经加载，则返回已加载音效的指针
     * @note 如果音效未加载，则从哈希字符串对应的文件路径加载音效，并返回加载的音效的指针
     */
    AudioBufferHandle getSound(entt::hashed_string str_hs);

    /**
     * @brief 卸载指定的音效资源
     * @param id 音效的唯一标识符, 通过entt::hashed_string生成
     */
    void unloadSound(entt::id_type id);

    /**
     * @brief 清空所有音效资源
     */
    void clearSounds();

    /**
     * @brief 从文件路径加载音乐
     * @param id 音乐的唯一标识符, 通过entt::hashed_string生成
     * @param file_path 音乐文件的路径
     * @return 加载的音乐的指针
     * @note 如果音乐已经加载，则返回已加载音乐的指针
     * @note 如果音乐未加载，则从文件路径加载音乐，并返回加载的音乐的指针
     */
    AudioBufferHandle loadMusic(entt::id_type id, std::string_view file_path);

    /**
     * @brief 从字符串哈希值加载音乐
     * @param str_hs entt::hashed_string类型
     * @return 加载的音乐的指针
     * @note 如果音乐已经加载，则返回已加载音乐的指针
     * @note 如果音乐未加载，则从哈希字符串对应的文件路径加载音乐，并返回加载的音乐的指针
     */
    AudioBufferHandle loadMusic(entt::hashed_string str_hs);

    /**
     * @brief 从文件路径获取音乐
     * @param id 音乐的唯一标识符, 通过entt::hashed_string生成
     * @return 加载的音乐的指针
     * @note 如果音乐已经加载，则返回已加载音乐的指针
     * @note 如果音乐未加载，则从哈希字符串对应的文件路径加载音乐，并返回加载的音乐的指针
     */
    AudioBufferHandle getMusic(entt::id_type id, std::string_view file_path = "");

    /**
     * @brief 从字符串哈希值获取音乐
     * @param str_hs entt::hashed_string类型
     * @return 加载的音乐的指针
     * @note 如果音乐已经加载，则返回已加载音乐的指针
     * @note 如果音乐未加载，则从哈希字符串对应的文件路径加载音乐，并返回加载的音乐的指针
     */
    AudioBufferHandle getMusic(entt::hashed_string str_hs);

    /**
     * @brief 卸载指定的音乐资源
     * @param id 音乐的唯一标识符, 通过entt::hashed_string生成
     */
    void unloadMusic(entt::id_type id);

    /**
     * @brief 清空所有音乐资源
     */
    void clearMusic();

    /**
     * @brief 清空所有音频资源
     */
    void clearAudio();

    void collectSoundDebugInfo(std::vector<AudioDebugInfo>& out) const;
    void collectMusicDebugInfo(std::vector<AudioDebugInfo>& out) const;

private:
    void collectAudioDebugInfo(
        const std::unordered_map<entt::id_type, AudioResourceEntry>& cache,
        AudioKind kind,
        std::vector<AudioDebugInfo>& out) const;
};

} // namespace engine::resource
