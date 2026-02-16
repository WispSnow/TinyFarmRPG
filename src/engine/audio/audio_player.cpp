#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#ifdef PlaySound    // 防止Windows平台下命名冲突
#undef PlaySound
#endif

#include "audio_player.h"
#include "engine/resource/resource_manager.h"

#include <spdlog/spdlog.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <vector>

namespace engine::audio {

namespace {
    constexpr float DEFAULT_SPATIAL_FALLOFF_DISTANCE = 600.0f;
    constexpr float DEFAULT_SPATIAL_PAN_RANGE = 300.0f;

    inline float clamp01(float value) {
        return glm::clamp(value, 0.0f, 1.0f);
    }
} // namespace

struct AudioPlayer::Impl {
    using AudioBufferHandle = engine::resource::AudioManager::AudioBufferHandle;

    struct ManagedSound {
        ma_sound handle{};
        ma_audio_buffer_ref buffer_ref{};
        AudioBufferHandle buffer;
        float base_volume{1.0f};
    };

    struct ManagedSoundDeleter {
        void operator()(ManagedSound* sound) const {
            if (!sound) {
                return;
            }
            ma_sound_uninit(&sound->handle);
            ma_audio_buffer_ref_uninit(&sound->buffer_ref);
            delete sound;
        }
    };

    using SoundPtr = std::unique_ptr<ManagedSound, ManagedSoundDeleter>;

    engine::resource::ResourceManager* resource_manager_{nullptr};
    ma_engine engine_{};
    bool engine_initialized_{false};

    float sound_volume_{1.0f};
    float music_volume_{1.0f};
    float spatial_falloff_distance_{DEFAULT_SPATIAL_FALLOFF_DISTANCE};
    float spatial_pan_range_{DEFAULT_SPATIAL_PAN_RANGE};

    std::vector<SoundPtr> active_sounds_;
    SoundPtr music_sound_;
    std::vector<SoundPtr> fading_music_sounds_;
    entt::id_type current_music_id_{};
    bool music_paused_{false};

    Impl() = default;

    [[nodiscard]] bool init(engine::resource::ResourceManager* resource_manager) {
        if (!resource_manager) {
            spdlog::error("AudioPlayer: ResourceManager 不能为空。");
            return false;
        }

        resource_manager_ = resource_manager;

        ma_engine_config config = ma_engine_config_init();
        config.listenerCount = 1;
        if (ma_engine_init(&config, &engine_) != MA_SUCCESS) {
            spdlog::error("AudioPlayer: 无法初始化 miniaudio 引擎。");
            resource_manager_ = nullptr;
            return false;
        }
        engine_initialized_ = true;
        spdlog::trace("AudioPlayer: miniaudio 引擎初始化完成");
        return true;
    }

    [[nodiscard]] static std::unique_ptr<Impl> create(engine::resource::ResourceManager* resource_manager) {
        auto impl = std::make_unique<Impl>();
        if (!impl->init(resource_manager)) {
            return nullptr;
        }
        return impl;
    }

    ~Impl() {
        // 先清理所有声音，再释放引擎
        active_sounds_.clear();
        music_sound_.reset();
        fading_music_sounds_.clear();

        if (engine_initialized_) {
            ma_engine_uninit(&engine_);
            engine_initialized_ = false;
        }
    }

    [[nodiscard]] SoundPtr createSound(AudioBufferHandle buffer) {
        if (!buffer || buffer->empty()) {
            spdlog::error("AudioPlayer: 无效的音频缓冲区。无法创建播放实例。");
            return {};
        }

        auto managed = std::make_unique<ManagedSound>();
        managed->buffer = buffer;

        const ma_result ref_result = ma_audio_buffer_ref_init(
            ma_format_f32,
            static_cast<ma_uint32>(buffer->channels),
            buffer->samples.data(),
            buffer->frame_count,
            &managed->buffer_ref
        );

        if (ref_result != MA_SUCCESS) {
            spdlog::error("AudioPlayer: 初始化音频缓冲区失败，错误码 {}", static_cast<int>(ref_result));
            return {};
        }

        managed->buffer_ref.sampleRate = buffer->sample_rate;

        const ma_result init_result = ma_sound_init_from_data_source(
            &engine_,
            reinterpret_cast<ma_data_source*>(&managed->buffer_ref),
            0,
            nullptr,
            &managed->handle
        );

        if (init_result != MA_SUCCESS) {
            spdlog::error("AudioPlayer: 使用缓存音频创建声音实例失败，错误码 {}", static_cast<int>(init_result));
            ma_audio_buffer_ref_uninit(&managed->buffer_ref);
            return {};
        }

        return SoundPtr(managed.release(), ManagedSoundDeleter{});
    }

