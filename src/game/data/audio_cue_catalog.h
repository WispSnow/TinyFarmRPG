#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>

namespace engine::resource {
class AssetRegistry;
}

namespace game::data {

enum class SceneAudioContext {
    Gameplay,
    Battle
};

struct MusicCueData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string music_id_{};
    entt::id_type music_id_hash_{};
    bool loop_{true};
    int fade_in_ms_{0};
    float volume_scale_{1.0F};
};

class AudioCueCatalog final {
public:
    [[nodiscard]] static entt::id_type hashId(std::string_view id);

    [[nodiscard]] bool loadFromFile(std::string_view file_path);
    [[nodiscard]] bool validateReferences(const engine::resource::AssetRegistry& asset_registry,
                                          std::string& out_error) const;

    [[nodiscard]] std::uint32_t schemaVersion() const { return schema_version_; }
    [[nodiscard]] const MusicCueData* findMusicCue(entt::id_type id_hash) const;
    [[nodiscard]] const MusicCueData* findMusicCue(std::string_view id) const;
    [[nodiscard]] const MusicCueData* defaultMusicCue(SceneAudioContext context) const;
    [[nodiscard]] std::vector<const MusicCueData*> listMusicCues() const;

private:
    std::uint32_t schema_version_{0};
    std::unordered_map<entt::id_type, MusicCueData> music_cues_{};
    std::string gameplay_default_cue_id_{};
    entt::id_type gameplay_default_cue_id_hash_{};
    std::string battle_default_cue_id_{};
    entt::id_type battle_default_cue_id_hash_{};
};

} // namespace game::data
