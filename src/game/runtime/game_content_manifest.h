#pragma once

namespace game::runtime {

struct GameContentManifest final {
    static constexpr const char* ActorBlueprints = "assets/data/actor_blueprint.json";
    static constexpr const char* AnimalBlueprints = "assets/data/animal_blueprint.json";
    static constexpr const char* CropBlueprints = "assets/data/crop_config.json";
    static constexpr const char* ItemIcons = "assets/data/icon_config.json";
    static constexpr const char* Items = "assets/data/item_config.json";
    static constexpr const char* AppearanceCatalog = "assets/data/appearance_catalog.json";
    static constexpr const char* VfxCatalog = "assets/data/vfx_catalog.json";
    static constexpr const char* RpgManifest = "assets/data/rpg/manifest.json";
    static constexpr const char* RpgRoot = "assets/data/rpg";
    static constexpr const char* Quests = "assets/data/quests.json";
    static constexpr const char* Shops = "assets/data/shops.json";
    static constexpr const char* AudioCues = "assets/data/audio_cues.json";
    static constexpr const char* GameTime = "assets/data/game_time_config.json";
    static constexpr const char* LightConfig = "assets/data/light_config.json";
    static constexpr const char* DialogueScript = "assets/data/dialogue_script.json";
    static constexpr const char* I18nLanguages = "assets/i18n/languages.json";
    static constexpr const char* World = "assets/maps/farm-rpg.world";
    static constexpr const char* MapLoadingConfig = "assets/data/map_loading_config.json";
    static constexpr const char* ScriptBootstrap = "scripts/bootstrap.lua";
    static constexpr const char* InitialMapName = "home_exterior";
    static constexpr const char* HomeInteriorMapName = "home_interior";
};

} // namespace game::runtime
