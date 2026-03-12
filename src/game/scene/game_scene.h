#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"
#include "game/runtime/game_mode.h"
#include "game/defs/events.h"

#include <RmlUi/Core/Types.h>
#include <glm/vec2.hpp>

#include <array>
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
    class HotbarUI;
    class TimeClockHud;
    class DialogueBubbleView;
    class DialogueBubbleController;
    class ItemTooltipUI;
}

namespace engine::ui {
    class IScreenFade;
}

namespace engine::ui::rmlui {
    class RmlScreenFade;
}

namespace Rml {
    class ElementDocument;
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

    std::unique_ptr<game::ui::HotbarUI> hotbar_ui_{};
    std::unique_ptr<game::ui::DialogueBubbleController> dialogue_controller_{};
    std::array<std::unique_ptr<game::ui::DialogueBubbleView>, 3> dialogue_bubbles_{};
    std::unique_ptr<game::ui::ItemTooltipUI> item_tooltip_ui_{};
    std::unique_ptr<game::ui::TimeClockHud> time_clock_hud_;
    std::unique_ptr<engine::ui::rmlui::RmlScreenFade> rml_screen_fade_;
    engine::ui::rmlui::RmlDataBridge overlay_data_bridge_{};
    engine::ui::rmlui::RmlEventBridge overlay_event_bridge_{};
    Rml::ElementDocument* overlay_document_{nullptr};
    bool overlay_click_listener_registered_{false};
    Rml::String primary_prompt_text_{};
    Rml::String secondary_prompt_text_{};
    Rml::String inventory_prompt_text_{};
    Rml::String pause_prompt_text_{};
    bool show_prompt_bar_{true};
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
    [[nodiscard]] bool initUI();
    void refreshOverlayPrompts();
    void removeOverlayEventListeners();
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
};

} // namespace game::scene
