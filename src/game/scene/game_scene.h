#pragma once

#include "game_scene_battle_settlement.h"
#include "engine/scene/scene.h"
#include "game/runtime/game_mode.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"

#include <glm/vec2.hpp>

#include <entt/core/fwd.hpp>

#include <memory>
#include <optional>
#include <unordered_map>

namespace game::data {
    struct GameTime;
}

namespace game::defs {
    struct EnterBattleCommand;
    struct BattleEndedEvent;
    struct QuestOfferRequestedEvent;
    struct RecruitOfferRequestedEvent;
}

namespace game::ui {
    class GameInputPromptOverlay;
    class GameOverlay;
    class GameSceneUiController;
}

namespace game::runtime {
    struct GameRuntimeServices;
    struct GameSystemBundle;
    class SystemScheduler;
}

namespace game::debug {
    class SchedulerProfiler;
}

namespace game::scene {

class GameScene : public engine::scene::Scene {
    std::unique_ptr<game::runtime::GameRuntimeServices> services_;
    std::unique_ptr<game::runtime::GameSystemBundle> systems_;
    std::unique_ptr<game::runtime::SystemScheduler> scheduler_;
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<game::debug::SchedulerProfiler> scheduler_profiler_;
#endif
    game::runtime::GameMode game_mode_{game::runtime::GameMode::Exploration};

    std::shared_ptr<game::data::GameTime> game_time_;
    std::optional<int> load_slot_{};
    bool abort_to_title_{false};
    bool context_pushed_{false};

    std::unique_ptr<game::ui::GameSceneUiController> ui_controller_{};
    std::unique_ptr<game::ui::GameOverlay> game_overlay_{};
    std::unique_ptr<game::ui::GameInputPromptOverlay> input_prompt_overlay_{};
    glm::vec2 previous_camera_position_{0.0f, 0.0f};
    bool has_previous_camera_position_{false};
    std::unordered_map<entt::id_type, int> active_battle_initial_item_stocks_{};
    bool has_active_battle_item_stocks_{false};
    std::optional<game::defs::EnemyEncounterBattleContext> active_encounter_context_{};
    game::system::helpers::NotificationTimer battle_reward_notification_{};

public:
    GameScene(std::string_view name, engine::core::Context& context,
              std::shared_ptr<game::data::GameTime> game_time = nullptr,
              std::optional<int> load_slot = std::nullopt);
    ~GameScene() noexcept override;

    bool init() override;
    void fixedUpdate(float delta_time) override;
    void update(float delta_time) override;
    void prepareUi(float interpolation_alpha) override;
    void render(float interpolation_alpha) override;

    void clean() override;
    void setGameMode(game::runtime::GameMode mode);
    [[nodiscard]] game::runtime::GameMode getGameMode() const { return game_mode_; }

private:
    void snapshotInterpolationState();
    void updateBattleRewardNotification(float delta_time);
    void bindSceneInputActions();
    [[nodiscard]] bool initUI();
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool registerDebugPanels();
#endif

    bool onInventoryToggle();
    bool onHotbarToggle();
    bool onPauseToggle();
    bool onTogglePromptBar();
    void onHotbarChanged(const game::defs::HotbarChanged& evt);
    void onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt);
    void onEnterBattleCommand(const game::defs::EnterBattleCommand& cmd);
    void onBattleEnded(const game::defs::BattleEndedEvent& evt);
    void onQuestOfferRequested(const game::defs::QuestOfferRequestedEvent& evt);
    void onRecruitOfferRequested(const game::defs::RecruitOfferRequestedEvent& evt);
    void releaseEnemyEncounterEntryFailure(const game::defs::EnemyEncounterBattleContext& context);
    void resolveActiveEnemyEncounter(const game::defs::BattleEndedEvent& evt);
};

} // namespace game::scene
