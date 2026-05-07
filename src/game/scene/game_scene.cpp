#include "game_scene.h"

#include "battle_scene.h"
#include "game/battle/battle_unit_factory.h"
#include "game/runtime/game_runtime_assembler.h"
#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"
#include "game_scene_battle_settlement.h"
#include "inventory_menu_scene.h"
#include "pause_menu_scene.h"
#include "quest_offer_scene.h"
#include "recruit_offer_scene.h"
#include "title_scene.h"
#include "engine/audio/audio_player.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/component/tags.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/system/light_system.h"
#include "engine/system/render_system.h"
#include "engine/system/ysort_system.h"
#include "game/component/appearance_component.h"
#include "game/component/inventory_component.h"
#include "game/component/enemy_encounter_component.h"
#include "game/component/map_component.h"
#include "game/component/npc_component.h"
#include "game/component/party_component.h"
#include "game/component/tags.h"
#include "game/data/battle_background_id.h"
#include "game/data/game_time.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/audio_ids.h"
#include "game/defs/commands.h"
#include "game/domain/inventory_domain_service.h"
#include "game/save/save_service.h"
#include "engine/script/script_host.h"
#include "game/system/camera_follow_system.h"
#include "game/system/interaction_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/render_target_system.h"
#include "game/ui/game_input_prompt_overlay.h"
#include "game/ui/game_overlay.h"
#include "game/ui/game_scene_ui_controller.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"
#include "engine/vfx/vfx_service.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#include "engine/debug/panels/vfx_debug_panel.h"
#include "engine/system/debug_render_system.h"
#include "game/debug/battle_debug_panel.h"
#include "game/debug/blueprint_inspector_debug_panel.h"
#include "game/debug/game_time_debug_panel.h"
#include "game/debug/inventory_debug_panel.h"
#include "game/debug/map_inspector_debug_panel.h"
#include "game/debug/player_debug_panel.h"
#include "game/debug/quest_debug_panel.h"
#include "game/debug/scheduler_debug_panel.h"
#include "game/debug/scheduler_profiler.h"
#include "game/debug/save_load_debug_panel.h"
#include "game/debug/shop_debug_panel.h"
#endif

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/common.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace entt::literals;

