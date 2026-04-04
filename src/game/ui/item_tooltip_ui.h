#pragma once

#include <cstdint>
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
class RmlUiRuntime;
}

namespace game::ui {

class ItemTooltipUI final {
    engine::core::Context& context_;
    engine::ui::rmlui::RmlUiRuntime* runtime_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* name_element_{nullptr};
    Rml::Element* category_element_{nullptr};
    Rml::Element* description_element_{nullptr};

    bool visible_{false};
    glm::vec2 size_{0.0F, 0.0F};
    glm::vec2 offset_{12.0f, 16.0f};
    std::string display_name_{};
    std::string category_{};
    std::string description_{};

public:
    ItemTooltipUI(engine::core::Context& context, uint64_t owner_scene_id);
    ~ItemTooltipUI();

    void showItem(std::string_view display_name,
                  std::string_view category,
                  std::string_view description);
    void hideTooltip();
    void update(float delta_time);

    void setOffset(glm::vec2 offset) { offset_ = offset; }
    [[nodiscard]] bool isVisible() const { return visible_; }

private:
    void initDocument(uint64_t owner_scene_id);
    void refreshLayoutMetrics();
};

} // namespace game::ui
