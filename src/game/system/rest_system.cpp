#include "rest_system.h"

#include "game/component/map_component.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/scene/rest_dialog_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace game::system {

RestSystem::RestSystem(entt::registry& registry, engine::core::Context& context)
    : registry_(registry),
      context_(context),
      dispatcher_(context.getDispatcher()) {
    dispatcher_.sink<game::defs::InteractRequest>().connect<&RestSystem::onInteractRequest>(this);
}

RestSystem::~RestSystem() {
    dispatcher_.disconnect(this);
}

void RestSystem::onInteractRequest(const game::defs::InteractRequest& event) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (event.player == entt::null || event.target == entt::null) {
        return;
    }
    if (!registry_.valid(event.target)) {
        return;
    }
    if (!registry_.any_of<game::component::RestArea>(event.target)) {
        return;
    }

    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::warn("RestSystem: GameTime not found in registry context; rest action ignored");
        return;
    }

    auto scene = std::make_unique<game::scene::RestDialogScene>("RestDialog", context_);
    dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace game::system