namespace {
constexpr int MUSIC_FADE_IN_MS = 200;
constexpr std::uint8_t BATTLE_REWARD_NOTIFICATION_CHANNEL = 1;
constexpr std::string_view DEFAULT_PARTY_ACTOR_ID = "actor.player";

[[nodiscard]] std::unordered_map<entt::id_type, int> collectPlayerItemStocks(entt::registry& registry) {
    std::unordered_map<entt::id_type, int> stocks{};

    auto players = registry.view<game::component::PlayerTag, game::component::InventoryComponent>();
    if (players.begin() == players.end()) {
        return stocks;
    }

    const entt::entity player = *players.begin();
    const auto& inventory = players.get<game::component::InventoryComponent>(player);
    for (const auto& slot : inventory.slots_) {
        if (slot.empty() || slot.item_id_ == entt::null || slot.count_ <= 0) {
            continue;
        }
        stocks[slot.item_id_] += slot.count_;
    }

    return stocks;
}

[[nodiscard]] entt::entity findPlayer(entt::registry& registry) {
    auto players = registry.view<game::component::PlayerTag>();
    return players.begin() == players.end() ? entt::null : *players.begin();
}

[[nodiscard]] std::optional<game::scene::AppearanceSnapshot> capturePlayerAppearanceSnapshot(entt::registry& registry) {
    const entt::entity player = findPlayer(registry);
    if (player == entt::null) {
        return std::nullopt;
    }

    const auto* appearance = registry.try_get<game::component::AppearanceComponent>(player);
    if (!appearance) {
        return std::nullopt;
    }

    game::scene::AppearanceSnapshot snapshot{};
    snapshot.profile_id = appearance->profile_id_;
    snapshot.gender = appearance->gender_;
    snapshot.slot_variants = appearance->slot_variants_;
    snapshot.valid = true;
    return snapshot;
}

[[nodiscard]] std::vector<game::scene::BattleSpriteSeed>
buildBattleSpriteSeeds(entt::registry& registry, const std::vector<game::battle::BattleUnit>& units) {
    std::vector<game::scene::BattleSpriteSeed> seeds;
    seeds.reserve(units.size());

    const auto player_appearance = capturePlayerAppearanceSnapshot(registry);
    for (const auto& unit : units) {
        game::scene::BattleSpriteSeed seed{};
        seed.unit_id = unit.id;
        seed.source_actor_id = unit.source_actor_id;
        seed.source_enemy_id = unit.source_enemy_id;
        if (unit.source_actor_id && *unit.source_actor_id == DEFAULT_PARTY_ACTOR_ID && player_appearance) {
            seed.appearance = *player_appearance;
        }
        seeds.push_back(std::move(seed));
    }

    return seeds;
}

[[nodiscard]] std::vector<std::string> resolveBattleActorIds(entt::registry& registry,
                                                             const std::vector<std::string>& explicit_actor_ids) {
    if (!explicit_actor_ids.empty()) {
        return explicit_actor_ids;
    }

    const entt::entity player = findPlayer(registry);
    if (player != entt::null) {
        if (const auto* party = registry.try_get<game::component::PartyComponent>(player);
            party && !party->active_actor_ids_.empty()) {
            return party->active_actor_ids_;
        }
    }

    spdlog::warn("GameScene: 未找到有效 PartyComponent，战斗队伍回退为 actor.player。");
    return {std::string(DEFAULT_PARTY_ACTOR_ID)};
}

[[nodiscard]] std::string sanitizeBattleBackgroundId(std::string_view id, std::string_view source_label) {
    if (id.empty()) {
        return {};
    }
    if (game::data::isValidBattleBackgroundId(id)) {
        return std::string{id};
    }

    spdlog::warn("GameScene: {} battle_background_id='{}' 非法，已忽略。", source_label, id);
    return {};
}

[[nodiscard]] const game::data::TroopData* findTroopForBattleBackground(
    const game::data::RpgCatalog* rpg_catalog,
    const std::string& troop_id,
    const std::vector<game::battle::BattleUnit>& units) {
    if (!rpg_catalog) {
        return nullptr;
    }

    if (!troop_id.empty()) {
        return rpg_catalog->findTroop(troop_id);
    }

    // Prebuilt-unit debug entries may omit troop_id. Enemy-id reverse lookup is best-effort:
    // if multiple troops share an enemy, the first matching troop supplies only a presentation fallback.
    for (const auto& unit : units) {
        if (unit.side == game::battle::BattleSide::Enemy && unit.source_enemy_id) {
            for (const auto* troop : rpg_catalog->listTroops()) {
                if (std::ranges::any_of(troop->members_, [&unit](const game::data::TroopMemberData& member) {
                        return unit.source_enemy_id && member.enemy_id_ == *unit.source_enemy_id;
                    })) {
                    return troop;
                }
            }
            break;
        }
    }

    return nullptr;
}

[[nodiscard]] std::string resolveBattleBackgroundId(const game::defs::EnterBattleCommand& cmd,
                                                    const game::runtime::GameRuntimeServices* services,
                                                    const std::vector<game::battle::BattleUnit>& units) {
    if (auto resolved = sanitizeBattleBackgroundId(cmd.battle_background_id, "command"); !resolved.empty()) {
        return resolved;
    }

    if (cmd.encounter_context && services && services->world_state) {
        if (const auto* map_state = services->world_state->getMapState(cmd.encounter_context->map_id)) {
            if (map_state->info.battle_background_id) {
                if (auto resolved = sanitizeBattleBackgroundId(*map_state->info.battle_background_id, "map");
                    !resolved.empty()) {
                    return resolved;
                }
            }
        }
    }

    const auto* troop = findTroopForBattleBackground(
        services && services->rpg_catalog ? services->rpg_catalog.get() : nullptr,
        cmd.troop_id,
        units);
    if (troop) {
        if (auto resolved = sanitizeBattleBackgroundId(troop->battle_background_id_, "troop"); !resolved.empty()) {
            return resolved;
        }
    }

    return std::string{game::data::DEFAULT_BATTLE_BACKGROUND_ID};
}

}

