#pragma once

#include "game/defs/events.h"

#include <entt/entity/fwd.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace engine::core {
class Context;
}

namespace engine::render {
class Camera;
}

namespace engine::ui {
class IScreenFade;
}

namespace engine::ui::rmlui {
class RmlScreenFade;
}

namespace game::data {
struct GameTime;
class ItemCatalog;
class RpgCatalog;
}

namespace game::ui {

class DialogueBoxView;
class DialoguePresentationController;
class FloatingNoticeView;
class HotbarUI;
class ItemTooltipUI;
class TimeClockHud;

class GameSceneUiController final {
public:
    GameSceneUiController(engine::core::Context& context,
                          entt::registry& registry,
                          uint64_t scene_instance_id,
                          game::data::ItemCatalog* item_catalog,
                          const game::data::RpgCatalog* rpg_catalog);
    ~GameSceneUiController();

    GameSceneUiController(const GameSceneUiController&) = delete;
    GameSceneUiController& operator=(const GameSceneUiController&) = delete;
    GameSceneUiController(GameSceneUiController&&) = delete;
    GameSceneUiController& operator=(GameSceneUiController&&) = delete;

    [[nodiscard]] bool init();
    void update(float delta_time);
    void refreshAnchoredWidgets(const engine::render::Camera& camera, float interpolation_alpha);
    void clean();

    [[nodiscard]] bool toggleHotbar();
    void applyHotbarChanged(const game::defs::HotbarChanged& evt);
    void applyHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt);

    [[nodiscard]] engine::ui::IScreenFade* screenFade() const { return screen_fade_; }

private:
    [[nodiscard]] entt::entity findPlayerEntity() const;

    engine::core::Context& context_;
    entt::registry& registry_;
    uint64_t scene_instance_id_{0};
    game::data::ItemCatalog* item_catalog_{nullptr};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};

    std::unique_ptr<game::ui::HotbarUI> hotbar_ui_{};
    std::unique_ptr<game::ui::DialogueBoxView> dialogue_box_{};
    std::unique_ptr<game::ui::DialoguePresentationController> dialogue_controller_{};
    std::array<std::unique_ptr<game::ui::FloatingNoticeView>, 2> floating_notices_{};
    std::unique_ptr<game::ui::ItemTooltipUI> item_tooltip_ui_{};
    std::unique_ptr<game::ui::TimeClockHud> time_clock_hud_{};
    std::unique_ptr<engine::ui::rmlui::RmlScreenFade> rml_screen_fade_{};

    engine::ui::IScreenFade* screen_fade_{nullptr};
};

} // namespace game::ui
