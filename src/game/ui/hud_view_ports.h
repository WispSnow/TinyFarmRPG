#pragma once

#include <glm/vec2.hpp>

#include <string_view>

namespace game::ui {

class DialogueBoxViewPort {
public:
    virtual ~DialogueBoxViewPort() = default;

    virtual void setSpeaker(std::string_view speaker) = 0;
    virtual void setText(std::string_view text) = 0;
    virtual void setPortraitDecorator(std::string_view decorator) = 0;
    virtual void setVisible(bool visible) = 0;
};

class FloatingNoticeViewPort {
public:
    virtual ~FloatingNoticeViewPort() = default;

    virtual void setText(std::string_view text) = 0;
    virtual void setVisible(bool visible) = 0;
    virtual void setWorldAnchor(glm::vec2 world_position, glm::vec2 screen_offset = {0.0F, 0.0F}) = 0;
    virtual void clearWorldAnchor() = 0;
};

class HotbarVisibilityPort {
public:
    virtual ~HotbarVisibilityPort() = default;

    [[nodiscard]] virtual bool isVisible() const = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
};

} // namespace game::ui