namespace game::scene {

GameScene::GameScene(std::string_view name,
                     engine::core::Context& context,
                     std::shared_ptr<game::data::GameTime> game_time,
                     std::optional<int> load_slot)
    : engine::scene::Scene(name, context),
      services_(std::make_unique<game::runtime::GameRuntimeServices>()),
      systems_(std::make_unique<game::runtime::GameSystemBundle>()),
      scheduler_(std::make_unique<game::runtime::SystemScheduler>()),
#ifdef TF_ENABLE_DEBUG_UI
      scheduler_profiler_(std::make_unique<game::debug::SchedulerProfiler>()),
#endif
      game_time_(std::move(game_time)),
      load_slot_(load_slot) {
}

GameScene::~GameScene() noexcept {
    context_.getInputManager().onAction("inventory"_hs).disconnect<&GameScene::onInventoryToggle>(this);
    context_.getInputManager().onAction("hotbar"_hs).disconnect<&GameScene::onHotbarToggle>(this);
    context_.getInputManager().onAction("pause"_hs).disconnect<&GameScene::onPauseToggle>(this);
    context_.getInputManager().onAction("toggle_prompt_bar"_hs).disconnect<&GameScene::onTogglePromptBar>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>().disconnect<&GameScene::onHotbarChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarSlotChanged>().disconnect<&GameScene::onHotbarSlotChanged>(this);
    context_.getDispatcher().sink<game::defs::EnterBattleCommand>().disconnect<&GameScene::onEnterBattleCommand>(this);
    context_.getDispatcher().sink<game::defs::BattleEndedEvent>().disconnect<&GameScene::onBattleEnded>(this);
    context_.getDispatcher().sink<game::defs::QuestOfferRequestedEvent>().disconnect<&GameScene::onQuestOfferRequested>(this);
    context_.getDispatcher().sink<game::defs::RecruitOfferRequestedEvent>().disconnect<&GameScene::onRecruitOfferRequested>(this);
}

bool GameScene::init() {
    if (!game::runtime::GameRuntimeAssembler::assembleServices({
            *this,
            context_,
            registry_,
            game_time_,
            *services_})) {
        return false;
    }

    if (!game::runtime::GameRuntimeAssembler::assembleSystems({
            context_,
            registry_,
            *services_,
            *systems_})) {
        return false;
    }

    context_.getInputManager().pushContext(engine::input::InputContextId::Gameplay);
    context_pushed_ = true;
    bindSceneInputActions();

    if (!initUI()) {
        return false;
    }

#ifdef TF_ENABLE_DEBUG_UI
    if (!registerDebugPanels()) {
        return false;
    }
#endif

    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::HotbarChanged>().connect<&GameScene::onHotbarChanged>(this);
    dispatcher.sink<game::defs::HotbarSlotChanged>().connect<&GameScene::onHotbarSlotChanged>(this);
    dispatcher.sink<game::defs::EnterBattleCommand>().connect<&GameScene::onEnterBattleCommand>(this);
    dispatcher.sink<game::defs::BattleEndedEvent>().connect<&GameScene::onBattleEnded>(this);
    dispatcher.sink<game::defs::QuestOfferRequestedEvent>().connect<&GameScene::onQuestOfferRequested>(this);
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&GameScene::onRecruitOfferRequested>(this);

    if (load_slot_) {
        std::string load_error;
        if (!services_->save_service->loadFromFile(game::save::SaveService::slotPath(*load_slot_), load_error)) {
            const std::string message = "读档失败: " + load_error;
            spdlog::error("GameScene: 读档失败 (slot {}): {}", *load_slot_, load_error);
            requestReplaceScene(std::make_unique<game::scene::TitleScene>("TitleScene", context_, message));
            abort_to_title_ = true;
        }
    }

    if (abort_to_title_) {
        (void)Scene::init();
        return true;
    }

    auto player_view = registry_.view<game::component::PlayerTag>();
    if (!player_view.empty()) {
        const entt::entity player = *player_view.begin();
        dispatcher.enqueue<game::defs::InventorySyncCommand>(player);
        dispatcher.enqueue<game::defs::HotbarSyncCommand>(player);
    }

    context_.getGameState().setState(engine::core::State::Playing);

    if (!Scene::init()) {
        return false;
    }

    context_.getAudioPlayer().playMusic(game::defs::audio::SCENE_BG_MUSIC_ID.value(), true, MUSIC_FADE_IN_MS);
    return true;
}

void GameScene::fixedUpdate(float delta_time) {
    if (abort_to_title_) {
        return;
    }

    snapshotInterpolationState();

    if (scheduler_) {
        const auto tick_result = scheduler_->tick({
            .mode = game_mode_,
            .systems = *systems_,
            .registry = registry_,
            .dispatcher = &context_.getDispatcher(),
            .delta_time = delta_time
        });

#ifdef TF_ENABLE_DEBUG_UI
        if (scheduler_profiler_ && scheduler_profiler_->isEnabled()) {
            scheduler_profiler_->captureFrame(
                game_mode_,
                tick_result,
                spdlog::should_log(spdlog::level::trace));
        }
#endif
    }
}

void GameScene::update(float delta_time) {
    // GameScene 的 frame update 仅承载 UI/表现层更新；
    // gameplay scheduler 已迁移到 fixedUpdate。
    if (!abort_to_title_) {
        updateBattleRewardNotification(delta_time);
    }
    if (!abort_to_title_ && services_ && services_->vfx_service) {
        services_->vfx_service->update(delta_time);
    }
    if (!abort_to_title_ && input_prompt_overlay_) {
        input_prompt_overlay_->update();
    }
    if (!abort_to_title_ && ui_controller_) {
        ui_controller_->update(delta_time);
    }
    Scene::update(delta_time);
}

void GameScene::prepareUi(float interpolation_alpha) {
    if (!isInitialized() || abort_to_title_) {
        return;
    }

    auto& camera = context_.getCamera();
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);
    const glm::vec2 camera_position_before = camera.getPosition();
    if (has_previous_camera_position_) {
        const glm::vec2 ui_camera_position =
            glm::mix(previous_camera_position_, camera_position_before, clamped_alpha);
        camera.setPosition(ui_camera_position);
    }

