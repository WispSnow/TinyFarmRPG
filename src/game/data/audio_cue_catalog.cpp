#include "game/data/audio_cue_catalog.h"

#include "engine/resource/asset_registry.h"
#include "engine/utils/json_file_loader.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace game::data {
namespace {

using Json = nlohmann::json;

constexpr std::uint32_t kSupportedSchemaVersion = 1;

[[nodiscard]] bool parseRequiredString(const Json& object,
                                       const char* key,
                                       const std::string_view owner_label,
                                       std::string& out_value) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        spdlog::error("AudioCueCatalog: {} 缺少 string 字段 '{}'", owner_label, key);
        return false;
    }

    out_value = it->get<std::string>();
    if (out_value.empty()) {
        spdlog::error("AudioCueCatalog: {} 的 '{}' 不能为空", owner_label, key);
        return false;
    }
    return true;
}

[[nodiscard]] bool parseMusicCue(std::string_view cue_id, const Json& cue_node, MusicCueData& out_cue) {
    if (!cue_node.is_object()) {
        spdlog::error("AudioCueCatalog: music cue '{}' 必须是 object", cue_id);
        return false;
    }

    MusicCueData cue{};
    cue.id_ = std::string{cue_id};
    cue.id_hash_ = AudioCueCatalog::hashId(cue.id_);

    if (!parseRequiredString(cue_node, "music_id", cue.id_, cue.music_id_)) {
        return false;
    }
    cue.music_id_hash_ = AudioCueCatalog::hashId(cue.music_id_);

    if (const auto loop_it = cue_node.find("loop"); loop_it != cue_node.end()) {
        if (!loop_it->is_boolean()) {
            spdlog::error("AudioCueCatalog: music cue '{}' 的 loop 必须是 boolean", cue.id_);
            return false;
        }
        cue.loop_ = loop_it->get<bool>();
    }

    if (const auto fade_it = cue_node.find("fade_in_ms"); fade_it != cue_node.end()) {
        if (!fade_it->is_number_integer()) {
            spdlog::error("AudioCueCatalog: music cue '{}' 的 fade_in_ms 必须是整数", cue.id_);
            return false;
        }
        cue.fade_in_ms_ = fade_it->get<int>();
        if (cue.fade_in_ms_ < 0) {
            spdlog::error("AudioCueCatalog: music cue '{}' 的 fade_in_ms 必须 >= 0", cue.id_);
            return false;
        }
    }

    if (const auto volume_it = cue_node.find("volume_scale"); volume_it != cue_node.end()) {
        if (!volume_it->is_number()) {
            spdlog::error("AudioCueCatalog: music cue '{}' 的 volume_scale 必须是 number", cue.id_);
            return false;
        }
        cue.volume_scale_ = volume_it->get<float>();
        if (cue.volume_scale_ < 0.0F || cue.volume_scale_ > 1.0F) {
            spdlog::error("AudioCueCatalog: music cue '{}' 的 volume_scale 必须在 [0, 1]", cue.id_);
            return false;
        }
    }

    out_cue = std::move(cue);
    return true;
}

[[nodiscard]] bool parseSceneDefault(const Json& defaults_node,
                                     const char* key,
                                     std::string& out_cue_id,
                                     entt::id_type& out_cue_id_hash) {
    if (!parseRequiredString(defaults_node, key, "scene_defaults", out_cue_id)) {
        return false;
    }
    out_cue_id_hash = AudioCueCatalog::hashId(out_cue_id);
    return true;
}

} // namespace

entt::id_type AudioCueCatalog::hashId(const std::string_view id) {
    return entt::hashed_string{id.data(), id.size()}.value();
}

