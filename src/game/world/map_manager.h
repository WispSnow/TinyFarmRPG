#pragma once

#include <memory>
#include <string_view>
#include <unordered_set>
#include <glm/vec2.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include "game/world/world_state.h"
#include "map_loading_settings.h"
#include "engine/utils/events.h"
#include "game/component/map_component.h"

namespace engine::scene {
    class Scene;
}

namespace engine::core {
    class Context;
}

namespace engine::loader {
    class LevelLoader;
}

namespace game::factory {
    class EntityFactory;
    class BlueprintManager;
}

namespace game::world {

class MapManager {
    engine::scene::Scene& scene_;
    engine::core::Context& context_;
    entt::registry& registry_;
    game::world::WorldState& world_state_;
    game::factory::EntityFactory& entity_factory_;
    game::factory::BlueprintManager& blueprint_manager_;
    std::unique_ptr<engine::loader::LevelLoader> level_loader_;
    game::world::MapLoadingSettings loading_settings_{};
    std::unordered_set<entt::id_type> preloaded_maps_{};
    entt::id_type current_map_id_{entt::null};
    glm::vec2 current_map_pixel_size_{0.0f};

public:
    MapManager(engine::scene::Scene& scene,
               engine::core::Context& context,
               entt::registry& registry,
               game::world::WorldState& world_state,
               game::factory::EntityFactory& entity_factory,
               game::factory::BlueprintManager& blueprint_manager);

    ~MapManager();

    [[nodiscard]] bool loadMap(entt::id_type map_id);
    [[nodiscard]] bool loadMap(std::string_view map_name);
    void setLoadingSettings(game::world::MapLoadingSettings settings);
    const game::world::MapLoadingSettings& loadingSettings() const { return loading_settings_; }
    void preloadAllMaps();
    [[nodiscard]] std::size_t preloadedMapCount() const { return preloaded_maps_.size(); }
    [[nodiscard]] bool isMapPreloaded(entt::id_type map_id) const;
    void unloadCurrentMap();
    void snapshotCurrentMap();
    void snapCameraTo(const glm::vec2& world_pos);

    entt::id_type currentMapId() const { return current_map_id_; }
    glm::vec2 currentMapPixelSize() const { return current_map_pixel_size_; }
    void onDayChanged(const engine::utils::DayChangedEvent& event);

private:
    void configureCamera(glm::vec2 map_pixel_size);
    bool preloadMap(entt::id_type map_id);
    void preloadRelatedMaps(entt::id_type map_id);
    void destroyEntitiesByMap(entt::id_type map_id);
    void snapshotMap(entt::id_type map_id);
    void restoreSnapshot(entt::id_type map_id);
    void applyPendingResourceNodes(entt::id_type map_id);
    void applyOpenedChests(entt::id_type map_id);
    void advanceOffline(game::world::MapState& state, std::uint32_t current_day);
};

} // namespace game::world
