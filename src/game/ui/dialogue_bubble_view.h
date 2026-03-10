#pragma once

#include "engine/ui/ui_types.h"
#include "game/ui/world_anchor_state.h"

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

namespace engine::render {
class Camera;
class TextRenderer;
}

namespace engine::ui::rmlui {
class RmlUILayer;
}

namespace game::ui {

class DialogueBubbleView final {
    engine::core::Context& context_;
    engine::render::TextRenderer& text_renderer_;
    engine::ui::rmlui::RmlUILayer* layer_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* text_element_{nullptr};
    bool visible_{false};
    std::string text_{};
    glm::vec2 size_{0.0F, 0.0F};
    glm::vec2 pivot_{0.5F, 1.0F};
    engine::ui::Thickness padding_{};
    game::ui::WorldAnchorState world_anchor_{};
    float min_content_width_{0.0F};
    float min_content_height_{0.0F};
    float line_height_{0.0F};
    entt::id_type font_id_{engine::ui::DEFAULT_UI_FONT_ID};
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};

public:
    DialogueBubbleView(engine::core::Context& context,
                       engine::render::TextRenderer& text_renderer,
                       uint64_t owner_scene_id,
                       entt::id_type font_id = entt::null,
                       int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX);
    ~DialogueBubbleView();

    void setText(std::string_view text);
    void setVisible(bool visible);
    void setWorldAnchor(glm::vec2 world_position, glm::vec2 screen_offset = {0.0F, 0.0F});
    void clearWorldAnchor();
    void refreshAnchoredPosition(const engine::render::Camera& camera, float interpolation_alpha);

    [[nodiscard]] bool isReady() const { return document_ != nullptr && panel_ != nullptr && text_element_ != nullptr; }
    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] bool hasWorldAnchor() const { return world_anchor_.hasWorldAnchor(); }
    [[nodiscard]] const std::string& getText() const { return text_; }
    [[nodiscard]] const glm::vec2& getWorldAnchor() const { return world_anchor_.worldAnchor(); }
    [[nodiscard]] const glm::vec2& getPreviousWorldAnchor() const { return world_anchor_.previousWorldAnchor(); }
    [[nodiscard]] const glm::vec2& getWorldAnchorOffset() const { return world_anchor_.screenOffset(); }

private:
    void initDocument(uint64_t owner_scene_id);
    void syncStyleMetricsFromDocument();
    void refreshLayoutFromText();
    [[nodiscard]] glm::vec2 measureText(std::string_view text) const;
};

} // namespace game::ui
