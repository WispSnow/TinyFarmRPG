#pragma once

#include "engine/ui/ui_element.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_label.h"
#include "engine/render/image.h"
#include <glm/vec2.hpp>
#include <string>
#include <cstdint>

namespace engine::render {
    class TextRenderer;
}

namespace game::ui {

class DialogueBubbleView final : public engine::ui::UIElement {
    engine::render::TextRenderer& text_renderer_;
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* label_{nullptr};
    engine::render::Image bubble_image_;
    float padding_{8.0f};
    entt::id_type font_id_{entt::null};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};

public:
    DialogueBubbleView(engine::core::Context& context,
                       engine::render::TextRenderer& text_renderer,
                       entt::id_type font_id = entt::null,
                       int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);
    ~DialogueBubbleView() override = default;

    void setText(std::string_view text);

private:
    void buildSkin(engine::core::Context& context);
    void buildLayout();
    void refreshLayoutFromText();
};

} // namespace game::ui
