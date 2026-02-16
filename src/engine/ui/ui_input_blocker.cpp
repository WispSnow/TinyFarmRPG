#include "ui_input_blocker.h"

namespace engine::ui {

UIInputBlocker::UIInputBlocker(engine::core::Context& context, glm::vec2 position, glm::vec2 size)
    : UIInteractive(context, position, size) {
}

} // namespace engine::ui
