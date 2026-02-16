#pragma once

#include "engine/ui/ui_element.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_panel.h"
#include <glm/vec2.hpp>
#include <string>
#include <string_view>

namespace engine::core {
class Context;
}

namespace game::ui {

class ItemTooltipUI final : public engine::ui::UIElement {
    engine::core::Context& context_;
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* name_label_{nullptr};
    engine::ui::UILabel* category_label_{nullptr};
    engine::ui::UILabel* description_label_{nullptr};

    engine::ui::Thickness padding_{8.0f, 8.0f, 8.0f, 8.0f};
    float spacing_{3.0f};
    glm::vec2 offset_{12.0f, 16.0f};
    float max_text_width_{240.0f};

    std::string font_path_{};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};

public:
    ItemTooltipUI(engine::core::Context& context,
                  std::string_view font_path = engine::ui::DEFAULT_UI_FONT_PATH,
                  int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);

    void showItem(std::string_view display_name,
                  std::string_view category,
                  std::string_view description);
    void hideTooltip();

    void update(float delta_time, engine::core::Context& context) override;

    void setMaxTextWidth(float width) { max_text_width_ = width; }
    void setOffset(glm::vec2 offset) { offset_ = offset; }
    void setPadding(const engine::ui::Thickness& padding);

private:
    void buildSkin();
    void buildLayout();
    void refreshLayout();
    [[nodiscard]] std::string wrapText(std::string_view text) const;
};

} // namespace game::ui
