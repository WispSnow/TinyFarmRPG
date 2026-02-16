#pragma once

#include "engine/ui/ui_item_slot.h"
#include "engine/ui/ui_manager.h"

#include <glm/vec2.hpp>

namespace game::ui::helpers {

[[nodiscard]] inline glm::vec2 resolveDragPreviewSize(const engine::ui::UIItemSlot* slot, glm::vec2 fallback_size) {
    if (slot == nullptr) {
        return fallback_size;
    }

    glm::vec2 size = slot->getIconLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        size = slot->getLayoutSize();
    }
    if (size.x <= 0.0f || size.y <= 0.0f) {
        size = slot->getSize();
    }
    if (size.x <= 0.0f || size.y <= 0.0f) {
        size = fallback_size;
    }
    return size;
}

inline void beginDragPreview(engine::ui::UIManager* ui_manager,
                             const engine::render::Image& icon,
                             int count,
                             glm::vec2 preview_size,
                             glm::vec2 start_pos) {
    if (ui_manager == nullptr) {
        return;
    }
    ui_manager->beginDragPreview(icon, count, preview_size);
    ui_manager->updateDragPreview(start_pos);
}

inline void endDragPreview(engine::ui::UIManager* ui_manager) {
    if (ui_manager == nullptr) {
        return;
    }
    ui_manager->endDragPreview();
}

[[nodiscard]] inline engine::ui::UIInteractive* findTarget(engine::ui::UIManager* ui_manager,
                                                           engine::ui::UIInteractive& owner,
                                                           const glm::vec2& pos) {
    if (ui_manager != nullptr) {
        return ui_manager->findInteractiveAt(pos);
    }

    engine::ui::UIElement* root = &owner;
    while (root->getParent() != nullptr) {
        root = root->getParent();
    }
    return root->findInteractiveAt(pos);
}

} // namespace game::ui::helpers