    // 这里传入 clamped_alpha 是为了插值 world anchor 本身；
    // 相机插值已经在上面通过临时 camera 位置完成，两者作用对象不同。
    if (ui_controller_) {
        ui_controller_->refreshAnchoredWidgets(camera, clamped_alpha);
    }

    if (has_previous_camera_position_) {
        camera.setPosition(camera_position_before);
    }
}

void GameScene::render(float interpolation_alpha) {
    if (abort_to_title_) {
        Scene::render(interpolation_alpha);
        return;
    }

    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);
    const glm::vec2 camera_position_before = camera.getPosition();
    if (has_previous_camera_position_) {
        const glm::vec2 render_camera_position =
            glm::mix(previous_camera_position_, camera_position_before, clamped_alpha);
        camera.setPosition(render_camera_position);
    }

    systems_->ysort_system->render(registry_, clamped_alpha);
    systems_->render_system->render(registry_, renderer, camera, clamped_alpha);
    systems_->light_system->render(registry_, renderer, clamped_alpha);
    systems_->render_target_system->render(renderer);
#ifdef TF_ENABLE_DEBUG_UI
    if (systems_->debug_render_system) {
        systems_->debug_render_system->render(registry_, renderer);
    }
#endif

    Scene::render(interpolation_alpha);

    if (has_previous_camera_position_) {
        camera.setPosition(camera_position_before);
    }
}

