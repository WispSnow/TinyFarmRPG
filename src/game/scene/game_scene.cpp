#include "game_scene.h"
#include "game/factory/entity_factory.h"
#include "game/factory/blueprint_manager.h"
#include "game/loader/entity_builder.h"
#include "game/system/state_system.h"
#include "game/system/action_sound_system.h"
#include "game/system/player_control_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/farm_system.h"
#include "game/system/pickup_system.h"
#include "game/system/render_target_system.h"
#include "game/system/animation_event_system.h"
#include "game/system/time_system.h"
#include "game/system/day_night_system.h"
#include "game/system/time_of_day_light_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/crop_system.h"
#include "game/system/inventory_system.h"
#include "game/system/hotbar_system.h"
#include "game/system/item_use_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/chest_system.h"
#include "game/system/interaction_system.h"
#include "game/system/rest_system.h"
#include "game/system/map_transition_system.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "game/debug/player_debug_panel.h"
#include "game/debug/game_time_debug_panel.h"
#include "game/debug/inventory_debug_panel.h"
#include "game/debug/save_load_debug_panel.h"
#include "game/debug/map_inspector_debug_panel.h"
#include "game/debug/blueprint_inspector_debug_panel.h"
#endif
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/save/save_service.h"
#include "title_scene.h"
#include "pause_menu_scene.h"
#include "game/ui/time_clock_ui.h"
#include "game/ui/inventory_ui.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/dialogue_bubble.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/component/tags.h"
#include "game/component/hotbar_component.h"
#include "game/defs/events.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/panels/spatial_index_debug_panel.h"
#endif
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/text_renderer.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/audio/audio_player.h"
#include "engine/system/render_system.h"
#include "engine/system/light_system.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/system/debug_render_system.h"
#endif
#include "engine/system/animation_system.h"
#include "engine/system/movement_system.h"
#include "engine/system/spatial_index_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/audio_system.h"
#include "engine/system/ysort_system.h"
#include "engine/spatial/collision_resolver.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_screen_fade.h"
#include "engine/utils/events.h"
#include "game/defs/audio_ids.h"
#include "game/world/world_state.h"
#include "game/world/map_manager.h"
#include <glm/vec2.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace {
    constexpr int MUSIC_FADE_IN_MS = 200;
}

