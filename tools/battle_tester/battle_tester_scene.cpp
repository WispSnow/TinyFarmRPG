#include "battle_tester_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/renderer.h"
#include "game/battle/battle_session.h"
#include "game/battle/battle_unit_factory.h"
#include "game/defs/events.h"
#include "game/runtime/gameplay_camera_defaults.h"
#include "game/runtime/rpg_catalog_loader.h"
#include "game/scene/battle_scene.h"
#include "game/scene/battle_scene_entry.h"

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <utility>

namespace tools::battle_tester {

namespace {

constexpr std::string_view kIconConfigPath{"assets/data/icon_config.json"};
constexpr std::string_view kItemConfigPath{"assets/data/item_config.json"};
constexpr std::string_view kActorBlueprintPath{"assets/data/actor_blueprint.json"};
constexpr std::string_view kAppearanceCatalogPath{"assets/data/appearance_catalog.json"};
constexpr std::string_view kBattleItemPotion{"potion"};

[[nodiscard]] entt::id_type hashId(std::string_view id) {
    return entt::hashed_string{id.data(), id.size()}.value();
}

} // namespace

BattleTesterScene::BattleTesterScene(std::string_view name,
                                     engine::core::Context& context,
                                     BattleTesterConfig config)
    : engine::scene::Scene(name, context),
      config_(std::move(config)) {}

BattleTesterScene::~BattleTesterScene() = default;

bool BattleTesterScene::init() {
    if (!Scene::init()) {
        return false;
    }

    context_.getGLRenderer().setDebugUIEnabled(false);
    game::runtime::applyGameplayCameraDefaults(context_.getCamera());
    context_.getRenderer().setClearColorFloat(engine::utils::FColor{0.06F, 0.08F, 0.12F, 1.0F});
    context_.getDispatcher().sink<game::defs::BattleEndedEvent>().connect<&BattleTesterScene::onBattleEnded>(this);

    if (!loadResources()) {
        return false;
    }

    launch_requested_ = true;
    spdlog::info("BattleTester: initialized with {} actor(s), troop='{}', battle_background='{}', potion_count={}.",
                 config_.actor_ids.size(),
                 config_.troop_id,
                 config_.battle_background_id,
                 config_.potion_count);
    return true;
}

void BattleTesterScene::update(float delta_time) {
    Scene::update(delta_time);

    if (launch_requested_) {
        launch_requested_ = false;
        if (!launchBattle()) {
            quit();
        }
        return;
    }

    handleRootShortcuts();
}

void BattleTesterScene::clean() {
    context_.getDispatcher().sink<game::defs::BattleEndedEvent>().disconnect<&BattleTesterScene::onBattleEnded>(this);
    battle_active_ = false;
    launch_requested_ = false;
    Scene::clean();
}

bool BattleTesterScene::loadResources() {
    if (!item_catalog_.loadIconConfig(kIconConfigPath)) {
        spdlog::error("BattleTester: failed to load icon config '{}'.", kIconConfigPath);
        return false;
    }
    if (!item_catalog_.loadItemConfig(kItemConfigPath)) {
        spdlog::error("BattleTester: failed to load item config '{}'.", kItemConfigPath);
        return false;
    }

    game::runtime::RpgCatalogLoadOptions rpg_options{};
    rpg_options.item_catalog = &item_catalog_;
    std::string rpg_error{};
    if (!game::runtime::loadRpgCatalogFromManifest(rpg_catalog_, rpg_options, rpg_error)) {
        spdlog::error("BattleTester: {}", rpg_error);
        return false;
    }

    if (!blueprint_manager_.loadActorBlueprints(kActorBlueprintPath)) {
        spdlog::error("BattleTester: failed to load actor blueprints '{}'.", kActorBlueprintPath);
        return false;
    }

    if (!appearance_catalog_.loadFromFile(kAppearanceCatalogPath)) {
        spdlog::error("BattleTester: failed to load appearance catalog '{}'.", kAppearanceCatalogPath);
        return false;
    }

    return true;
}

bool BattleTesterScene::launchBattle() {
    auto battle_scene = createBattleScene();
    if (!battle_scene) {
        return false;
    }

    battle_active_ = true;
    requestPushScene(std::move(battle_scene));
    return true;
}

std::unique_ptr<engine::scene::Scene> BattleTesterScene::createBattleScene() {
    game::battle::BattleUnitBuildOptions build_options{};
    build_options.actor_ids = config_.actor_ids;
    build_options.troop_id = config_.troop_id;

    std::vector<game::battle::BattleUnit> units{};
    std::string build_error{};
    if (!game::battle::buildBattleUnitsFromCatalog(rpg_catalog_, build_options, units, build_error)) {
        spdlog::error("BattleTester: failed to build battle units: {}", build_error);
        return nullptr;
    }

    game::battle::BattleSessionOptions session_options{};
    session_options.rpg_catalog = &rpg_catalog_;
    session_options.item_catalog = &item_catalog_;
    if (config_.potion_count > 0) {
        const entt::id_type potion_id = hashId(kBattleItemPotion);
        if (item_catalog_.findItem(potion_id)) {
            session_options.item_stocks[potion_id] = config_.potion_count;
        } else {
            spdlog::warn("BattleTester: default battle item '{}' is missing from ItemCatalog.", kBattleItemPotion);
        }
    }

    game::scene::BattleScenePresentationOptions presentation_options{};
    presentation_options.blueprint_manager = &blueprint_manager_;
    presentation_options.appearance_catalog = &appearance_catalog_;
    presentation_options.battle_background_id = config_.battle_background_id;
    presentation_options.sprite_seeds = game::scene::buildBattleSpriteSeeds(
        units,
        game::scene::defaultBattleAppearanceSnapshot(appearance_catalog_));

    spdlog::info("BattleTester: launching battle with {} unit(s), troop='{}'.", units.size(), config_.troop_id);
    return std::make_unique<game::scene::BattleScene>(
        "BattleScene",
        context_,
        std::move(units),
        std::move(session_options),
        std::move(presentation_options));
}

void BattleTesterScene::handleRootShortcuts() {
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    if (!keyboard_state) {
        return;
    }

    const bool restart_pressed = keyboard_state[SDL_SCANCODE_R] != 0;
    if (restart_pressed && !previous_restart_pressed_ && !battle_active_) {
        launch_requested_ = true;
    }
    previous_restart_pressed_ = restart_pressed;

    const bool escape_pressed = keyboard_state[SDL_SCANCODE_ESCAPE] != 0;
    if (escape_pressed && !previous_escape_pressed_ && !battle_active_) {
        quit();
    }
    previous_escape_pressed_ = escape_pressed;
}

void BattleTesterScene::onBattleEnded(const game::defs::BattleEndedEvent& evt) {
    battle_active_ = false;
    last_outcome_ = evt.outcome;
    spdlog::info("BattleTester: battle ended outcome={}, final_units={}, remaining_item_stocks={}. Press R to restart.",
                 game::battle::toString(evt.outcome),
                 evt.final_units.size(),
                 evt.remaining_item_stocks.size());
}

} // namespace tools::battle_tester
