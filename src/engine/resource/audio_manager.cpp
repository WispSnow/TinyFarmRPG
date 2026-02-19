#include "audio_manager.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

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

AudioBufferHandle AudioManager::findSound(entt::id_type id) const {
    return sound_cache_[id];
}

AudioBufferHandle AudioManager::findMusic(entt::id_type id) const {
    return music_cache_[id];
}

// --- 音效管理 ---
AudioBufferHandle AudioManager::loadSound(entt::id_type id, std::string_view file_path) {
    auto [it, _] = sound_cache_.load(id, file_path);
    AudioBufferHandle handle = it->second;
    if (!handle) {
        sound_cache_.erase(id);
        spdlog::error("AudioManager: 音效 '{}' 加载失败 (路径: '{}')，已移除无效缓存条目。", id, file_path);
        return {};
    }

    spdlog::debug("AudioManager: 缓存音效 {} -> {}", id, file_path);
    return handle;
}

AudioBufferHandle AudioManager::loadSound(entt::hashed_string str_hs) {
    return loadSound(str_hs.value(), str_hs.data());
}

void AudioManager::unloadSound(entt::id_type id) {
    if (sound_cache_.erase(id) > 0U) {
        spdlog::debug("AudioManager: 卸载音效 {}", id);
    } else {
        spdlog::warn("AudioManager: 尝试卸载不存在的音效 id={}", id);
    }
}

void AudioManager::clearSounds() {
    if (!sound_cache_.empty()) {
        spdlog::debug("AudioManager: 清除所有 {} 个音效缓存。", sound_cache_.size());
        sound_cache_.clear();
    }
}

// --- 音乐管理 ---
AudioBufferHandle AudioManager::loadMusic(entt::id_type id, std::string_view file_path) {
    auto [it, _] = music_cache_.load(id, file_path);
    AudioBufferHandle handle = it->second;
    if (!handle) {
        music_cache_.erase(id);
        spdlog::error("AudioManager: 音乐 '{}' 加载失败 (路径: '{}')，已移除无效缓存条目。", id, file_path);
        return {};
    }

    spdlog::debug("AudioManager: 缓存音乐 {} -> {}", id, file_path);
    return handle;
}

AudioBufferHandle AudioManager::loadMusic(entt::hashed_string str_hs) {
    return loadMusic(str_hs.value(), str_hs.data());
}

void AudioManager::unloadMusic(entt::id_type id) {
    if (music_cache_.erase(id) > 0U) {
        spdlog::debug("AudioManager: 卸载音乐 {}", id);
    } else {
        spdlog::warn("AudioManager: 尝试卸载不存在的音乐 id={}", id);
    }
}

void AudioManager::clearMusic() {
    if (!music_cache_.empty()) {
        spdlog::debug("AudioManager: 清除所有 {} 首音乐缓存。", music_cache_.size());
        music_cache_.clear();
    }
}

void AudioManager::clearAudio()
{
    clearSounds();
    clearMusic();
}

void AudioManager::collectAudioDebugInfo(
    const SoundCache& cache,
    AudioKind kind,
    std::vector<AudioDebugInfo>& out) const {
    out.reserve(out.size() + cache.size());
    for (const auto [id, handle] : cache) {
        if (!handle) {
            continue;
        }
        AudioDebugInfo info{};
        info.id = id;
        info.kind = kind;
        info.channels = handle->channels;
        info.sample_rate = handle->sample_rate;
        info.frame_count = handle->frame_count;
        info.sample_count = handle->samples.size();
        if (info.sample_rate > 0U) {
            info.duration_seconds = static_cast<double>(info.frame_count) / static_cast<double>(info.sample_rate);
        }
        out.push_back(std::move(info));
    }
}

void AudioManager::collectSoundDebugInfo(std::vector<AudioDebugInfo>& out) const {
    out.clear();
    collectAudioDebugInfo(sound_cache_, AudioKind::Sound, out);
}

void AudioManager::collectMusicDebugInfo(std::vector<AudioDebugInfo>& out) const {
    out.clear();
    collectAudioDebugInfo(music_cache_, AudioKind::Music, out);
}

} // namespace engine::resource