namespace game::scene {

GameScene::GameScene(std::string_view name, engine::core::Context& context,
                     std::shared_ptr<game::data::GameTime> game_time,
                     std::optional<int> load_slot)
    : engine::scene::Scene(name, context),
      game_time_(std::move(game_time)),
      load_slot_(load_slot) {
}

GameScene::~GameScene() {
    context_.getInputManager().onAction("inventory"_hs).disconnect<&GameScene::onInventoryToggle>(this);
    context_.getInputManager().onAction("hotbar"_hs).disconnect<&GameScene::onHotbarToggle>(this);
    context_.getInputManager().onAction("pause"_hs).disconnect<&GameScene::onPauseToggle>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>().disconnect<&GameScene::onHotbarChanged>(this);
    context_.getDispatcher().sink<game::defs::InventoryChanged>().disconnect<&GameScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarSlotChanged>().disconnect<&GameScene::onHotbarSlotChanged>(this);
}

bool GameScene::init() {
    // --- 1) 基础数据与核心设施（data + infra） ---
    if (!ensureBlueprintManager()) return false;
    if (!ensureItemCatalog()) return false;
    if (!createCollisionResolver()) return false;

    // --- 2) registry.ctx 与 world-level 数据 ---
    if (!initRegistryContext()) return false;  // ctx 必须先就位（例如 GameTime）
    if (!initWorldState()) return false;

    // --- 3) 工厂与地图管理（map services） ---
    if (!initFactory()) return false;
    if (!initMapManager()) return false;
    if (!initSaveService()) return false;

    // --- 4) 初始地图（fresh start baseline） ---
    if (!loadLevel()) return false;
    if (!configureCamera()) return false;

    // --- 5) 系统装配（systems） ---
    if (!initSystems()) return false;

    // --- 6) UI 与调试面板（UI/Debug 依赖系统与数据） ---
    if (!initUI()) return false;               // UI 放在系统之后：需要事件与输入绑定已就绪
#ifdef TF_ENABLE_DEBUG_UI
    if (!registerDebugPanels()) return false;
#endif

    // --- 7) Scene 级事件监听（用于驱动 UI） ---
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::InventoryChanged>().connect<&GameScene::onInventoryChanged>(this);
    dispatcher.sink<game::defs::HotbarChanged>().connect<&GameScene::onHotbarChanged>(this);
    dispatcher.sink<game::defs::HotbarSlotChanged>().connect<&GameScene::onHotbarSlotChanged>(this);

    // --- 8) 可选：读档（放在 UI/监听就绪之后，避免首帧 UI 不一致） ---
    if (load_slot_) {
        std::string load_error;
        if (!save_service_->loadFromFile(game::save::SaveService::slotPath(*load_slot_), load_error)) {
            const std::string message = "读档失败: " + load_error;
            spdlog::error("GameScene: 读档失败 (slot {}): {}", *load_slot_, load_error);
            requestReplaceScene(std::make_unique<game::scene::TitleScene>("TitleScene", context_, message));
            abort_to_title_ = true;
        }
    }

    // 读档失败：交给 SceneManager 在下一帧 replace；本 Scene 不进入 Playing/不播 BGM。
    // 仍然调用 Scene::init() 以保证 replace 时 clean() 路径一致（清理 registry/spatial/debug panels）。
    if (abort_to_title_) {
        (void)Scene::init();
        return true;
    }

    // --- 9) 初始 UI 同步（放在读档之后，保证目标实体与数据已稳定） ---
    auto player_view = registry_.view<game::component::PlayerTag>();
    if (!player_view.empty()) {
        const entt::entity player = *player_view.begin();
        dispatcher.enqueue<game::defs::InventorySyncRequest>(player);
        dispatcher.enqueue<game::defs::HotbarSyncRequest>(player);
    }

    context_.getGameState().setState(engine::core::State::Playing);

    if (!Scene::init()) return false;
    context_.getAudioPlayer().playMusic(game::defs::audio::SCENE_BG_MUSIC_ID.value(), true, MUSIC_FADE_IN_MS);
    return true;
}

void GameScene::update(float delta_time) {
    if (abort_to_title_) {
        Scene::update(delta_time);
        return;
    }

    // 说明：System 的更新顺序不是随意的，而是隐式依赖的体现。
    // - 先清理（RemoveEntitySystem）：避免“已经标记删除”的实体继续参与本帧逻辑。
    // - 过渡期间优先推进过渡状态并冻结世界：避免切图瞬间误操作与状态抖动。
    // - 先时间/光照，再输入/AI：让后续系统读到最新时间，并基于最新输入做决策。
    // - 移动之后再更新空间索引/相机：空间查询与相机跟随都依赖最新位置。
    // - 最后更新 UI（Scene::update）：UI 通常希望读取本帧已经结算后的世界状态。
    // 每轮循环最先移除标记实体删除的实体
    remove_entity_system_->update(registry_);

    // 地图切换过渡期间冻结世界（只推进过渡状态机与UI，避免切图瞬间的误操作与画面抖动）
    if (map_transition_system_ && map_transition_system_->isTransitionActive()) {
        map_transition_system_->update();
        light_toggle_system_->update();
        Scene::update(delta_time);
        return;
    }

    // 更新时间系统（应该在最早更新，以便其他系统可以使用最新的时间）
    time_system_->update(delta_time);
    day_night_system_->update();  // 更新光照参数
    
    player_control_system_->update(delta_time);
    npc_wander_system_->update(delta_time);
    animal_behavior_system_->update(delta_time);
    chest_system_->update(delta_time);
    item_use_system_->update(delta_time);
    dialogue_system_->update(delta_time);
    action_sound_system_->update(delta_time);
    auto_tile_system_->update();
    state_system_->update();
    movement_system_->update(registry_, delta_time);
    map_transition_system_->update();
    light_toggle_system_->update();

    if (map_transition_system_ && map_transition_system_->isTransitionActive()) {
        Scene::update(delta_time);
        return;
    }
    spatial_index_system_->update(registry_);   // 空间索引系统在移动系统之后更新
    pickup_system_->update(delta_time);
    interaction_system_->update();
    camera_follow_system_->update(delta_time);  // 相机跟随应该在移动系统之后更新
    animation_system_->update(delta_time);
    Scene::update(delta_time);
}
    
void GameScene::render() {
    if (abort_to_title_) {
        Scene::render();
        return;
    }
    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();

    ysort_system_->update(registry_);
    render_system_->update(registry_, renderer, camera);
    light_system_->update(registry_, renderer);
    render_target_system_->update(renderer);
#ifdef TF_ENABLE_DEBUG_UI
    if (debug_render_system_) {
        debug_render_system_->update(registry_, renderer);
    }
#endif

    Scene::render();
}

void GameScene::clean() {
#ifdef TF_ENABLE_DEBUG_UI
    context_.getDebugUIManager().unregisterPanels(engine::debug::PanelCategory::Game);
#endif
    Scene::clean();
}

bool GameScene::createCollisionResolver() {
    collision_resolver_ = std::make_unique<engine::spatial::CollisionResolver>(registry_, context_.getSpatialIndexManager());
    return true;
}

bool GameScene::initRegistryContext() {
    // 如果未传入 game_time，则从配置文件加载
    if (!game_time_) {
        game_time_ = game::data::GameTime::loadFromConfig("assets/data/game_time_config.json");
        if (!game_time_) {
            spdlog::error("从配置文件加载 GameTime 失败");
            return false;
        }
    }

    // 视觉规则（自发光显隐窗口）来自 light_config.json；与玩法 TimeOfDay 配置解耦。
    game_time_->loadEmissiveVisibilityFromLightConfig("assets/data/light_config.json");
    
    // 将 GameTime 放入注册表上下文
    registry_.ctx().emplace<game::data::GameTime>(*game_time_);
    spdlog::trace("已将 GameTime 放入注册表上下文。");
    return true;
}

bool GameScene::ensureBlueprintManager() {
    if (!blueprint_manager_) {
        blueprint_manager_ = std::make_shared<game::factory::BlueprintManager>();
        if (!blueprint_manager_->loadActorBlueprints("assets/data/actor_blueprint.json")) {
            spdlog::error("加载角色蓝图失败");
            return false;
        }
        if (!blueprint_manager_->loadAnimalBlueprints("assets/data/animal_blueprint.json")) {
            spdlog::error("加载动物蓝图失败");
            return false;
        }
        if (!blueprint_manager_->loadCropBlueprints("assets/data/crop_config.json")) {
            spdlog::error("加载作物蓝图失败");
            return false;
        }
    }
    return true;
}

bool GameScene::ensureItemCatalog() {
    if (!item_catalog_) {
        item_catalog_ = std::make_shared<game::data::ItemCatalog>();
        if (!item_catalog_->loadIconConfig("assets/data/icon_config.json")) {
            spdlog::error("加载物品图标配置失败");
            return false;
        }
        if (!item_catalog_->loadItemConfig("assets/data/item_config.json")) {
            spdlog::error("加载物品配置失败");
            return false;
        }
    }
    return true;
}

bool GameScene::initWorldState() {
    if (!world_state_) {
        world_state_ = std::make_unique<game::world::WorldState>();
    }
    if (!world_state_->loadFromWorldFile("assets/maps/farm-rpg.world", "home_exterior"_hs)) {
        spdlog::error("加载 world 布局失败");
        return false;
    }
    // 注册外部地图（未来可设置配置文件）
    (void)world_state_->ensureExternalMap("home_interior");
    if (auto ptr = registry_.ctx().find<game::world::WorldState*>()) {
        *ptr = world_state_.get();
    } else {
        registry_.ctx().emplace<game::world::WorldState*>(world_state_.get());
    }
    return true;
}

bool GameScene::initFactory() {
    if (!blueprint_manager_) return false;
    auto& spatial_index_manager = context_.getSpatialIndexManager();
    auto& auto_tile_library = context_.getResourceManager().getAutoTileLibrary();
    entity_factory_ = std::make_unique<game::factory::EntityFactory>(registry_, *blueprint_manager_, &spatial_index_manager, &auto_tile_library);
    return true;
}

bool GameScene::initMapManager() {
    if (!world_state_ || !entity_factory_ || !blueprint_manager_) return false;
    map_manager_ = std::make_unique<game::world::MapManager>(*this, context_, registry_, *world_state_, *entity_factory_, *blueprint_manager_);

    // 地图加载配置（预加载开关来自配置文件）
    auto settings = game::world::MapLoadingSettings::loadFromFile("assets/data/map_loading_config.json");
    map_manager_->setLoadingSettings(settings);
    spdlog::info("MapLoading: preload_mode={}, log_timings={}",
                 game::world::MapLoadingSettings::toString(settings.preload_mode),
                 settings.log_timings);
    if (settings.preload_mode == game::world::MapPreloadMode::All) {
        map_manager_->preloadAllMaps();
    }

    return true;
}

bool GameScene::initSaveService() {
    if (!map_manager_ || !world_state_ || !blueprint_manager_) {
        spdlog::error("MapManager/WorldState/BlueprintManager 未初始化，无法创建 SaveService");
        return false;
    }
    save_service_ = std::make_unique<game::save::SaveService>(context_, registry_, *world_state_, *map_manager_, *blueprint_manager_);
    return true;
}

bool GameScene::loadLevel() {
    if (!map_manager_) {
        spdlog::error("MapManager 未初始化，无法加载地图");
        return false;
    }
    entt::id_type initial_map = entt::null;
    if (world_state_) {
        initial_map = world_state_->getCurrentMap();
        if (initial_map == entt::null) {
            if (auto* map_state = world_state_->getMapState("home_exterior")) {
                initial_map = map_state->info.id;
            }
        }
    }
    if (initial_map == entt::null) {
        initial_map = "home_exterior"_hs;
    }
    if (!map_manager_->loadMap(initial_map)) {
        spdlog::error("加载关卡失败");
        return false;
    }
    map_pixel_size_ = map_manager_->currentMapPixelSize();
    return true;
}

bool GameScene::configureCamera() {
    auto& camera = context_.getCamera();
    camera.setZoom(2.0f);
    return true;
}

bool GameScene::initSystems() {
    auto& dispatcher = context_.getDispatcher();
    auto& input_manager = context_.getInputManager();
    auto& camera = context_.getCamera();
    auto& spatial_index_manager = context_.getSpatialIndexManager();
#ifdef TF_ENABLE_DEBUG_UI
    auto& debug_ui_manager = context_.getDebugUIManager();
#endif
    auto& resource_manager = context_.getResourceManager();
    auto& auto_tile_library = resource_manager.getAutoTileLibrary();
#ifdef TF_ENABLE_DEBUG_UI
    auto* spatial_panel = debug_ui_manager.getPanel<engine::debug::SpatialIndexDebugPanel>(engine::debug::PanelCategory::Engine);
#endif

    // --- Engine systems（引擎层） ---
    render_system_ = std::make_unique<engine::system::RenderSystem>();
    light_system_ = std::make_unique<engine::system::LightSystem>();
    ysort_system_ = std::make_unique<engine::system::YSortSystem>();
#ifdef TF_ENABLE_DEBUG_UI
    debug_render_system_ = std::make_unique<engine::system::DebugRenderSystem>(spatial_index_manager, spatial_panel);
#endif
    movement_system_ = std::make_unique<engine::system::MovementSystem>(collision_resolver_.get());
    spatial_index_system_ = std::make_unique<engine::system::SpatialIndexSystem>(spatial_index_manager);
    animation_system_ = std::make_unique<engine::system::AnimationSystem>(registry_, dispatcher);

    // --- Game systems（游戏逻辑层） ---
    state_system_ = std::make_unique<game::system::StateSystem>(registry_, dispatcher);
    action_sound_system_ = std::make_unique<game::system::ActionSoundSystem>(registry_, dispatcher);
    player_control_system_ = std::make_unique<game::system::PlayerControlSystem>(registry_, dispatcher, input_manager, camera, spatial_index_manager, item_catalog_.get());
    camera_follow_system_ = std::make_unique<game::system::CameraFollowSystem>(registry_, camera, input_manager);
    auto_tile_system_ = std::make_unique<engine::system::AutoTileSystem>(registry_, dispatcher, auto_tile_library, spatial_index_manager);
    farm_system_ = std::make_unique<game::system::FarmSystem>(registry_, dispatcher, spatial_index_manager, *entity_factory_, *blueprint_manager_, item_catalog_.get());
    pickup_system_ = std::make_unique<game::system::PickupSystem>(registry_, dispatcher);
    render_target_system_ = std::make_unique<game::system::RenderTargetSystem>(registry_);
    animation_event_system_ = std::make_unique<game::system::AnimationEventSystem>(registry_, dispatcher);

    // --- 时间/光照 ---
    time_system_ = std::make_unique<game::system::TimeSystem>(registry_, dispatcher);
    time_of_day_light_system_ = std::make_unique<game::system::TimeOfDayLightSystem>(registry_, dispatcher);
    day_night_system_ = std::make_unique<game::system::DayNightSystem>(registry_);
    light_toggle_system_ = std::make_unique<game::system::LightToggleSystem>(registry_, dispatcher, "assets/data/light_config.json");

    // --- NPC / 动物 / 对话 ---
    npc_wander_system_ = std::make_unique<game::system::NPCWanderSystem>(registry_);
    animal_behavior_system_ = std::make_unique<game::system::AnimalBehaviorSystem>(registry_);
    dialogue_system_ = std::make_unique<game::system::DialogueSystem>(registry_, dispatcher);
    dialogue_system_->loadDialogueFile("assets/data/dialogue_script.json");

    // --- 交互/休息/箱子 ---
    chest_system_ = std::make_unique<game::system::ChestSystem>(registry_, dispatcher, *world_state_, *item_catalog_);
    interaction_system_ = std::make_unique<game::system::InteractionSystem>(
        registry_, dispatcher, input_manager, spatial_index_manager, *world_state_);
    rest_system_ = std::make_unique<game::system::RestSystem>(registry_, context_);

    // --- 物品栏/快捷栏/使用 ---
    inventory_system_ = std::make_unique<game::system::InventorySystem>(registry_, dispatcher, *item_catalog_);
    hotbar_system_ = std::make_unique<game::system::HotbarSystem>(registry_, dispatcher);
    item_use_system_ = std::make_unique<game::system::ItemUseSystem>(registry_, dispatcher, *item_catalog_);

    // 加载光照配置
    if (!day_night_system_->loadConfig("assets/data/light_config.json")) {
        spdlog::warn("光照配置加载失败，将使用默认配置");
    }

    // --- 作物（依赖蓝图） ---
    crop_system_ = std::make_unique<game::system::CropSystem>(registry_, dispatcher, spatial_index_manager, *blueprint_manager_);

    // --- 生命周期/音频 ---
    remove_entity_system_ = std::make_unique<engine::system::RemoveEntitySystem>();
    audio_system_ = std::make_unique<engine::system::AudioSystem>(registry_, context_);

    // --- 地图切换 ---
    map_transition_system_ = std::make_unique<game::system::MapTransitionSystem>(
        registry_, 
        *world_state_, 
        *map_manager_,
        collision_resolver_.get()
    );

    // --- Scene 级输入（打开 UI / 暂停） ---
    input_manager.onAction("inventory"_hs).connect<&GameScene::onInventoryToggle>(this);
    input_manager.onAction("hotbar"_hs).connect<&GameScene::onHotbarToggle>(this);
    input_manager.onAction("pause"_hs).connect<&GameScene::onPauseToggle>(this);
    return true;
}

#ifdef TF_ENABLE_DEBUG_UI
bool GameScene::registerDebugPanels() {
    auto& debug_ui_manager = context_.getDebugUIManager();
    auto& dispatcher = context_.getDispatcher();

    // 注册玩家调试面板
    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::PlayerDebugPanel>(registry_, dispatcher),
        false,  // 默认不启用
        engine::debug::PanelCategory::Game  // 游戏层面板
    );

