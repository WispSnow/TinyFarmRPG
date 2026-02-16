#pragma once
#include <memory>
#include <string_view>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <entt/core/hashed_string.hpp>

namespace engine::resource {
    class ResourceManager;
}

namespace engine::audio {

/**
 * @brief 用于控制音频播放的单例类。
 *
 * @note 提供播放音效和音乐的方法，使用由 ResourceManager 管理的资源。
 * @note 必须使用有效的 ResourceManager 实例初始化。
 */
class AudioPlayer final{
private:
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< @brief 内部实现，封装具体的音频播放逻辑。

public:
    static constexpr std::string_view DEFAULT_CONFIG_PATH{"config/audio.json"};

    [[nodiscard]] static std::unique_ptr<AudioPlayer> create(engine::resource::ResourceManager* resource_manager);
    ~AudioPlayer();

    // 删除复制/移动操作
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;

    // --- 播放控制方法 --- 
    /**
     * @brief 播放音效（chunk）。
     * @note 必须确保 ResourceManager 加载了音效。
     * @param sound_id 音效ID。
     * @param sound_path 音效文件路径。
     * @return 成功返回 true，失败返回 false。
     */
    [[nodiscard]] bool playSound(entt::id_type sound_id, std::string_view sound_path = "");
    /**
     * @brief 播放音效（chunk）。
     * @note 如果尚未缓存，则通过 ResourceManager 加载音效。
     * @param hashed_path 音效文件路径。
     * @return 成功返回 true，失败返回 false。
     */
    [[nodiscard]] bool playSound(entt::hashed_string hashed_path);

    /**
     * @brief 播放带有 2D 空间感知效果的音效。
     * @param sound_id 音效ID。
     * @param source 音源位置。
     * @param listener 监听者位置。
     * @param sound_path 音效文件路径。
     * @return 成功返回 true，失败返回 false。
     */
    [[nodiscard]] bool playSound2D(entt::id_type sound_id, const glm::vec2& source, const glm::vec2& listener, std::string_view sound_path = "");

    /**
     * @brief 播放带有 2D 空间感知效果的音效。
     * @param hashed_path 音效文件路径（哈希）。
     * @param source 音源位置。
     * @param listener 监听者位置。
     * @return 成功返回 true，失败返回 false。
     */
    [[nodiscard]] bool playSound2D(entt::hashed_string hashed_path, const glm::vec2& source, const glm::vec2& listener);

    /**
     * @brief 播放背景音乐。如果正在播放，且本次指定了淡入时间，则旧音乐会按同样时长淡出（形成短暂的 cross-fade）。
     * @note 必须确保 ResourceManager 加载了音乐。
     * @param music_id 音乐ID。
     * @param loop 是否循环播放。
     * @param fade_in_ms 音乐淡入的时间（毫秒）（0 表示不淡入）。默认为 0。
     * @param music_path 音乐文件路径。
     * @return 成功返回 true，出错返回 false。
     */
    bool playMusic(entt::id_type music_id, bool loop = true, int fade_in_ms = 0, std::string_view music_path = "");

    /**
     * @brief 播放背景音乐。如果正在播放，且本次指定了淡入时间，则旧音乐会按同样时长淡出（形成短暂的 cross-fade）。
     * @note 如果尚未缓存，则通过 ResourceManager 加载音乐。
     * @param hashed_path 音乐文件路径。
     * @param loop 是否循环播放。
     * @param fade_in_ms 音乐淡入的时间（毫秒）（0 表示不淡入）。默认为 0。
     * @return 成功返回 true，出错返回 false。
     */
    bool playMusic(entt::hashed_string hashed_path, bool loop = true, int fade_in_ms = 0);

    /**
     * @brief 停止当前正在播放的背景音乐。
     * @param fade_out_ms 淡出时间（毫秒）（0 表示立即停止）。默认为 0。
     */
    void stopMusic(int fade_out_ms = 0);

    /**
     * @brief 暂停当前正在播放的背景音乐。
     */
    void pauseMusic();

    /**
     * @brief 恢复已暂停的背景音乐。
     */
    void resumeMusic();

    /**
     * @brief 设置音效通道的音量。
     * @param volume 音量级别（0.0-1.0）。
     */
    void setSoundVolume(float volume);

    /**
     * @brief 设置音乐通道的音量。
     * @param volume 音量级别（0.0-1.0）。
     */
    void setMusicVolume(float volume);

    /**
     * @brief 获取当前音乐音量。
     * @return 音量级别（0.0-1.0）。
     */
    float getMusicVolume() const;

    /**
     * @brief 获取当前音效音量。
     * @return 音量级别（0.0-1.0）。
     */
    float getSoundVolume() const;

    [[nodiscard]] bool loadConfig(std::string_view config_path);
    [[nodiscard]] bool saveConfig(std::string_view config_path) const;

    void setSpatialFalloffDistance(float value);
    [[nodiscard]] float getSpatialFalloffDistance() const;

    void setSpatialPanRange(float value);
    [[nodiscard]] float getSpatialPanRange() const;

private:
    explicit AudioPlayer(std::unique_ptr<Impl> impl);
};

} // namespace engine::audio
