#include "game_scene.h"

#include "battle_scene.h"
#include "game/runtime/game_runtime_assembler.h"
#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"
#include "pause_menu_scene.h"
#include "title_scene.h"
#include "engine/audio/audio_player.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_defaults.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_screen_fade.h"
#include "engine/system/light_system.h"
#include "engine/system/render_system.h"
#include "engine/vfx/vfx_service.h"
#include "engine/system/ysort_system.h"
#include "game/component/hotbar_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/defs/audio_ids.h"
#include "game/defs/commands.h"
#include "game/save/save_service.h"
#ifdef TF_ENABLE_SCRIPTING
#include "engine/script/script_host.h"
#endif
#include "game/system/camera_follow_system.h"
#include "game/system/interaction_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/render_target_system.h"
#include "game/ui/dialogue_bubble_controller.h"
#include "game/ui/dialogue_bubble_view.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/inventory_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/time_clock_ui.h"
#include "game/world/map_manager.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#include "engine/debug/panels/vfx_debug_panel.h"
#include "engine/system/debug_render_system.h"
#include "game/debug/blueprint_inspector_debug_panel.h"
#include "game/debug/game_time_debug_panel.h"
#include "game/debug/inventory_debug_panel.h"
#include "game/debug/map_inspector_debug_panel.h"
#include "game/debug/player_debug_panel.h"
#include "game/debug/scheduler_debug_panel.h"
#include "game/debug/scheduler_profiler.h"
#include "game/debug/save_load_debug_panel.h"
#endif

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/common.hpp>
#include <vector>

using namespace entt::literals;

namespace {
constexpr int MUSIC_FADE_IN_MS = 200;
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
    context_.getDispatcher().sink<game::defs::HotbarChanged>().disconnect<&GameScene::onHotbarChanged>(this);
    context_.getDispatcher().sink<game::defs::InventoryChanged>().disconnect<&GameScene::onInventoryChanged>(this);
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
    dispatcher.sink<game::defs::InventoryChanged>().connect<&GameScene::onInventoryChanged>(this);
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
    if (!abort_to_title_ && services_ && services_->vfx_service) {
        services_->vfx_service->update(delta_time);
    }
    Scene::update(delta_time);
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

#ifdef TF_ENABLE_SCRIPTING
    if (services_ && services_->script_host) {
        services_->script_host->shutdown();
        services_->script_host.reset();
    }
#endif
#ifdef TF_ENABLE_DEBUG_UI
    if (auto* vfx_panel = context_.getDebugUIManager().getPanel<engine::debug::VfxDebugPanel>(
            engine::debug::PanelCategory::Engine)) {
        vfx_panel->clearVfxService();
        vfx_panel->clearPlayerPositionProvider();
    }
    context_.getDebugUIManager().unregisterPanels(engine::debug::PanelCategory::Game);
#endif
    auto& dispatcher = context_.getDispatcher();
    dispatcher.clear<game::defs::DialogueShowEvent>();
    dispatcher.clear<game::defs::DialogueMoveEvent>();
    dispatcher.clear<game::defs::DialogueHideEvent>();

    dialogue_controller_.reset();
    has_previous_camera_position_ = false;
    previous_camera_position_ = glm::vec2{0.0f, 0.0f};
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
}