    void applyVolume(ManagedSound& sound, float volume_factor) {
        sound.base_volume = clamp01(volume_factor);
        ma_sound_set_volume(&sound.handle, sound.base_volume * sound_volume_);
    }

    void cleanupFinishedSounds() {
        std::erase_if(active_sounds_, [](const SoundPtr& sound) {
            return !sound || ma_sound_is_playing(&sound->handle) == MA_FALSE;
        });
        std::erase_if(fading_music_sounds_, [](const SoundPtr& sound) {
            return !sound || ma_sound_is_playing(&sound->handle) == MA_FALSE;
        });
    }

    [[nodiscard]] bool playSoundInternal(const AudioBufferHandle& buffer, float base_volume, std::optional<float> pan) {
        cleanupFinishedSounds();

        auto sound = createSound(buffer);
        if (!sound) {
            return false;
        }

        applyVolume(*sound, base_volume);
        if (pan.has_value()) {
            ma_sound_set_pan(&sound->handle, glm::clamp(pan.value(), -1.0f, 1.0f));
        }

        if (ma_sound_start(&sound->handle) != MA_SUCCESS) {
            spdlog::error("AudioPlayer: 播放音效失败: 缓存不可用或引擎错误");
            return false;
        }

        active_sounds_.push_back(std::move(sound));
        return true;
    }

    [[nodiscard]] bool playSound(entt::id_type id, std::string_view sound_path) {
        auto buffer = resource_manager_->getSound(id, sound_path);
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音效资源 id={}", id);
            return false;
        }
        return playSoundInternal(buffer, 1.0f, std::nullopt);
    }

