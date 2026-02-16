#pragma once
#include "game/defs/events.h"
#include "game/component/resource_node_component.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <string_view>

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace game::factory {
    class EntityFactory;
    class BlueprintManager;
}

namespace game::data {
    class ItemCatalog;
}

namespace game::system {

class FarmSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::spatial::SpatialIndexManager& spatial_index_;
    game::factory::EntityFactory& entity_factory_;
    const game::factory::BlueprintManager& blueprint_manager_;
    game::data::ItemCatalog* item_catalog_{nullptr};

public:
    FarmSystem(entt::registry& registry,
               entt::dispatcher& dispatcher,
               engine::spatial::SpatialIndexManager& spatial_index,
               game::factory::EntityFactory& entity_factory,
               const game::factory::BlueprintManager& blueprint_manager,
               game::data::ItemCatalog* item_catalog);
    ~FarmSystem();

private:
    void onUseToolEvent(const game::defs::UseToolEvent& event);
    void onUseSeedEvent(const game::defs::UseSeedEvent& event);

    /**
     * @brief 添加耕地自动图块（soil_tilled）
     * @param world_pos 目标自动图块世界位置
     * @return 是否成功添加
     */
    bool addTilledEntity(const glm::vec2& world_pos);

    /**
     * @brief 添加湿润耕地自动图块（soil_wet）
     * @param world_pos 目标自动图块世界位置
     * @return 是否成功添加
     */
    bool addWetEntity(const glm::vec2& world_pos);

    /**
     * @brief 种植种子
     * @param world_pos 目标世界位置
     * @param seed_type 种子类型
     * @return 是否成功种植
     */
    bool plantSeed(const glm::vec2& world_pos, game::defs::CropType seed_type);

    /**
     * @brief 收获作物
     * @param world_pos 目标世界位置
     * @return 是否成功收获
     */
    bool harvestCrop(const glm::vec2& world_pos);

    bool hitRock(const glm::vec2& world_pos);
    bool hitTree(const glm::vec2& world_pos);

    /// Shared helper for hitRock / hitTree: handles the hit, animation, drop, and removal
    /// after the caller has already located the target entity.
    /// @tparam CleanupFn  void(entt::entity target, const glm::vec2& world_pos)
    template <typename CleanupFn>
    bool hitResourceNode(entt::entity target,
                         const glm::vec2& world_pos,
                         entt::id_type sound_id,
                         entt::id_type default_anim_id,
                         std::string_view not_found_msg,
                         std::string_view missing_component_msg,
                         std::string_view destroyed_msg,
                         CleanupFn cleanup);

    void dropResource(const game::component::ResourceNodeComponent& node, const glm::vec2& world_pos);
};

} // namespace game::system
