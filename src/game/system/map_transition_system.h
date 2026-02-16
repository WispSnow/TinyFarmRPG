#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include "game/component/map_component.h"
#include "engine/utils/math.h"

namespace game::world {
    class WorldState;
    class MapManager;
}

namespace engine::spatial {
    class CollisionResolver;
    enum class Direction;
}

namespace engine::ui {
    class UIScreenFade;
}

namespace game::system {

class MapTransitionSystem {
    enum class TransitionPhase {
        Idle,
        FadingOut,
        FadingIn
    };

    enum class TransitionType {
        None,
        Edge,
        Trigger
    };

    struct PendingTransition {
        TransitionType type{TransitionType::None};
        entt::entity player{entt::null};
        entt::id_type target_map_id{entt::null};
        glm::vec2 edge_spawn_pos{0.0f, 0.0f};
        int target_trigger_id{0};
        glm::vec2 fallback_spawn_pos{0.0f, 0.0f};
    };

    entt::registry& registry_;
    game::world::WorldState& world_state_;
    game::world::MapManager& map_manager_;
    engine::spatial::CollisionResolver* collision_resolver_{nullptr};
    float edge_offset_{8.0f};
    engine::ui::UIScreenFade* fade_{nullptr};
    TransitionPhase transition_phase_{TransitionPhase::Idle};
    PendingTransition pending_{};
    float fade_seconds_{0.12f};

public:
    MapTransitionSystem(entt::registry& registry,
                        game::world::WorldState& world_state,
                        game::world::MapManager& map_manager,
                        engine::spatial::CollisionResolver* collision_resolver,
                        float edge_offset = 8.0f);

    void update();
    void setFadeOverlay(engine::ui::UIScreenFade* fade) { fade_ = fade; }
    [[nodiscard]] bool isTransitionActive() const { return transition_phase_ != TransitionPhase::Idle; }

private:
    bool handleEdgeTransition(entt::entity player, const glm::vec2& pos);
    bool handleTriggerTransition(entt::entity player, const glm::vec2& pos);
    
    // Compute safe spawn position, searching nearby if blocked
    glm::vec2 findSafeSpawnPosition(entt::entity player, glm::vec2 target_pos);

    glm::vec2 computeEdgeSpawnPos(engine::spatial::Direction dir, const glm::vec2& pos, const glm::vec2& target_size) const;
    glm::vec2 computeOffsetPosition(const engine::utils::Rect& rect, game::component::StartOffset offset) const;
    glm::vec2 clampToMap(const glm::vec2& pos, const glm::vec2& size) const;

    void beginTransition(PendingTransition pending);
    void updateTransition();
    void finishTransition();
    bool commitPendingTransition();
};

} // namespace game::system