    [[nodiscard]] bool playSound(entt::hashed_string hashed_path) {
        auto buffer = resource_manager_->getSound(hashed_path);
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音效资源 id={}, path={}", hashed_path.value(), hashed_path.data());
            return false;
        }
        return playSoundInternal(buffer, 1.0f, std::nullopt);
    }

    [[nodiscard]] bool playSound2D(entt::id_type id, const glm::vec2& source, const glm::vec2& listener, std::string_view sound_path) {
        auto buffer = resource_manager_->getSound(id, sound_path);
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音效资源 id={} (2D)", id);
            return false;
        }
        return playSoundSpatialInternal(buffer, source, listener);
    }

    [[nodiscard]] bool playSound2D(entt::hashed_string hashed_path, const glm::vec2& source, const glm::vec2& listener) {
        auto buffer = resource_manager_->getSound(hashed_path);
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音效资源 id={}, path={} (2D)", hashed_path.value(), hashed_path.data());
            return false;
        }
        return playSoundSpatialInternal(buffer, source, listener);
    }

    [[nodiscard]] bool playSoundSpatialInternal(const AudioBufferHandle& buffer, const glm::vec2& source, const glm::vec2& listener) {
        glm::vec2 delta = source - listener;
        float distance = glm::length(delta);
        float attenuation = 1.0f;

        if (spatial_falloff_distance_ > 0.0f) {
            attenuation = clamp01(1.0f - (distance / spatial_falloff_distance_));
        }

        float pan = 0.0f;
        if (spatial_pan_range_ > 0.0f) {
            pan = glm::clamp(delta.x / spatial_pan_range_, -1.0f, 1.0f);
        }

        return playSoundInternal(buffer, attenuation, pan);
    }

    void fadeOutMusicInternal(int fade_out_ms) {
        if (!music_sound_) {
            return;
        }

        cleanupFinishedSounds();

        if (fade_out_ms > 0 && ma_sound_is_playing(&music_sound_->handle) == MA_TRUE) {
            ma_sound_stop_with_fade_in_milliseconds(&music_sound_->handle, static_cast<ma_uint64>(fade_out_ms));
            fading_music_sounds_.push_back(std::move(music_sound_));
        } else {
            ma_sound_stop(&music_sound_->handle);
            ma_sound_seek_to_pcm_frame(&music_sound_->handle, 0);
            music_sound_.reset();
        }

        current_music_id_ = {};
        music_paused_ = false;
    }

    [[nodiscard]] bool playMusicInternal(const AudioBufferHandle& buffer, entt::id_type id, bool loop, int fade_in_ms) {
        cleanupFinishedSounds();

        // 如果有正在播放的音乐，则按淡入时长对旧音乐做淡出（实现"切歌可淡出"的稳定语义）
        if (music_sound_) {
            fadeOutMusicInternal(fade_in_ms > 0 ? fade_in_ms : 0);
        }

        auto music = createSound(buffer);
        if (!music) {
            return false;
        }

        if (loop) {
            ma_sound_set_looping(&music->handle, MA_TRUE);
        }

        ma_sound_set_volume(&music->handle, music_volume_);

        if (ma_sound_start(&music->handle) != MA_SUCCESS) {
            spdlog::error("AudioPlayer: 播放音乐失败，音频引擎返回错误。");
            return false;
        }

        if (fade_in_ms > 0) {
            ma_sound_set_fade_in_milliseconds(&music->handle, 0.0f, 1.0f, static_cast<ma_uint64>(fade_in_ms));
        }

        music->base_volume = 1.0f;
        current_music_id_ = id;
        music_paused_ = false;
        music_sound_ = std::move(music);
        return true;
    }

    [[nodiscard]] bool playMusic(entt::id_type id, bool loop, int fade_in_ms, std::string_view music_path) {
        if (id == current_music_id_ && music_sound_ && ma_sound_is_playing(&music_sound_->handle) == MA_TRUE) {
            return true;
        }

        auto buffer = resource_manager_->getMusic(id, music_path);
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音乐资源 id={}", id);
            return false;
        }

        return playMusicInternal(buffer, id, loop, fade_in_ms);
    }

    [[nodiscard]] bool playMusic(entt::hashed_string hashed_path, bool loop, int fade_in_ms) {
        auto buffer = resource_manager_->getMusic(hashed_path, hashed_path.data());
        if (!buffer) {
            spdlog::error("AudioPlayer: 找不到音乐资源 id={}, path={}", hashed_path.value(), hashed_path.data());
            return false;
        }
        return playMusicInternal(buffer, hashed_path.value(), loop, fade_in_ms);
    }

    void stopMusic(int fade_out_ms) {
        if (!music_sound_) {
            return;
        }

        if (fade_out_ms > 0) {
            fadeOutMusicInternal(fade_out_ms);
            spdlog::trace("AudioPlayer: 停止音乐（淡出 {}ms）", fade_out_ms);
            return;
        }

        ma_sound_stop(&music_sound_->handle);
        ma_sound_seek_to_pcm_frame(&music_sound_->handle, 0);

        music_sound_.reset();
        current_music_id_ = {};
        music_paused_ = false;
        spdlog::trace("AudioPlayer: 停止音乐");
    }

    void pauseMusic() {
        if (!music_sound_) {
            return;
        }

        if (ma_sound_is_playing(&music_sound_->handle) == MA_TRUE) {
            ma_sound_stop(&music_sound_->handle);
            music_paused_ = true;
            spdlog::trace("AudioPlayer: 暂停音乐");
        }
    }

    void resumeMusic() {
        if (!music_sound_ || !music_paused_) {
            return;
        }

        if (ma_sound_start(&music_sound_->handle) == MA_SUCCESS) {
            ma_sound_set_volume(&music_sound_->handle, music_sound_->base_volume * music_volume_);
            music_paused_ = false;
            spdlog::trace("AudioPlayer: 恢复音乐");
        } else {
            spdlog::error("AudioPlayer: 恢复音乐失败");
        }
    }

    void setSoundVolume(float volume) {
        sound_volume_ = clamp01(volume);
        cleanupFinishedSounds();
        for (auto& sound : active_sounds_) {
            if (sound) {
                ma_sound_set_volume(&sound->handle, sound->base_volume * sound_volume_);
            }
        }
        spdlog::trace("AudioPlayer: 设置音效音量为 {:.2f}", sound_volume_);
    }

    void setMusicVolume(float volume) {
        music_volume_ = clamp01(volume);
        if (music_sound_) {
            ma_sound_set_volume(&music_sound_->handle, music_sound_->base_volume * music_volume_);
        }
        cleanupFinishedSounds();
        for (auto& sound : fading_music_sounds_) {
            if (sound) {
                ma_sound_set_volume(&sound->handle, sound->base_volume * music_volume_);
            }
        }
        spdlog::trace("AudioPlayer: 设置音乐音量为 {:.2f}", music_volume_);
    }

    [[nodiscard]] float getMusicVolume() const {
        return music_volume_;
    }

    [[nodiscard]] float getSoundVolume() const {
        return sound_volume_;
    }

    void setSpatialFalloffDistance(float value) {
        spatial_falloff_distance_ = std::max(0.0f, value);
    }

    [[nodiscard]] float getSpatialFalloffDistance() const {
        return spatial_falloff_distance_;
    }

    void setSpatialPanRange(float value) {
        spatial_pan_range_ = std::max(0.0f, value);
    }

    [[nodiscard]] float getSpatialPanRange() const {
        return spatial_pan_range_;
    }
};

std::unique_ptr<AudioPlayer> AudioPlayer::create(engine::resource::ResourceManager* resource_manager) {
    auto impl = Impl::create(resource_manager);
    if (!impl) {
        return nullptr;
    }
    return std::unique_ptr<AudioPlayer>(new AudioPlayer(std::move(impl)));
}

