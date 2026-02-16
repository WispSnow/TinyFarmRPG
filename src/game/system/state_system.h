#pragma once

#include "game/component/state_component.h"
#include "engine/utils/events.h"
#include <array>
#include <entt/signal/dispatcher.hpp>
#include <entt/entity/registry.hpp>

namespace engine::component {
struct AnimationComponent;
}

namespace game::system {

/**
 * @brief 根据 StateComponent 状态，发送动画播放事件
 */
class StateSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

    static constexpr std::size_t ACTION_COUNT = static_cast<std::size_t>(game::component::Action::Count);
    static constexpr std::size_t DIRECTION_COUNT = static_cast<std::size_t>(game::component::Direction::Count);

    using AnimationLookupTable = std::array<std::array<entt::id_type, DIRECTION_COUNT>, ACTION_COUNT>;
    AnimationLookupTable animation_lookup_{};
    entt::id_type fallback_animation_id_{entt::null};

public:
    explicit StateSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~StateSystem();

    void update();

private:
    void buildAnimationLookup();
    [[nodiscard]] entt::id_type resolveAnimationId(game::component::Action action,
                                                   game::component::Direction direction,
                                                   const ::engine::component::AnimationComponent* anim_component) const;

    void onAnimationFinishedEvent(const ::engine::utils::AnimationFinishedEvent& event);
};

} // namespace game::system