void GameScene::snapshotInterpolationState() {
    auto view = registry_.view<engine::component::TransformComponent>();
    for (auto entity : view) {
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        transform.previous_position_ = transform.position_;
    }

    previous_camera_position_ = context_.getCamera().getPosition();
    has_previous_camera_position_ = true;
}

void GameScene::clean() {
    context_.getGLRenderer().setVfxBackend(nullptr);

    if (services_ && services_->script_host) {
        services_->script_host->shutdown();
        services_->script_host.reset();
    }
#ifdef TF_ENABLE_DEBUG_UI
    if (auto* vfx_panel = context_.getDebugUIManager().getPanel<engine::debug::VfxDebugPanel>(
            engine::debug::PanelCategory::Engine)) {
        vfx_panel->clearVfxService();
        vfx_panel->clearPlayerPositionProvider();
    }
    context_.getDebugUIManager().unregisterPanels(engine::debug::PanelCategory::Game);
#endif
    if (systems_ && systems_->map_transition_system) {
        systems_->map_transition_system->setFadeOverlay(nullptr);
    }
    game_overlay_.reset();
    input_prompt_overlay_.reset();
    ui_controller_.reset();
    has_previous_camera_position_ = false;
    previous_camera_position_ = glm::vec2{0.0f, 0.0f};
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

void GameScene::setGameMode(game::runtime::GameMode mode) {
    game_mode_ = mode;
}

void GameScene::bindSceneInputActions() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("inventory"_hs).connect<&GameScene::onInventoryToggle>(this);
    input_manager.onAction("hotbar"_hs).connect<&GameScene::onHotbarToggle>(this);
    input_manager.onAction("pause"_hs).connect<&GameScene::onPauseToggle>(this);
    input_manager.onAction("toggle_prompt_bar"_hs).connect<&GameScene::onTogglePromptBar>(this);
}

#ifdef TF_ENABLE_DEBUG_UI
bool GameScene::registerDebugPanels() {
    auto& debug_ui_manager = context_.getDebugUIManager();
    auto& dispatcher = context_.getDispatcher();

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::PlayerDebugPanel>(registry_, dispatcher, services_->appearance_catalog.get()),
        false,
        engine::debug::PanelCategory::Game);

    if (services_->rpg_catalog) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::BattleDebugPanel>(
                registry_,
                dispatcher,
                services_->rpg_catalog.get()),
            false,
            engine::debug::PanelCategory::Game);
    }

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::GameTimeDebugPanel>(registry_, dispatcher),
        false,
        engine::debug::PanelCategory::Game);

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::InventoryDebugPanel>(registry_, dispatcher, services_->item_catalog.get()),
        false,
        engine::debug::PanelCategory::Game);

    if (services_->quest_catalog && services_->quest_turn_in_service) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::QuestDebugPanel>(
                registry_,
                services_->quest_catalog.get(),
                services_->item_catalog.get(),
                services_->quest_turn_in_service.get()),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (services_->shop_catalog && services_->shop_transaction_service) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::ShopDebugPanel>(
                context_,
                registry_,
                services_->shop_catalog.get(),
                services_->item_catalog.get(),
                services_->shop_transaction_service.get()),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (services_->save_service) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::SaveLoadDebugPanel>(*services_->save_service),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (services_->map_manager && services_->world_state) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::MapInspectorDebugPanel>(
                *services_->map_manager,
                *services_->world_state,
                registry_,
                context_.getSpatialIndexManager()),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (services_->blueprint_manager) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::BlueprintInspectorDebugPanel>(*services_->blueprint_manager),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (scheduler_profiler_) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::SchedulerDebugPanel>(*scheduler_profiler_, &game_mode_),
            false,
            engine::debug::PanelCategory::Game);
    }

    if (auto* vfx_panel = debug_ui_manager.getPanel<engine::debug::VfxDebugPanel>(
            engine::debug::PanelCategory::Engine)) {
        vfx_panel->setVfxService(services_ ? services_->vfx_service.get() : nullptr);
        vfx_panel->setPlayerPositionProvider([this]() -> std::optional<glm::vec2> {
            auto view = registry_.view<game::component::PlayerTag, engine::component::TransformComponent>();
            if (view.begin() == view.end()) {
                return std::nullopt;
            }
            const auto player = *view.begin();
            return view.get<engine::component::TransformComponent>(player).position_;
        });
    }

    spdlog::trace("游戏层调试面板注册完成。");
    return true;
}
#endif

