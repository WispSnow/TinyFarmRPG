#include "rest_system.h"

#include "game/component/map_component.h"
#include "game/data/game_time.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/party_rest_service.h"
#include "game/runtime/localization_service.h"
#include "game/scene/rest_dialog_scene.h"
#include "game/system/system_helpers.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <memory>
#include <utility>
#include <vector>

namespace game::system {

RestSystem::RestSystem(entt::registry& registry,
                       engine::core::Context& context,
                       const game::data::RpgCatalog* rpg_catalog)
    : registry_(registry),
      context_(context),
      dispatcher_(context.getDispatcher()),
      rpg_catalog_(rpg_catalog) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&RestSystem::onInteractCommand>(this);
    dispatcher_.sink<game::defs::RestConfirmRequest>().connect<&RestSystem::onRestConfirmRequest>(this);
}

RestSystem::~RestSystem() {
    dispatcher_.disconnect(this);
}

void RestSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (event.player == entt::null || event.target == entt::null) {
        return;
    }
    if (!registry_.valid(event.target)) {
        return;
    }
    if (helpers::isScriptedInteraction(registry_, event.target)) {
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

    std::vector<game::domain::RestRecoveryPreview> recovery_previews{};
    if (rpg_catalog_) {
        recovery_previews.reserve(game::scene::RestDialogScene::MAX_REST_HOURS);
        for (int hours = game::scene::RestDialogScene::MIN_REST_HOURS;
             hours <= game::scene::RestDialogScene::MAX_REST_HOURS;
             ++hours) {
            recovery_previews.push_back(game::domain::PartyRestService::previewActivePartyRecovery(
                registry_,
                event.player,
                *rpg_catalog_,
                hours));
        }
    }

    auto** localization_ptr = registry_.ctx().find<game::runtime::LocalizationService*>();
    const auto* localization = localization_ptr ? *localization_ptr : nullptr;

    auto scene = std::make_unique<game::scene::RestDialogScene>(
        "RestDialog",
        context_,
        event.player,
        std::move(recovery_previews),
        localization);
    dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

void RestSystem::onRestConfirmRequest(const game::defs::RestConfirmRequest& event) {
    if (event.hours <= 0 || event.player == entt::null || !registry_.valid(event.player)) {
        return;
    }

    if (rpg_catalog_) {
        const auto result = game::domain::PartyRestService::applyActivePartyRecovery(
            registry_,
            event.player,
            *rpg_catalog_,
            event.hours);
        if (result.runtime_state_changed) {
            dispatcher_.trigger(game::defs::PartyRuntimeStatsChanged{
                .player = event.player,
                .actor_id = {},
                .full_sync = true,
            });
        }
    }

    dispatcher_.enqueue(game::defs::AdvanceTimeRequest{event.hours});
}

} // namespace game::system
