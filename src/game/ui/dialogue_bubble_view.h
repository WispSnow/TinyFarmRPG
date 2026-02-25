#pragma once

#include "engine/render/image.h"
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_panel.h"

#include <string_view>

namespace engine::core {
class Context;
}

namespace engine::render {
class TextRenderer;
}

namespace game::ui {

class DialogueBubbleView final : public engine::ui::UIElement {
    engine::render::TextRenderer& text_renderer_;
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* label_{nullptr};
    engine::render::Image bubble_image_{};
    float padding_{8.0F};
    entt::id_type font_id_{entt::null};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};

public:
    DialogueBubbleView(engine::core::Context& context,
                       engine::render::TextRenderer& text_renderer,
                       entt::id_type font_id = entt::null,
                       int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);

    void setText(std::string_view text);

private:
    void buildSkin(engine::core::Context& context);
    void buildLayout();
    void refreshLayoutFromText();
};

} // namespace game::ui