bool GameScene::initUI() {
    ui_controller_ = std::make_unique<game::ui::GameSceneUiController>(
        context_,
        registry_,
        instance_id_,
        services_ ? services_->item_catalog.get() : nullptr);
    if (!ui_controller_ || !ui_controller_->init()) {
        spdlog::error("GameScene: 创建 GameSceneUiController 失败。");
        ui_controller_.reset();
        return false;
    }

    if (systems_ && systems_->map_transition_system) {
        systems_->map_transition_system->setFadeOverlay(ui_controller_->screenFade());
    }

    game_overlay_ = std::make_unique<game::ui::GameOverlay>(
        *context_.getRmlUi(),
        instance_id_,
        [this]() { (void)onPauseToggle(); });

    input_prompt_overlay_ = std::make_unique<game::ui::GameInputPromptOverlay>(
        *context_.getRmlUi(),
        context_.getInputManager(),
        instance_id_);

    spdlog::debug("GameScene: UI controller 初始化完成。");
    return true;
}

bool GameScene::onInventoryToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }

    if (systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return false;
    }

    auto player_view = registry_.view<game::component::PlayerTag>();
    if (player_view.begin() == player_view.end()) {
        return false;
    }
    const entt::entity player = *player_view.begin();

    requestPushScene(std::make_unique<game::scene::InventoryMenuScene>(
        "InventoryMenu",
        context_,
        registry_,
        player,
        services_->item_catalog.get(),
        services_->quest_catalog.get()));
    return true;
}

bool GameScene::onHotbarToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }

    if (systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return false;
    }

    if (ui_controller_) {
        return ui_controller_->toggleHotbar();
    }

    return false;
}

bool GameScene::onPauseToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }

    if (systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return false;
    }

    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    auto menu = std::make_unique<game::scene::PauseMenuScene>(
        "PauseMenu",
        context_,
        services_->save_service.get(),
        game_time);
    requestPushScene(std::move(menu));
    return true;
}

bool GameScene::onTogglePromptBar() {
    if (input_prompt_overlay_) {
        input_prompt_overlay_->toggleVisible();
        return true;
    }

    return false;
}

void GameScene::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (ui_controller_) {
        ui_controller_->applyHotbarChanged(evt);
    }
}

void GameScene::onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt) {
    if (ui_controller_) {
        ui_controller_->applyHotbarSlotChanged(evt);
    }
}

