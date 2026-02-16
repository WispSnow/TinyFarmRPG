#pragma once
#include "game/system/fwd.h"
#include "game/defs/events.h"
#include "engine/scene/scene.h"
#include "engine/system/fwd.h"
#include "game/world/world_state.h"
#include "game/world/map_manager.h"
#include <glm/vec2.hpp>
#include <memory>
#include <optional>

namespace game::factory {
    class EntityFactory;
    class BlueprintManager;
}

namespace game::data {
    struct GameTime;
    class ItemCatalog;
}

namespace game::save {
    class SaveService;
}

namespace game::ui {
    class InventoryUI;
    class HotbarUI;
    class DialogueBubble;
    class ItemTooltipUI;
}

namespace engine::spatial {
    class CollisionResolver;
}

namespace engine::ui {
    class UIScreenFade;
}

namespace game::scene {

class GameScene : public engine::scene::Scene {
    std::unique_ptr<engine::system::RenderSystem> render_system_;
    std::unique_ptr<engine::system::LightSystem> light_system_;
    std::unique_ptr<engine::system::YSortSystem> ysort_system_;
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::system::DebugRenderSystem> debug_render_system_;
#endif
    std::unique_ptr<engine::system::MovementSystem> movement_system_;
    std::unique_ptr<engine::system::AnimationSystem> animation_system_;
    std::unique_ptr<engine::system::SpatialIndexSystem> spatial_index_system_;
    std::unique_ptr<engine::spatial::CollisionResolver> collision_resolver_;
    std::unique_ptr<engine::system::AutoTileSystem> auto_tile_system_;
    std::unique_ptr<engine::system::RemoveEntitySystem> remove_entity_system_;
    std::unique_ptr<engine::system::AudioSystem> audio_system_;
    std::unique_ptr<game::system::StateSystem> state_system_;
    std::unique_ptr<game::system::ActionSoundSystem> action_sound_system_;
    std::unique_ptr<game::system::PlayerControlSystem> player_control_system_;
    std::unique_ptr<game::system::CameraFollowSystem> camera_follow_system_;
    std::unique_ptr<game::system::FarmSystem> farm_system_;
    std::unique_ptr<game::system::PickupSystem> pickup_system_;
    std::unique_ptr<game::system::RenderTargetSystem> render_target_system_;
    std::unique_ptr<game::system::AnimationEventSystem> animation_event_system_;
    std::unique_ptr<game::system::TimeSystem> time_system_;
    std::unique_ptr<game::system::TimeOfDayLightSystem> time_of_day_light_system_;
    std::unique_ptr<game::system::LightToggleSystem> light_toggle_system_;
    std::unique_ptr<game::system::DayNightSystem> day_night_system_;
    std::unique_ptr<game::system::CropSystem> crop_system_;
    std::unique_ptr<game::system::InventorySystem> inventory_system_;
    std::unique_ptr<game::system::HotbarSystem> hotbar_system_;
    std::unique_ptr<game::system::ItemUseSystem> item_use_system_;
    std::unique_ptr<game::system::NPCWanderSystem> npc_wander_system_;
    std::unique_ptr<game::system::AnimalBehaviorSystem> animal_behavior_system_;
    std::unique_ptr<game::system::DialogueSystem> dialogue_system_;
    std::unique_ptr<game::system::ChestSystem> chest_system_;
    std::unique_ptr<game::system::InteractionSystem> interaction_system_;
    std::unique_ptr<game::system::RestSystem> rest_system_;
    std::unique_ptr<game::system::MapTransitionSystem> map_transition_system_;

    std::unique_ptr<game::factory::EntityFactory> entity_factory_;      // 实体工厂，负责创建和管理实体

    std::shared_ptr<game::factory::BlueprintManager> blueprint_manager_;// 蓝图管理器，负责管理蓝图数据
    std::shared_ptr<game::data::ItemCatalog> item_catalog_;             // 物品目录

    std::shared_ptr<game::data::GameTime> game_time_;                  // 游戏时间数据
    std::optional<int> load_slot_{};
    bool abort_to_title_{false};
    std::unique_ptr<game::world::WorldState> world_state_;
    std::unique_ptr<game::world::MapManager> map_manager_;
    std::unique_ptr<game::save::SaveService> save_service_;

    glm::vec2 map_pixel_size_;

    game::ui::InventoryUI* inventory_ui_{nullptr};
    game::ui::HotbarUI* hotbar_ui_{nullptr};
    game::ui::DialogueBubble* dialogue_bubble_{nullptr};
    game::ui::ItemTooltipUI* item_tooltip_ui_{nullptr};
    engine::ui::UIScreenFade* screen_fade_{nullptr};

public:
    GameScene(std::string_view name, engine::core::Context& context,
              std::shared_ptr<game::data::GameTime> game_time = nullptr,
              std::optional<int> load_slot = std::nullopt);
    ~GameScene() override;

    bool init() override;
    void update(float delta_time) override;
    void render() override;

    void clean() override;

private:
    [[nodiscard]] bool createCollisionResolver();
    [[nodiscard]] bool ensureBlueprintManager();
    [[nodiscard]] bool ensureItemCatalog();
    [[nodiscard]] bool initWorldState();
    [[nodiscard]] bool initFactory();
    [[nodiscard]] bool initMapManager();
    [[nodiscard]] bool initSaveService();
    [[nodiscard]] bool loadLevel();
    [[nodiscard]] bool initRegistryContext();  // 初始化注册表上下文（将 GameTime 放入 registry.ctx）
    [[nodiscard]] bool configureCamera();
    [[nodiscard]] bool initSystems();
    [[nodiscard]] bool initUI();               // 在具体场景中初始化UI管理器，且位置要靠后，确保按键注册顺序正确
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool registerDebugPanels();
#endif

    bool onInventoryToggle();
    bool onHotbarToggle();
    bool onPauseToggle();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);
    void onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt);

};

} // namespace game::scene
