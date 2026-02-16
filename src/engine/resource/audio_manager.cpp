#include "audio_manager.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>
#include <miniaudio.h>

namespace engine::resource {

AudioManager::AudioManager() {
    spdlog::trace("AudioManager 构造成功。");
}

AudioManager::~AudioManager()
{
    clearSounds();
    clearMusic();
    spdlog::trace("AudioManager 析构成功。");
}

namespace {
    constexpr std::size_t DECODE_CHUNK_FRAMES = 4096;
} // namespace

AudioManager::AudioBufferPtr AudioManager::decodeAudio(std::string_view file_path) {
    if (file_path.empty()) {
        spdlog::error("AudioManager: decodeAudio 失败，提供的文件路径为空。");
        return {};
    }

    const std::string path_str(file_path);  // 确保 null 终止（ma_decoder_init_file 要求 C 字符串）
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder{};
    ma_result init_result = ma_decoder_init_file(path_str.c_str(), &config, &decoder);
    if (init_result != MA_SUCCESS) {
        spdlog::error("AudioManager: 无法解码音频文件 '{}', 错误码 {}", file_path, static_cast<int>(init_result));
        return {};
    }

    const std::uint32_t channels = decoder.outputChannels;
    const std::uint32_t sample_rate = decoder.outputSampleRate;

    if (channels == 0 || sample_rate == 0) {
        spdlog::error("AudioManager: 解码音频文件 '{}' 失败，通道数或采样率无效。", file_path);
        ma_decoder_uninit(&decoder);
        return {};
    }

    std::vector<float> samples;
    samples.reserve(DECODE_CHUNK_FRAMES * channels);

    std::vector<float> read_buffer(DECODE_CHUNK_FRAMES * channels);
    std::uint64_t total_frames = 0;

    while (true) {
        ma_uint64 frames_read = 0;
        const ma_result read_result = ma_decoder_read_pcm_frames(
            &decoder,
            read_buffer.data(),
            DECODE_CHUNK_FRAMES,
            &frames_read
        );

        if ((read_result != MA_SUCCESS && read_result != MA_AT_END) || frames_read == 0) {
            if (read_result != MA_SUCCESS && read_result != MA_AT_END) {
                spdlog::error("AudioManager: 读取音频文件 '{}' 时出错，错误码 {}", file_path, static_cast<int>(read_result));
            }
            break;
        }

        const std::size_t sample_count = static_cast<std::size_t>(frames_read) * channels;
        samples.insert(samples.end(), read_buffer.data(), read_buffer.data() + sample_count);
        total_frames += frames_read;

        if (read_result == MA_AT_END) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);

    if (total_frames == 0 || samples.empty()) {
        spdlog::warn("AudioManager: 解码文件 '{}' 未获得有效音频数据。", file_path);
        return {};
    }

    auto buffer = std::make_shared<AudioBuffer>();
    buffer->samples = std::move(samples);
    buffer->channels = channels;
    buffer->sample_rate = sample_rate;
    buffer->frame_count = total_frames;

    spdlog::debug(
        "AudioManager: 成功解码音频 '{}'，通道数={}, 采样率={}, 帧数={}",
        file_path,
        channels,
        sample_rate,
        total_frames
    );

    return buffer;
}

// --- 音效管理 ---
AudioManager::AudioBufferHandle AudioManager::loadSound(entt::id_type id, std::string_view file_path) {
    auto it = sounds_.find(id);
    if (it != sounds_.end()) {
        return it->second.buffer;
    }

    auto buffer = decodeAudio(file_path);
    if (!buffer || buffer->empty()) {
        spdlog::error("AudioManager: 音效 '{}' 加载失败 (路径: '{}')", id, file_path);
        return {};
    }

    spdlog::debug("AudioManager: 缓存音效 {} -> {}", id, file_path);
    AudioResourceEntry entry{};
    entry.buffer = std::move(buffer);
    entry.source_path = std::string(file_path);
    auto [inserted_it, _] = sounds_.emplace(id, std::move(entry));
    return inserted_it->second.buffer;
}

AudioManager::AudioBufferHandle AudioManager::loadSound(entt::hashed_string str_hs) {
    return loadSound(str_hs.value(), str_hs.data());
}

AudioManager::AudioBufferHandle AudioManager::getSound(entt::id_type id, std::string_view file_path) {
    auto it = sounds_.find(id);
    if (it != sounds_.end()) {
        return it->second.buffer;
    }

    if (file_path.empty()) {
        spdlog::error("AudioManager: 音效 '{}' 未缓存且未提供加载路径。", id);
        return {};
    }

    spdlog::trace("AudioManager: 音效 '{}' 未缓存，尝试即时加载。", id);
    return loadSound(id, file_path);
}

AudioManager::AudioBufferHandle AudioManager::getSound(entt::hashed_string str_hs) {
    return getSound(str_hs.value(), str_hs.data());
}

void AudioManager::unloadSound(entt::id_type id) {
    auto it = sounds_.find(id);
    if (it != sounds_.end()) {
        spdlog::debug("AudioManager: 卸载音效 {}", id);
        sounds_.erase(it);
    } else {
        spdlog::warn("AudioManager: 尝试卸载不存在的音效 id={}", id);
    }
}

void AudioManager::clearSounds() {
    if (!sounds_.empty()) {
        spdlog::debug("AudioManager: 清除所有 {} 个音效缓存。", sounds_.size());
        sounds_.clear();
    }
}

// --- 音乐管理 ---
AudioManager::AudioBufferHandle AudioManager::loadMusic(entt::id_type id, std::string_view file_path) {
    auto it = music_.find(id);
    if (it != music_.end()) {
        return it->second.buffer;
    }

    auto buffer = decodeAudio(file_path);
    if (!buffer || buffer->empty()) {
        spdlog::error("AudioManager: 音乐 '{}' 加载失败 (路径: '{}')", id, file_path);
        return {};
    }

    spdlog::debug("AudioManager: 缓存音乐 {} -> {}", id, file_path);
    AudioResourceEntry entry{};
    entry.buffer = std::move(buffer);
    entry.source_path = std::string(file_path);
    auto [inserted_it, _] = music_.emplace(id, std::move(entry));
    return inserted_it->second.buffer;
}

AudioManager::AudioBufferHandle AudioManager::loadMusic(entt::hashed_string str_hs) {
    return loadMusic(str_hs.value(), str_hs.data());
}

AudioManager::AudioBufferHandle AudioManager::getMusic(entt::id_type id, std::string_view file_path) {
    auto it = music_.find(id);
    if (it != music_.end()) {
        return it->second.buffer;
    }

    if (file_path.empty()) {
        spdlog::error("AudioManager: 音乐 '{}' 未缓存且未提供加载路径。", id);
        return {};
    }

    spdlog::trace("AudioManager: 音乐 '{}' 未缓存，尝试即时加载。", id);
    return loadMusic(id, file_path);
}

AudioManager::AudioBufferHandle AudioManager::getMusic(entt::hashed_string str_hs) {
    return getMusic(str_hs.value(), str_hs.data());
}

void AudioManager::unloadMusic(entt::id_type id) {
    auto it = music_.find(id);
    if (it != music_.end()) {
        spdlog::debug("AudioManager: 卸载音乐 {}", id);
        music_.erase(it);
    } else {
        spdlog::warn("AudioManager: 尝试卸载不存在的音乐 id={}", id);
    }
}

void AudioManager::clearMusic() {
    if (!music_.empty()) {
        spdlog::debug("AudioManager: 清除所有 {} 首音乐缓存。", music_.size());
        music_.clear();
    }
}

void AudioManager::clearAudio()
{
    clearSounds();
    clearMusic();
}

void AudioManager::collectAudioDebugInfo(
    const std::unordered_map<entt::id_type, AudioResourceEntry>& cache,
    AudioKind kind,
    std::vector<AudioDebugInfo>& out) const {
    out.reserve(out.size() + cache.size());
    for (const auto& [id, entry] : cache) {
        if (!entry.buffer) {
            continue;
        }
        AudioDebugInfo info{};
        info.id = id;
        info.kind = kind;
        info.source = entry.source_path;
        info.channels = entry.buffer->channels;
        info.sample_rate = entry.buffer->sample_rate;
        info.frame_count = entry.buffer->frame_count;
        info.sample_count = entry.buffer->samples.size();
        info.memory_bytes = info.sample_count * sizeof(float);
        if (info.sample_rate > 0U) {
            info.duration_seconds = static_cast<double>(info.frame_count) / static_cast<double>(info.sample_rate);
        }
        out.push_back(std::move(info));
    }
}

void AudioManager::collectSoundDebugInfo(std::vector<AudioDebugInfo>& out) const {
    collectAudioDebugInfo(sounds_, AudioKind::Sound, out);
}

void AudioManager::collectMusicDebugInfo(std::vector<AudioDebugInfo>& out) const {
    collectAudioDebugInfo(music_, AudioKind::Music, out);
}

} // namespace engine::resource
