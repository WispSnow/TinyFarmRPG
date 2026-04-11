#include "game_scene.h"

#include "battle_scene.h"
#include "game/battle/battle_unit_factory.h"
#include "game/runtime/game_runtime_assembler.h"
#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"
#include "game_scene_battle_settlement.h"
#include "inventory_menu_scene.h"
#include "pause_menu_scene.h"
#include "title_scene.h"
#include "engine/audio/audio_player.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/system/light_system.h"
#include "engine/system/render_system.h"
#include "engine/system/ysort_system.h"
#include "game/component/inventory_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
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
#endif

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/common.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace entt::literals;

namespace {
constexpr int MUSIC_FADE_IN_MS = 200;
constexpr std::uint8_t BATTLE_REWARD_NOTIFICATION_CHANNEL = 1;

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
            return;
        }

        // GameScene 负责将 command 适配为 battle factory 的输入结构，
        // 保持 battle 层不依赖 defs::EnterBattleCommand。
        game::battle::BattleUnitBuildOptions build_options{};
        build_options.actor_ids = cmd.actor_ids;
        build_options.troop_id = cmd.troop_id;
        std::string build_error{};
        if (!game::battle::buildBattleUnitsFromCatalog(*services_->rpg_catalog, build_options, units, build_error)) {
            spdlog::warn("GameScene: 无法从 RPG catalog 构造战斗单位: {}", build_error);
            return;
        }
    }

    active_battle_initial_item_stocks_ = initial_item_stocks;
    has_active_battle_item_stocks_ = true;

    requestPushScene(std::make_unique<game::scene::BattleScene>(
        "BattleScene",
        context_,
        std::move(units),
        std::move(session_options)));
}

void GameScene::onBattleEnded(const game::defs::BattleEndedEvent& evt) {
    spdlog::info("GameScene: Battle ended, outcome={}, final_units={}.",
                 game::battle::toString(evt.outcome),
                 evt.final_units.size());
    game::scene::processBattleEndedForGameScene(
        registry_,
        context_.getDispatcher(),
        services_.get(),
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        battle_reward_notification_,
        evt);
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