void GameScene::onEnterBattleCommand(const game::defs::EnterBattleCommand& cmd) {
    if (cmd.encounter_context && active_encounter_context_) {
        spdlog::warn("GameScene: 收到地图遭遇战斗请求，但已有未结算遭遇 encounter_id={}。",
                     active_encounter_context_->encounter_id);
        releaseEnemyEncounterEntryFailure(*cmd.encounter_context);
        return;
    }

    std::vector<game::battle::BattleUnit> units{};
    units.reserve(cmd.player_units.size() + cmd.enemy_units.size());
    units.insert(units.end(), cmd.player_units.begin(), cmd.player_units.end());
    units.insert(units.end(), cmd.enemy_units.begin(), cmd.enemy_units.end());

    const auto initial_item_stocks = collectPlayerItemStocks(registry_);

    game::battle::BattleSessionOptions session_options{};
    if (services_) {
        session_options.rpg_catalog = services_->rpg_catalog.get();
        session_options.item_catalog = services_->item_catalog.get();
    }
    session_options.item_stocks = initial_item_stocks;

    if (units.empty()) {
        if (!services_ || !services_->rpg_catalog) {
            spdlog::warn("GameScene: EnterBattleCommand 未提供单位，且 RPG catalog 不可用。");
            if (cmd.encounter_context) {
                releaseEnemyEncounterEntryFailure(*cmd.encounter_context);
            }
            return;
        }

        // GameScene 负责将 command 适配为 battle factory 的输入结构，
        // 保持 battle 层不依赖 defs::EnterBattleCommand。
        game::battle::BattleUnitBuildOptions build_options{};
        build_options.actor_ids = resolveBattleActorIds(registry_, cmd.actor_ids);
        build_options.troop_id = cmd.troop_id;
        std::string build_error{};
        if (!game::battle::buildBattleUnitsFromCatalog(*services_->rpg_catalog, build_options, units, build_error)) {
            spdlog::warn("GameScene: 无法从 RPG catalog 构造战斗单位: {}", build_error);
            if (cmd.encounter_context) {
                releaseEnemyEncounterEntryFailure(*cmd.encounter_context);
            }
            return;
        }
    }

    active_battle_initial_item_stocks_ = initial_item_stocks;
    has_active_battle_item_stocks_ = true;
    active_encounter_context_ = cmd.encounter_context;

    game::scene::BattleScenePresentationOptions presentation_options{};
    presentation_options.sprite_seeds = buildBattleSpriteSeeds(registry_, units);
    presentation_options.battle_background_id = resolveBattleBackgroundId(cmd, services_.get(), units);
    if (services_) {
        presentation_options.blueprint_manager = services_->blueprint_manager.get();
        presentation_options.appearance_catalog = services_->appearance_catalog.get();
    }

    requestPushScene(std::make_unique<game::scene::BattleScene>(
        "BattleScene",
        context_,
        std::move(units),
        std::move(session_options),
        std::move(presentation_options)));
}

void GameScene::onQuestOfferRequested(const game::defs::QuestOfferRequestedEvent& evt) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (systems_ && systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return;
    }
    if (!services_ || !services_->quest_catalog) {
        spdlog::warn("GameScene: QuestOfferRequestedEvent 收到后 QuestCatalog 不可用。");
        return;
    }

    const auto* quest = services_->quest_catalog->findQuest(evt.quest_id_hash);
    if (!quest) {
        spdlog::warn("GameScene: QuestOfferRequestedEvent quest_id='{}' 未在 QuestCatalog 中找到。", evt.quest_id);
        return;
    }

    requestPushScene(std::make_unique<game::scene::QuestOfferScene>(
        "QuestOfferScene",
        context_,
        registry_,
        evt.player,
        evt.giver,
        *quest,
        services_->item_catalog.get()));
}

void GameScene::onRecruitOfferRequested(const game::defs::RecruitOfferRequestedEvent& evt) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (systems_ && systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return;
    }
    if (!services_ || !services_->rpg_catalog) {
        spdlog::warn("GameScene: RecruitOfferRequestedEvent 收到后 RpgCatalog 不可用。");
        return;
    }

    const auto* actor = services_->rpg_catalog->findActor(evt.actor_id_hash);
    if (!actor) {
        spdlog::warn("GameScene: RecruitOfferRequestedEvent actor_id='{}' 未在 RpgCatalog 中找到。", evt.actor_id);
        return;
    }

    requestPushScene(std::make_unique<game::scene::RecruitOfferScene>(
        "RecruitOfferScene",
        context_,
        registry_,
        evt.player,
        evt.recruiter,
        *actor));
}