    // 注册游戏时间调试面板
    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::GameTimeDebugPanel>(registry_, dispatcher),
        false,  // 默认不启用
        engine::debug::PanelCategory::Game  // 游戏层面板
    );
    debug_ui_manager.registerPanel(
        std::make_unique<game::debug::InventoryDebugPanel>(registry_, dispatcher, item_catalog_.get()),
        false,
        engine::debug::PanelCategory::Game
    );

    if (save_service_) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::SaveLoadDebugPanel>(*save_service_),
            false,
            engine::debug::PanelCategory::Game
        );
    }

    if (map_manager_ && world_state_) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::MapInspectorDebugPanel>(
                *map_manager_,
                *world_state_,
                registry_,
                context_.getSpatialIndexManager()
            ),
            false,
            engine::debug::PanelCategory::Game
        );
    }

    if (blueprint_manager_) {
        debug_ui_manager.registerPanel(
            std::make_unique<game::debug::BlueprintInspectorDebugPanel>(*blueprint_manager_),
            false,
            engine::debug::PanelCategory::Game
        );
    }

    spdlog::trace("游戏层调试面板注册完成。");
    return true;
}
#endif

bool GameScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();

    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);
    // 获取文本渲染器引用
    auto& text_renderer = context_.getTextRenderer();

    // 创建时钟UI（屏幕左上角位置）
    auto time_clock_ui = std::make_unique<game::ui::TimeClockUI>(context_, registry_, text_renderer);

    // 添加到UI管理器
    ui_manager_->addElement(std::move(time_clock_ui));

    auto inventory_ui = std::make_unique<game::ui::InventoryUI>(context_, item_catalog_.get());
    inventory_ui_ = inventory_ui.get();
    ui_manager_->addElement(std::move(inventory_ui));

    auto hotbar_ui = std::make_unique<game::ui::HotbarUI>(context_, item_catalog_.get());
    hotbar_ui_ = hotbar_ui.get();
    ui_manager_->addElement(std::move(hotbar_ui));

    auto item_tooltip_ui = std::make_unique<game::ui::ItemTooltipUI>(context_);
    item_tooltip_ui->setOrderIndex(1000);
    item_tooltip_ui_ = item_tooltip_ui.get();
    ui_manager_->addElement(std::move(item_tooltip_ui));

    auto& dispatcher_ref = context_.getDispatcher();
    auto dialogue_bubble = std::make_unique<game::ui::DialogueBubble>(context_,
                                                                      dispatcher_ref,
                                                                      text_renderer,
                                                                      engine::ui::DEFAULT_UI_FONT_PATH,
                                                                      engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                                      0);
    dialogue_bubble_ = dialogue_bubble.get();
    ui_manager_->addElement(std::move(dialogue_bubble));

    // 通知气泡（用于拾取/开箱等短提示），与对话气泡分频道避免互相覆盖
    ui_manager_->addElement(std::make_unique<game::ui::DialogueBubble>(context_,
                                                                       dispatcher_ref,
                                                                       text_renderer,
                                                                       engine::ui::DEFAULT_UI_FONT_PATH,
                                                                       engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                                       1));

    // 物品“使用/消耗”提示气泡（仅物品栏右键显示）
    auto item_use_bubble = std::make_unique<game::ui::DialogueBubble>(context_,
                                                                      dispatcher_ref,
                                                                      text_renderer,
                                                                      engine::ui::DEFAULT_UI_FONT_PATH,
                                                                      engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                                      2);
    item_use_bubble->setOffset(glm::vec2{0.0f, -56.0f});
    ui_manager_->addElement(std::move(item_use_bubble));

    if (inventory_ui_) {
        inventory_ui_->setUIManager(ui_manager_.get());
    }
    if (hotbar_ui_) {
        hotbar_ui_->setUIManager(ui_manager_.get());
    }
    if (item_tooltip_ui_) {
        if (inventory_ui_) inventory_ui_->setTooltipUI(item_tooltip_ui_);
        if (hotbar_ui_) hotbar_ui_->setTooltipUI(item_tooltip_ui_);
    }

    // 设置 UI 之间的关联与目标实体
    auto player_view = registry_.view<game::component::PlayerTag>();
    if (!player_view.empty()) {
        entt::entity player = *player_view.begin();
        if (inventory_ui_) inventory_ui_->setTarget(player);
        if (hotbar_ui_) hotbar_ui_->setTarget(player);
    }
    if (inventory_ui_ && hotbar_ui_) {
        inventory_ui_->setHotbarUI(hotbar_ui_);
        hotbar_ui_->setInventoryUI(inventory_ui_);
    }

    auto menu_button = engine::ui::UIButton::create(
        context_, "menu", glm::vec2{-10.0f, 10.0f}, glm::vec2{32.0f, 32.0f}, [this]() { (void)onPauseToggle(); });
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
    if (map_transition_system_) {
        map_transition_system_->setFadeOverlay(screen_fade_);
    }

    spdlog::debug("游戏UI初始化完成（时钟UI、物品栏UI与快捷栏UI已创建）。");
    return true;
}

