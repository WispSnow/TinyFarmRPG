#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace game::ui {

/// @brief 背包上下文操作菜单中的单条操作。
struct InventoryActionEntryViewModel {
    int action_id{0};
    Rml::String label{};
    bool is_destructive{false};
};

/// @brief 背包上下文操作菜单的 RmlUi 数据模型状态。
class InventoryActionMenuModel final {
public:
    std::vector<InventoryActionEntryViewModel> entries{};
    Rml::String title{};
    bool visible{false};

    [[nodiscard]] bool bind(Rml::DataModelConstructor& constructor);
    [[nodiscard]] bool isVisible() const { return visible; }

    void show(Rml::String next_title,
              std::vector<InventoryActionEntryViewModel> next_entries,
              engine::ui::rmlui::RmlDocumentController& document_controller);
    void close(engine::ui::rmlui::RmlDocumentController& document_controller);
    void markDirty(engine::ui::rmlui::RmlDocumentController& document_controller) const;
};

} // namespace game::ui