void GameScene::onBattleEnded(const game::defs::BattleEndedEvent& evt) {
    spdlog::info("GameScene: Battle ended, outcome={}, final_units={}.",
                 game::battle::toString(evt.outcome),
                 evt.final_units.size());
    resolveActiveEnemyEncounter(evt);
    game::scene::processBattleEndedForGameScene(
        registry_,
        context_.getDispatcher(),
        services_.get(),
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        battle_reward_notification_,
        evt);
}

void GameScene::releaseEnemyEncounterEntryFailure(const game::defs::EnemyEncounterBattleContext& context) {
    if (context.source_entity == entt::null || !registry_.valid(context.source_entity)) {
        return;
    }

    auto* encounter = registry_.try_get<game::component::EnemyEncounterComponent>(context.source_entity);
    if (!encounter || encounter->encounter_id_ != context.encounter_id) {
        return;
    }

    encounter->engaged_ = false;
    encounter->cooldown_timer_ = std::max(encounter->cooldown_timer_, encounter->cooldown_seconds_);
}

void GameScene::resolveActiveEnemyEncounter(const game::defs::BattleEndedEvent& evt) {
    if (!active_encounter_context_) {
        return;
    }

    const auto context = *active_encounter_context_;
    active_encounter_context_.reset();
    const bool victory = evt.outcome == game::battle::BattleOutcome::Victory;

    if (victory && context.once && services_ && services_->world_state) {
        if (auto* map_state = services_->world_state->getMapStateMutable(context.map_id)) {
            map_state->persistent.defeated_encounters.insert(context.encounter_id);
        }
    }

    if (context.source_entity == entt::null || !registry_.valid(context.source_entity)) {
        return;
    }

    auto* encounter = registry_.try_get<game::component::EnemyEncounterComponent>(context.source_entity);
    if (!encounter || encounter->encounter_id_ != context.encounter_id) {
        return;
    }

    if (victory && context.once) {
        encounter->defeated_ = true;
        if (context_.getSpatialIndexManager().isInitialized()) {
            context_.getSpatialIndexManager().removeColliderEntity(context.source_entity);
        }
        registry_.emplace_or_replace<engine::component::NeedRemoveTag>(context.source_entity);
        return;
    }

    encounter->engaged_ = false;
    encounter->cooldown_timer_ = std::max(encounter->cooldown_timer_, encounter->cooldown_seconds_);

    if (auto* transform = registry_.try_get<engine::component::TransformComponent>(context.source_entity)) {
        transform->position_ = context.home_position;
        registry_.emplace_or_replace<engine::component::TransformDirtyTag>(context.source_entity);
    }

    if (auto* velocity = registry_.try_get<engine::component::VelocityComponent>(context.source_entity)) {
        velocity->velocity_ = glm::vec2{0.0F, 0.0F};
    }

    if (auto* wander = registry_.try_get<game::component::WanderComponent>(context.source_entity)) {
        wander->home_position_ = context.home_position;
        wander->target_ = context.home_position;
        wander->phase_ = game::component::WanderPhase::Waiting;
        wander->wait_timer_ = encounter->cooldown_timer_;
        wander->stuck_timer_ = 0.0F;
    }

    if (context_.getSpatialIndexManager().isInitialized()) {
        context_.getSpatialIndexManager().updateColliderEntity(context.source_entity);
    }
}

void GameScene::updateBattleRewardNotification(const float delta_time) {
    game::system::helpers::updateTimedNotification(
        registry_,
        context_.getDispatcher(),
        BATTLE_REWARD_NOTIFICATION_CHANNEL,
        battle_reward_notification_,
        delta_time);
}

} // namespace game::scene