#ifdef TF_ENABLE_DEBUG_UI
bool GameScene::registerDebugPanels() {
    auto& debug_ui_manager = context_.getDebugUIManager();
    auto& dispatcher = context_.getDispatcher();

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::PlayerDebugPanel>(registry_, dispatcher, services_->appearance_catalog.get()),
        false,
        engine::debug::PanelCategory::Game);

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::GameTimeDebugPanel>(registry_, dispatcher),
        false,
        engine::debug::PanelCategory::Game);

    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::InventoryDebugPanel>(registry_, dispatcher, services_->item_catalog.get()),
        false,
        engine::debug::PanelCategory::Game);

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
    const auto logical_size = context_.getGameState().getLogicalSize();
    constexpr entt::id_type DIALOGUE_BUBBLE_CH0_ID = "dialogue_bubble_ch0"_hs;
    constexpr entt::id_type DIALOGUE_BUBBLE_CH1_ID = "dialogue_bubble_ch1"_hs;
    constexpr entt::id_type DIALOGUE_BUBBLE_CH2_ID = "dialogue_bubble_ch2"_hs;

    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);
    auto& text_renderer = context_.getTextRenderer();

    auto time_clock_ui = std::make_unique<game::ui::TimeClockUI>(context_, registry_, text_renderer);
    ui_manager_->addElement(std::move(time_clock_ui));

    auto inventory_ui = std::make_unique<game::ui::InventoryUI>(context_, services_->item_catalog.get());
    inventory_ui_ = inventory_ui.get();
    ui_manager_->addElement(std::move(inventory_ui));

    auto hotbar_ui = std::make_unique<game::ui::HotbarUI>(context_, services_->item_catalog.get());
    hotbar_ui_ = hotbar_ui.get();
    ui_manager_->addElement(std::move(hotbar_ui));

    auto item_tooltip_ui = std::make_unique<game::ui::ItemTooltipUI>(context_);
    item_tooltip_ui->setOrderIndex(1000);
    item_tooltip_ui_ = item_tooltip_ui.get();
    ui_manager_->addElement(std::move(item_tooltip_ui));

    auto& dispatcher_ref = context_.getDispatcher();
    dialogue_controller_ = std::make_unique<game::ui::DialogueBubbleController>(dispatcher_ref);

    auto dialogue_bubble_ch0 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
    auto* dialogue_bubble_ch0_ptr = dialogue_bubble_ch0.get();
    dialogue_bubble_ch0->setId(DIALOGUE_BUBBLE_CH0_ID);
    ui_manager_->addElement(std::move(dialogue_bubble_ch0));

    auto dialogue_bubble_ch1 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
    auto* dialogue_bubble_ch1_ptr = dialogue_bubble_ch1.get();
    dialogue_bubble_ch1->setId(DIALOGUE_BUBBLE_CH1_ID);
    ui_manager_->addElement(std::move(dialogue_bubble_ch1));

    auto dialogue_bubble_ch2 = std::make_unique<game::ui::DialogueBubbleView>(context_, text_renderer);
    auto* dialogue_bubble_ch2_ptr = dialogue_bubble_ch2.get();
    dialogue_bubble_ch2->setId(DIALOGUE_BUBBLE_CH2_ID);
    ui_manager_->addElement(std::move(dialogue_bubble_ch2));

    dialogue_controller_->registerBubble(0, dialogue_bubble_ch0_ptr);
    dialogue_controller_->registerBubble(1, dialogue_bubble_ch1_ptr);
    dialogue_controller_->registerBubble(2, dialogue_bubble_ch2_ptr, {0.0F, -56.0F});

    if (inventory_ui_) {
        inventory_ui_->setUIManager(ui_manager_.get());
    }

    if (hotbar_ui_) {
        hotbar_ui_->setUIManager(ui_manager_.get());
    }

    if (item_tooltip_ui_) {
        if (inventory_ui_) {
            inventory_ui_->setTooltipUI(item_tooltip_ui_);
        }
        if (hotbar_ui_) {
            hotbar_ui_->setTooltipUI(item_tooltip_ui_);
        }
    }

    auto player_view = registry_.view<game::component::PlayerTag>();
    if (!player_view.empty()) {
        const entt::entity player = *player_view.begin();
        if (inventory_ui_) {
            inventory_ui_->setTarget(player);
        }
        if (hotbar_ui_) {
            hotbar_ui_->setTarget(player);
        }
    }

    if (inventory_ui_ && hotbar_ui_) {
        inventory_ui_->setHotbarUI(hotbar_ui_);
        hotbar_ui_->setInventoryUI(inventory_ui_);
    }

    auto menu_button = engine::ui::UIButton::create(
        context_,
        "menu",
        glm::vec2{-10.0f, 10.0f},
        glm::vec2{32.0f, 32.0f},
        [this]() { (void)onPauseToggle(); });
    if (menu_button) {
        menu_button->setAnchor({1.0f, 0.0f}, {1.0f, 0.0f});
        menu_button->setPivot({1.0f, 0.0f});
        ui_manager_->addElement(std::move(menu_button));
    } else {
        spdlog::error("GameScene: 创建菜单按钮失败，UI初始化将继续。");
    }

    auto screen_fade = std::make_unique<engine::ui::UIScreenFade>(context_);
    screen_fade->setOrderIndex(10000);
    screen_fade_ = screen_fade.get();
    ui_manager_->addElement(std::move(screen_fade));

    if (systems_->map_transition_system) {
        systems_->map_transition_system->setFadeOverlay(screen_fade_);
    }

    spdlog::debug("游戏UI初始化完成（时钟UI、物品栏UI与快捷栏UI已创建）。");
    return true;
}

