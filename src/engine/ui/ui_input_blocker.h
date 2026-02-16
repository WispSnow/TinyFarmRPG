#pragma once

#include "ui_interactive.h"
#include <memory>

namespace engine::ui {

class UIInputBlocker final : public UIInteractive {
public:
    UIInputBlocker(engine::core::Context& context, glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});

protected:
    void renderSelf(engine::core::Context& /*context*/) override {}
};

} // namespace engine::ui
