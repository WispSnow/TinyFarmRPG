#include "npc_wander_system.h"

#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/system/wander_runtime.h"

#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"

#include <entt/entity/registry.hpp>

namespace game::system {

NPCWanderSystem::NPCWanderSystem(entt::registry& registry)
    : registry_(registry) {}

void NPCWanderSystem::update(const float delta_time, engine::system::DeferredCommands& deferred) {
    auto view = registry_.view<game::component::NPCTag,
                               game::component::WanderComponent,
                               engine::component::TransformComponent,
                               engine::component::VelocityComponent,
                               game::component::ActorComponent,
                               game::component::StateComponent>(entt::exclude<game::component::AnimalTag>);

    for (const auto entity : view) {
        auto& wander = view.get<game::component::WanderComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        auto& velocity = view.get<engine::component::VelocityComponent>(entity);
        auto& actor = view.get<game::component::ActorComponent>(entity);
        auto& state = view.get<game::component::StateComponent>(entity);

        const auto* sleep = registry_.try_get<game::component::SleepRoutine>(entity);
        const bool is_sleeping = sleep && sleep->is_sleeping_;
        const auto* dialogue = registry_.try_get<game::component::DialogueComponent>(entity);
        const bool in_dialogue = dialogue && dialogue->active_;

        if (is_sleeping || in_dialogue) {
            wander_runtime::stopMovement(wander, velocity);
            if (state.action_ != game::component::Action::Idle) {
                state.action_ = game::component::Action::Idle;
                deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
            }
            continue;
        }

        wander_runtime::tickMovement(wander,
                                     transform,
                                     velocity,
                                     state,
                                     actor.speed_,
                                     delta_time,
                                     entity,
                                     deferred);
    }
}

} // namespace game::system
