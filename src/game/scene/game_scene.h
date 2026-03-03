#pragma once

#include "engine/scene/scene.h"
#include "game/runtime/game_mode.h"
#include "game/defs/events.h"

#include <glm/vec2.hpp>

#include <memory>
#include <optional>

namespace game::data {
    struct GameTime;
}

namespace game::defs {
    struct EnterBattleCommand;
    struct BattleEndedEvent;
}

namespace game::ui {
    class InventoryUI;
    class HotbarUI;
    class TimeClockHud;
    class DialogueBubbleView;
    class DialogueBubbleController;
    class ItemTooltipUI;
}

namespace engine::ui {
    class IScreenFade;
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

    game::ui::InventoryUI* inventory_ui_{nullptr};
    game::ui::HotbarUI* hotbar_ui_{nullptr};
    std::unique_ptr<game::ui::DialogueBubbleController> dialogue_controller_{};
    game::ui::ItemTooltipUI* item_tooltip_ui_{nullptr};
    std::unique_ptr<game::ui::TimeClockHud> time_clock_hud_;
    engine::ui::IScreenFade* screen_fade_{nullptr};
    glm::vec2 previous_camera_position_{0.0f, 0.0f};
    bool has_previous_camera_position_{false};

public:
    GameScene(std::string_view name, engine::core::Context& context,
              std::shared_ptr<game::data::GameTime> game_time = nullptr,
              std::optional<int> load_slot = std::nullopt);
    ~GameScene() noexcept override;

    bool init() override;
    void fixedUpdate(float delta_time) override;
    void update(float delta_time) override;
    void render(float interpolation_alpha) override;

    void clean() override;
    void setGameMode(game::runtime::GameMode mode);
    [[nodiscard]] game::runtime::GameMode getGameMode() const { return game_mode_; }

private:
    void snapshotInterpolationState();
    void bindSceneInputActions();
    [[nodiscard]] bool initUI();  // 在具体场景中初始化UI管理器，且位置要靠后，确保按键注册顺序正确
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool registerDebugPanels();
#endif

    bool onInventoryToggle();
    bool onHotbarToggle();
    bool onPauseToggle();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);
    void onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt);
    void onEnterBattleCommand(const game::defs::EnterBattleCommand& cmd);
    void onBattleEnded(const game::defs::BattleEndedEvent& evt);
};

} // namespace game::scene
