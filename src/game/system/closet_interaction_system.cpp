#include "game/system/closet_interaction_system.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"
#include "game/component/map_component.h"
#include "game/defs/commands.h"
#include "game/scene/appearance_customize_scene.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace game::system {

ClosetInteractionSystem::ClosetInteractionSystem(entt::registry& registry,
                                                 engine::core::Context& context,
                                                 std::shared_ptr<game::data::AppearanceCatalog> appearance_catalog)
    : registry_(registry),
      context_(context),
      dispatcher_(context.getDispatcher()),
      appearance_catalog_(std::move(appearance_catalog)) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&ClosetInteractionSystem::onInteractCommand>(this);
}

ClosetInteractionSystem::~ClosetInteractionSystem() {
    dispatcher_.disconnect(this);
}

void ClosetInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (event.player == entt::null || event.target == entt::null) {
        return;
    }
    if (!registry_.valid(event.player) || !registry_.valid(event.target)) {
        return;
    }
    if (!registry_.any_of<game::component::ClosetArea>(event.target)) {
        return;
    }
    if (!appearance_catalog_) {
        spdlog::warn("ClosetInteractionSystem: AppearanceCatalog 不可用，忽略衣柜交互。");
        return;
    }

    auto scene = std::make_unique<game::scene::AppearanceCustomizeScene>(
        "AppearanceCustomize",
        context_,
        registry_,
        event.player,
        appearance_catalog_);
    dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace game::system