bool GameScene::onInventoryToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }

    if (systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return false;
    }

    if (inventory_ui_) {
        inventory_ui_->toggle();
        if (inventory_ui_->isVisible()) {
            auto view = registry_.view<game::component::PlayerTag>();
            if (!view.empty()) {
                const entt::entity player = *view.begin();
                context_.getDispatcher().enqueue<game::defs::InventorySyncCommand>(player);
                context_.getDispatcher().enqueue<game::defs::HotbarSyncCommand>(player);
            }
        }
        return true;
    }

    return false;
}

bool GameScene::onHotbarToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }

    if (systems_->map_transition_system && systems_->map_transition_system->isTransitionActive()) {
        return false;
    }

    if (hotbar_ui_) {
        hotbar_ui_->toggle();
        if (hotbar_ui_->isVisible()) {
            auto view = registry_.view<game::component::PlayerTag>();
            if (!view.empty()) {
                const entt::entity player = *view.begin();
                context_.getDispatcher().enqueue<game::defs::HotbarSyncCommand>(player);
            }
        }
        return true;
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

void GameScene::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    (void)evt;
}

void GameScene::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (!hotbar_ui_) {
        return;
    }

    if (!registry_.valid(evt.target)) {
        return;
    }

    if (evt.full_sync) {
        hotbar_ui_->clearAllSlots();
        hotbar_ui_->resetInventoryMappings();
    }

    for (const auto& slot : evt.slots) {
        if (slot.hotbar_index < 0 || slot.hotbar_index >= game::component::HotbarComponent::SLOT_COUNT) {
            continue;
        }

        hotbar_ui_->setSlotInventoryIndex(slot.hotbar_index, slot.inventory_slot_index);
        if (slot.item_id != entt::null && slot.count > 0) {
            const engine::render::Image icon = services_->item_catalog
                ? services_->item_catalog->getItemIcon(slot.item_id)
                : engine::render::Image{};
            hotbar_ui_->setSlotItem(slot.hotbar_index, engine::ui::SlotItem{slot.item_id, slot.count, icon});
        } else {
            hotbar_ui_->clearSlot(slot.hotbar_index);
        }
    }
}

void GameScene::onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt) {
    if (!registry_.valid(evt.target)) {
        return;
    }

    if (hotbar_ui_) {
        hotbar_ui_->setActiveSlot(evt.slot_index);
    }
}

void GameScene::onEnterBattleCommand(const game::defs::EnterBattleCommand& cmd) {
    std::vector<game::battle::BattleUnit> units{};
    units.reserve(cmd.player_units.size() + cmd.enemy_units.size());
    units.insert(units.end(), cmd.player_units.begin(), cmd.player_units.end());
    units.insert(units.end(), cmd.enemy_units.begin(), cmd.enemy_units.end());

    requestPushScene(std::make_unique<game::scene::BattleScene>("BattleScene", context_, std::move(units)));
}

void GameScene::onBattleEnded(const game::defs::BattleEndedEvent& evt) {
    spdlog::info("GameScene: Battle ended, outcome={}, final_units={}.",
                 game::battle::toString(evt.outcome),
                 evt.final_units.size());
}

} // namespace game::scene
