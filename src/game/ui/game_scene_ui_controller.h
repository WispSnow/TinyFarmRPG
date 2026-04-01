#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/defs/events.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/fwd.hpp>

#include <array>
#include <cstdint>
#include <functional>
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
}

namespace game::ui {

class DialogueBubbleController;
class DialogueBubbleView;
class HotbarUI;
class ItemTooltipUI;
class TimeClockHud;

class GameSceneUiController final {
public:
    using MenuRequestHandler = std::function<void()>;

    GameSceneUiController(engine::core::Context& context,
                          entt::registry& registry,
                          uint64_t scene_instance_id,
                          game::data::ItemCatalog* item_catalog,
                          MenuRequestHandler on_menu_requested = {});
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

    void setPromptBarVisible(bool visible);
    [[nodiscard]] bool isPromptBarVisible() const { return show_prompt_bar_; }

    [[nodiscard]] engine::ui::IScreenFade* screenFade() const { return screen_fade_; }

private:
    [[nodiscard]] entt::entity findPlayerEntity() const;
    void refreshOverlayPrompts();

    engine::core::Context& context_;
    entt::registry& registry_;
    uint64_t scene_instance_id_{0};
    game::data::ItemCatalog* item_catalog_{nullptr};
    MenuRequestHandler on_menu_requested_{};

    std::unique_ptr<game::ui::HotbarUI> hotbar_ui_{};
    std::unique_ptr<game::ui::DialogueBubbleController> dialogue_controller_{};
    std::array<std::unique_ptr<game::ui::DialogueBubbleView>, 3> dialogue_bubbles_{};
    std::unique_ptr<game::ui::ItemTooltipUI> item_tooltip_ui_{};
    std::unique_ptr<game::ui::TimeClockHud> time_clock_hud_{};
    std::unique_ptr<engine::ui::rmlui::RmlScreenFade> rml_screen_fade_{};

    engine::ui::rmlui::RmlDocumentController overlay_controller_{};
    Rml::String primary_prompt_text_{};
    Rml::String secondary_prompt_text_{};
    Rml::String inventory_prompt_text_{};
    Rml::String pause_prompt_text_{};
    bool show_prompt_bar_{true};
    engine::ui::IScreenFade* screen_fade_{nullptr};
};

} // namespace game::ui
