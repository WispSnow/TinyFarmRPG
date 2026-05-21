#pragma once

#include "game/ui/hud_view_ports.h"

#include <cstdint>
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

class DialogueBoxView final : public DialogueBoxViewPort {
    engine::core::Context& context_;
    engine::ui::rmlui::RmlUiRuntime* runtime_{nullptr};
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* panel_{nullptr};
    Rml::Element* portrait_frame_{nullptr};
    Rml::Element* portrait_{nullptr};
    Rml::Element* speaker_element_{nullptr};
    Rml::Element* text_element_{nullptr};
    Rml::Element* continue_element_{nullptr};
    bool visible_{false};
    std::string speaker_{};
    std::string text_{};
    std::string portrait_decorator_{"none"};

public:
    DialogueBoxView(engine::core::Context& context, std::uint64_t owner_scene_id);
    ~DialogueBoxView() override;

    void setSpeaker(std::string_view speaker) override;
    void setText(std::string_view text) override;
    void setPortraitDecorator(std::string_view decorator) override;
    void setVisible(bool visible) override;

    [[nodiscard]] bool isReady() const;
    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] const std::string& getSpeaker() const { return speaker_; }
    [[nodiscard]] const std::string& getText() const { return text_; }
    [[nodiscard]] const std::string& getPortraitDecorator() const { return portrait_decorator_; }

private:
    void initDocument(std::uint64_t owner_scene_id);
};

} // namespace game::ui
