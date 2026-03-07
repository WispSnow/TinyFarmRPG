#pragma once

#include "engine/ui/ui_defaults.h"
#include "engine/ui/ui_element.h"

#include <string>
#include <string_view>

namespace Rml {
class Element;
class ElementDocument;
}

namespace engine::core {
class Context;
}

namespace engine::render {
class TextRenderer;
}

namespace engine::ui::rmlui {
class RmlUILayer;
}

namespace game::ui {

class DialogueBubbleView final : public engine::ui::UIElement {
    engine::core::Context& context_;
    engine::render::TextRenderer& text_renderer_;
    engine::ui::rmlui::RmlUILayer* layer_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* text_element_{nullptr};
    std::string text_{};
    float padding_{8.0F};
    entt::id_type font_id_{engine::ui::DEFAULT_UI_FONT_ID};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};

public:
    DialogueBubbleView(engine::core::Context& context,
                       engine::render::TextRenderer& text_renderer,
                       uint64_t owner_scene_id,
                       entt::id_type font_id = entt::null,
                       int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);
    ~DialogueBubbleView() override;

    void setText(std::string_view text);
    void setVisible(bool visible);

private:
    void initDocument(uint64_t owner_scene_id);
    void refreshLayoutFromText();
    void renderSelf(engine::core::Context& context) override;
};

} // namespace game::ui
