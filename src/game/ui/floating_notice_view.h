#pragma once

#include "game/ui/hud_view_ports.h"
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
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::ui {

class FloatingNoticeView final : public FloatingNoticeViewPort {
    engine::core::Context& context_;
    engine::ui::rmlui::RmlUiRuntime* runtime_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* text_element_{nullptr};
    bool visible_{false};
    std::string text_{};
    glm::vec2 size_{0.0F, 0.0F};
    glm::vec2 pivot_{0.5F, 1.0F};
    game::ui::WorldAnchorState world_anchor_{};

public:
    FloatingNoticeView(engine::core::Context& context, uint64_t owner_scene_id);
    ~FloatingNoticeView() override;

    void setText(std::string_view text) override;
    void setVisible(bool visible) override;
    void setWorldAnchor(glm::vec2 world_position, glm::vec2 screen_offset = {0.0F, 0.0F}) override;
    void clearWorldAnchor() override;
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
    void refreshLayoutMetrics();
};

} // namespace game::ui
