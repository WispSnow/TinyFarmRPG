#pragma once

#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/system/deferred_commands.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"

#include <entt/entity/entity.hpp>

namespace game::system::wander_runtime {

void stopMovement(game::component::WanderComponent& wander,
                  engine::component::VelocityComponent& velocity);

void tickMovement(game::component::WanderComponent& wander,
                  const engine::component::TransformComponent& transform,
                  engine::component::VelocityComponent& velocity,
                  game::component::StateComponent& state,
                  float speed,
                  float delta_time,
                  entt::entity entity,
                  engine::system::DeferredCommands& deferred);

} // namespace game::system::wander_runtime
