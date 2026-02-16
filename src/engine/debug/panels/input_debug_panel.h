#pragma once

#include "engine/debug/debug_panel.h"
#include "engine/input/input_manager.h"

namespace engine::debug {

class InputDebugPanel final : public DebugPanel {
    engine::input::InputManager& input_manager_;

public:
    explicit InputDebugPanel(engine::input::InputManager& input_manager);

    std::string_view name() const override;
    void draw(bool& is_open) override;

private:
};

} // namespace engine::debug