AudioPlayer::AudioPlayer(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {
    [[maybe_unused]] const bool config_loaded = loadConfig(DEFAULT_CONFIG_PATH);
}

AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::playSound(entt::id_type sound_id, std::string_view sound_path) {
    return impl_->playSound(sound_id, sound_path);
}

bool AudioPlayer::playSound(entt::hashed_string hashed_path) {
    return impl_->playSound(hashed_path);
}

bool AudioPlayer::playSound2D(entt::id_type sound_id, const glm::vec2& source, const glm::vec2& listener, std::string_view sound_path) {
    return impl_->playSound2D(sound_id, source, listener, sound_path);
}

bool AudioPlayer::playSound2D(entt::hashed_string hashed_path, const glm::vec2& source, const glm::vec2& listener) {
    return impl_->playSound2D(hashed_path, source, listener);
}

bool AudioPlayer::playMusic(entt::id_type music_id, bool loop, int fade_in_ms, std::string_view music_path) {
    return impl_->playMusic(music_id, loop, fade_in_ms, music_path);
}

bool AudioPlayer::playMusic(entt::hashed_string hashed_path, bool loop, int fade_in_ms) {
    return impl_->playMusic(hashed_path, loop, fade_in_ms);
}

void AudioPlayer::stopMusic(int fade_out_ms) {
    impl_->stopMusic(fade_out_ms);
}

void AudioPlayer::pauseMusic() {
    impl_->pauseMusic();
}

void AudioPlayer::resumeMusic() {
    impl_->resumeMusic();
}

void AudioPlayer::setSoundVolume(float volume) {
    impl_->setSoundVolume(volume);
}

void AudioPlayer::setMusicVolume(float volume) {
    impl_->setMusicVolume(volume);
}

float AudioPlayer::getMusicVolume() const {
    return impl_->getMusicVolume();
}

float AudioPlayer::getSoundVolume() const {
    return impl_->getSoundVolume();
}

bool AudioPlayer::loadConfig(std::string_view config_path) {
    if (config_path.empty()) {
        return false;
    }

    const std::filesystem::path path{config_path};
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("AudioPlayer: 无法打开音频配置文件 '{}'，使用默认配置。", path.string());
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::warn("AudioPlayer: 解析音频配置 '{}' 失败，使用默认配置。", path.string());
        return false;
    }

    const nlohmann::json* config = &root;
    if (auto it = root.find("audio"); it != root.end() && it->is_object()) {
        config = &(*it);
    }

    auto readFloat = [](const nlohmann::json& obj, std::string_view key, float fallback) -> float {
        const auto it = obj.find(std::string(key));
        if (it == obj.end()) {
            return fallback;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_float_t*>()) {
            return static_cast<float>(*v);
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_integer_t*>()) {
            return static_cast<float>(*v);
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            return static_cast<float>(*v);
        }
        return fallback;
    };

    const float music_volume = readFloat(*config, "music_volume", getMusicVolume());
    const float sound_volume = readFloat(*config, "sound_volume", getSoundVolume());

    const nlohmann::json* spatial = nullptr;
    if (auto it = config->find("spatial"); it != config->end() && it->is_object()) {
        spatial = &(*it);
    }

    const float falloff_distance =
        spatial ? readFloat(*spatial, "falloff_distance", getSpatialFalloffDistance()) : getSpatialFalloffDistance();
    const float pan_range = spatial ? readFloat(*spatial, "pan_range", getSpatialPanRange()) : getSpatialPanRange();

    setMusicVolume(music_volume);
    setSoundVolume(sound_volume);
    setSpatialFalloffDistance(falloff_distance);
    setSpatialPanRange(pan_range);

    spdlog::info("AudioPlayer: 成功加载音频配置 '{}'", path.string());
    return true;
}

bool AudioPlayer::saveConfig(std::string_view config_path) const {
    if (config_path.empty()) {
        return false;
    }

    const std::filesystem::path path{config_path};
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::warn("AudioPlayer: 无法写入音频配置文件 '{}'", path.string());
        return false;
    }

    const nlohmann::ordered_json json{
        {"audio",
         {{"music_volume", getMusicVolume()},
          {"sound_volume", getSoundVolume()},
          {"spatial",
           {{"falloff_distance", getSpatialFalloffDistance()},
            {"pan_range", getSpatialPanRange()}}}}}};

    file << json.dump(2);
    spdlog::info("AudioPlayer: 已写入音频配置 '{}'", path.string());
    return true;
}

void AudioPlayer::setSpatialFalloffDistance(float value) {
    impl_->setSpatialFalloffDistance(value);
}

float AudioPlayer::getSpatialFalloffDistance() const {
    return impl_->getSpatialFalloffDistance();
}

void AudioPlayer::setSpatialPanRange(float value) {
    impl_->setSpatialPanRange(value);
}

float AudioPlayer::getSpatialPanRange() const {
    return impl_->getSpatialPanRange();
}

} // namespace engine::audio