bool GameScene::onInventoryToggle() {
    if (context_.getGameState().isPaused()) {
        return false;
    }
    if (map_transition_system_ && map_transition_system_->isTransitionActive()) {
        return false;
    }
    if (inventory_ui_) {
        inventory_ui_->toggle();
        if (inventory_ui_->isVisible()) {
            auto view = registry_.view<game::component::PlayerTag>();
            if (!view.empty()) {
                entt::entity player = *view.begin();
                context_.getDispatcher().enqueue<game::defs::InventorySyncRequest>(player);
                context_.getDispatcher().enqueue<game::defs::HotbarSyncRequest>(player);
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
    if (map_transition_system_ && map_transition_system_->isTransitionActive()) {
        return false;
    }
    if (hotbar_ui_) {
        hotbar_ui_->toggle();
        if (hotbar_ui_->isVisible()) {
            auto view = registry_.view<game::component::PlayerTag>();
            if (!view.empty()) {
                entt::entity player = *view.begin();
                context_.getDispatcher().enqueue<game::defs::HotbarSyncRequest>(player);
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
    if (map_transition_system_ && map_transition_system_->isTransitionActive()) {
        return false;
    }

    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    auto menu = std::make_unique<game::scene::PauseMenuScene>("PauseMenu", context_, save_service_.get(), game_time);
    requestPushScene(std::move(menu));
    return true;
}

void GameScene::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    // HotbarSystem 始终存在（initSystems 中创建），由 HotbarChanged 事件驱动快捷栏 UI 更新。
    // 此回调仅用于未来需要在 InventoryChanged 时做额外 UI 响应的扩展点。
    (void)evt;
}

void GameScene::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (!hotbar_ui_) return;
    if (!registry_.valid(evt.target)) return;

    if (evt.full_sync) {
        hotbar_ui_->clearAllSlots();
        hotbar_ui_->resetInventoryMappings();
    }

    for (const auto& slot : evt.slots) {
        if (slot.hotbar_index < 0 || slot.hotbar_index >= game::component::HotbarComponent::SLOT_COUNT) continue;
        hotbar_ui_->setSlotInventoryIndex(slot.hotbar_index, slot.inventory_slot_index);
        if (slot.item_id != entt::null && slot.count > 0) {
            const engine::render::Image icon = item_catalog_ ? item_catalog_->getItemIcon(slot.item_id) : engine::render::Image{};
            hotbar_ui_->setSlotItem(slot.hotbar_index, engine::ui::SlotItem{slot.item_id, slot.count, icon});
        } else {
            hotbar_ui_->clearSlot(slot.hotbar_index);
        }
    }
}

void GameScene::onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt) {
    if (!registry_.valid(evt.target)) return;

    // 更新快捷栏UI的激活槽位
    if (hotbar_ui_) {
        hotbar_ui_->setActiveSlot(evt.slot_index);
    }
}

} // namespace game::scene