bool AudioCueCatalog::loadFromFile(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "AudioCueCatalog", spdlog::level::err)) {
        return false;
    }

    const auto schema_version = root.value("schema_version", 0U);
    if (schema_version != kSupportedSchemaVersion) {
        spdlog::error("AudioCueCatalog: '{}' 的 schema_version 必须为 {}", file_path, kSupportedSchemaVersion);
        return false;
    }

    const auto music_cues_it = root.find("music_cues");
    if (music_cues_it == root.end() || !music_cues_it->is_object() || music_cues_it->empty()) {
        spdlog::error("AudioCueCatalog: '{}' 缺少非空 music_cues object", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, MusicCueData> parsed_music_cues{};
    parsed_music_cues.reserve(music_cues_it->size());
    for (const auto& [cue_id, cue_node] : music_cues_it->items()) {
        if (cue_id.empty()) {
            spdlog::error("AudioCueCatalog: '{}' 存在空 music cue id", file_path);
            return false;
        }

        MusicCueData cue{};
        if (!parseMusicCue(cue_id, cue_node, cue)) {
            return false;
        }
        if (parsed_music_cues.contains(cue.id_hash_)) {
            spdlog::error("AudioCueCatalog: '{}' 存在重复 music cue id '{}'", file_path, cue.id_);
            return false;
        }
        parsed_music_cues.insert_or_assign(cue.id_hash_, std::move(cue));
    }

    if (const auto sfx_cues_it = root.find("sfx_cues");
        sfx_cues_it != root.end() && !sfx_cues_it->is_object()) {
        spdlog::error("AudioCueCatalog: '{}' 的 sfx_cues 必须是 object", file_path);
        return false;
    }

    const auto defaults_it = root.find("scene_defaults");
    if (defaults_it == root.end() || !defaults_it->is_object()) {
        spdlog::error("AudioCueCatalog: '{}' 缺少 scene_defaults object", file_path);
        return false;
    }

    std::string gameplay_default{};
    entt::id_type gameplay_default_hash{};
    if (!parseSceneDefault(*defaults_it, "gameplay", gameplay_default, gameplay_default_hash)) {
        return false;
    }

    std::string battle_default{};
    entt::id_type battle_default_hash{};
    if (!parseSceneDefault(*defaults_it, "battle", battle_default, battle_default_hash)) {
        return false;
    }

    if (!parsed_music_cues.contains(gameplay_default_hash)) {
        spdlog::error("AudioCueCatalog: scene_defaults.gameplay 引用未知 cue '{}'", gameplay_default);
        return false;
    }
    if (!parsed_music_cues.contains(battle_default_hash)) {
        spdlog::error("AudioCueCatalog: scene_defaults.battle 引用未知 cue '{}'", battle_default);
        return false;
    }

    schema_version_ = schema_version;
    music_cues_ = std::move(parsed_music_cues);
    gameplay_default_cue_id_ = std::move(gameplay_default);
    gameplay_default_cue_id_hash_ = gameplay_default_hash;
    battle_default_cue_id_ = std::move(battle_default);
    battle_default_cue_id_hash_ = battle_default_hash;
    return true;
}

bool AudioCueCatalog::validateReferences(const engine::resource::AssetRegistry& asset_registry,
                                         std::string& out_error) const {
    out_error.clear();

    for (const auto& [_, cue] : music_cues_) {
        if (asset_registry.findMusicPath(cue.music_id_hash_).empty()) {
            out_error = "music cue '" + cue.id_ + "' references unregistered music id '" + cue.music_id_ + "'";
            return false;
        }
    }

    return true;
}

const MusicCueData* AudioCueCatalog::findMusicCue(const entt::id_type id_hash) const {
    if (const auto it = music_cues_.find(id_hash); it != music_cues_.end()) {
        return &it->second;
    }
    return nullptr;
}

const MusicCueData* AudioCueCatalog::findMusicCue(const std::string_view id) const {
    return findMusicCue(hashId(id));
}

const MusicCueData* AudioCueCatalog::defaultMusicCue(const SceneAudioContext context) const {
    switch (context) {
        case SceneAudioContext::Gameplay:
            return findMusicCue(gameplay_default_cue_id_hash_);
        case SceneAudioContext::Battle:
            return findMusicCue(battle_default_cue_id_hash_);
    }

    return nullptr;
}

std::vector<const MusicCueData*> AudioCueCatalog::listMusicCues() const {
    std::vector<const MusicCueData*> cues;
    cues.reserve(music_cues_.size());
    for (const auto& [_, cue] : music_cues_) {
        cues.push_back(&cue);
    }
    std::ranges::sort(cues, {}, &MusicCueData::id_);
    return cues;
}

} // namespace game::data
