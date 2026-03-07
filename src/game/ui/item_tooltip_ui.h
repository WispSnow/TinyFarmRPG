#pragma once

#include "engine/ui/ui_defaults.h"
#include "engine/ui/ui_element.h"

#include <glm/vec2.hpp>
#include <string>
#include <string_view>

namespace Rml {
class Element;
class ElementDocument;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class RmlUILayer;
}

namespace game::ui {

class ItemTooltipUI final : public engine::ui::UIElement {
    engine::core::Context& context_;
    engine::ui::rmlui::RmlUILayer* layer_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* name_element_{nullptr};
    Rml::Element* category_element_{nullptr};
    Rml::Element* description_element_{nullptr};

    engine::ui::Thickness padding_{};
    float name_spacing_{0.0f};
    float category_spacing_{0.0f};
    glm::vec2 offset_{12.0f, 16.0f};
    float max_text_width_{0.0f};
    float min_content_width_{0.0f};
    float min_content_height_{0.0f};

    entt::id_type font_id_{engine::ui::DEFAULT_UI_FONT_ID};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};
    int category_font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};
    int description_font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};
    float name_line_height_{0.0f};
    float category_line_height_{0.0f};
    float description_line_height_{0.0f};
    std::string display_name_{};
    std::string category_{};
    std::string description_{};

public:
    ItemTooltipUI(engine::core::Context& context,
                  uint64_t owner_scene_id,
                  entt::id_type font_id = entt::null,
                  int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);
    ~ItemTooltipUI() override;

    void showItem(std::string_view display_name,
                  std::string_view category,
                  std::string_view description);
    void hideTooltip();

    void update(float delta_time, engine::core::Context& context) override;

    void setMaxTextWidth(float width);
    void setOffset(glm::vec2 offset) { offset_ = offset; }
    void setPadding(const engine::ui::Thickness& padding);

private:
    void initDocument(uint64_t owner_scene_id);
    void syncStyleMetricsFromDocument();
    void refreshLayout();
    [[nodiscard]] std::string wrapText(std::string_view text, int font_size) const;
    [[nodiscard]] glm::vec2 measureText(std::string_view text, int font_size, float line_height) const;
};

} // namespace game::ui
