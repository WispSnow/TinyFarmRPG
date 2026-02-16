#pragma once
#include "blueprint.h"
#include "game/defs/crop_defs.h"
#include <glm/vec2.hpp>
#include <entt/entity/fwd.hpp>
#include <cstdint>

namespace engine::spatial {
    class SpatialIndexManager;
}
namespace engine::resource {
    class AutoTileLibrary;
}
namespace engine::component {
    struct Sprite;
}

namespace game::factory {

class BlueprintManager;

class EntityFactory {
    entt::registry& registry_;
    BlueprintManager& blueprint_manager_;

    engine::spatial::SpatialIndexManager* spatial_index_manager_ = nullptr;  ///< @brief 空间索引管理器（非拥有指针）
    engine::resource::AutoTileLibrary* auto_tile_library_ = nullptr;         ///< @brief 自动图块库（非拥有指针）
    
public:
    EntityFactory(entt::registry& registry,
                  BlueprintManager& blueprint_manager,
                  engine::spatial::SpatialIndexManager* spatial_index_manager = nullptr,
                  engine::resource::AutoTileLibrary* auto_tile_library = nullptr)
        : registry_(registry),
          blueprint_manager_(blueprint_manager),
          spatial_index_manager_(spatial_index_manager),
          auto_tile_library_(auto_tile_library) {}
    ~EntityFactory() = default;

    [[nodiscard]] entt::entity createActor(const entt::id_type actor_name_id, const glm::vec2& position);
    [[nodiscard]] entt::entity createAnimal(const entt::id_type animal_name_id, const glm::vec2& position);
    [[nodiscard]] entt::entity createCrop(const entt::id_type crop_type_id, const glm::vec2& position, std::uint32_t planted_day);
    [[nodiscard]] entt::entity createSoilTile(const glm::vec2& position, entt::id_type rule_id, entt::id_type layer_id);
    entt::entity createResourcePickup(entt::id_type item_id,
                                      const engine::component::Sprite& sprite,
                                      const glm::vec2& start_pos,
                                      const glm::vec2& target_pos,
                                      float flight_time = 0.22f,
                                      float arc_height = 6.0f);

private:
    entt::entity createMobBase(entt::id_type name_id,
                               const std::string& display_name,
                               float speed,
                               const SpriteBlueprint& sprite,
                               const SoundBlueprint& sounds,
                               const std::unordered_map<entt::id_type, AnimationBlueprint>& animations,
                               const glm::vec2& position);

    void addTransformComponent(entt::entity entity, glm::vec2 position, glm::vec2 scale = glm::vec2(1.0f), float rotation = 0.0f);
    void addSpriteComponent(entt::entity entity, const SpriteBlueprint& sprite);
    void addOneAnimationComponent(entt::entity entity, const AnimationBlueprint& animation_blueprint, entt::id_type animation_id, bool loop = false);
    void addAnimationComponent(entt::entity entity, 
        const std::unordered_map<entt::id_type, AnimationBlueprint>& animation_blueprints, 
        entt::id_type default_animation_id = entt::null);
    void addAudioComponent(entt::entity entity, const SoundBlueprint& sounds);
    void addCropComponent(entt::entity entity, game::defs::CropType crop_type, std::uint32_t planted_day, int initial_countdown);
};

} // namespace game::factory
