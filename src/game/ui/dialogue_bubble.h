#pragma once

#include "engine/ui/ui_element.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_label.h"
#include "engine/render/image.h"
#include "game/defs/events.h"
#include <glm/vec2.hpp>
#include <string>
#include <cstdint>
#include <entt/signal/dispatcher.hpp>

namespace engine::render {
    class TextRenderer;
}

namespace game::ui {

class DialogueBubble final : public engine::ui::UIElement {
    engine::render::TextRenderer& text_renderer_;
    entt::dispatcher& dispatcher_;
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* label_{nullptr};
    engine::render::Image bubble_image_;
    glm::vec2 world_position_{0.0f};
    glm::vec2 offset_{0.0f, -4.0f};    ///< @brief 屏幕偏移，让气泡出现在头顶
    float padding_{8.0f};
    std::string font_path_;
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};
    std::uint8_t channel_{0};

public:
    DialogueBubble(engine::core::Context& context,
                   entt::dispatcher& dispatcher,
                   engine::render::TextRenderer& text_renderer,
                   std::string_view font_path = engine::ui::DEFAULT_UI_FONT_PATH,
                   int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                   std::uint8_t channel = 0);
    ~DialogueBubble() override;

    void setText(std::string_view text);
    void setWorldPosition(glm::vec2 world_position);
    void setOffset(glm::vec2 offset);

    void update(float delta_time, engine::core::Context& context) override;

private:
    void subscribeEvents();
    void onShowEvent(const game::defs::DialogueShowEvent& evt);
    void onMoveEvent(const game::defs::DialogueMoveEvent& evt);
    void onHideEvent(const game::defs::DialogueHideEvent& evt);
    void buildSkin(engine::core::Context& context);
    void buildLayout();
    void refreshLayoutFromText();
};

} // namespace game::ui
