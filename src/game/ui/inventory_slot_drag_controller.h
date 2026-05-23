#pragma once

#include "game/ui/slot_grid_support.h"

#include <vector>

namespace game::component {
struct HotbarComponent;
}

namespace game::ui {

enum class InventorySlotDragCommandKind {
    InventoryMove,
    HotbarBind,
};

/// @brief Drag/drop 解析出的背包或快捷栏命令意图。
struct InventorySlotDragCommand {
    InventorySlotDragCommandKind kind{InventorySlotDragCommandKind::InventoryMove};
    int inventory_slot_index{-1};
    int target_inventory_slot_index{-1};
    int hotbar_slot_index{-1};
    bool allow_merge{true};

    [[nodiscard]] static InventorySlotDragCommand inventoryMove(int source_inventory_slot,
                                                                int target_inventory_slot,
                                                                bool merge_stacks);
    [[nodiscard]] static InventorySlotDragCommand hotbarBind(int hotbar_slot, int inventory_slot);
};

struct InventorySlotDragDropPlan {
    bool handled{false};
    std::vector<InventorySlotDragCommand> commands{};
};

struct InventorySlotDragEndPlan {
    bool handled{false};
    bool unbind_hotbar{false};
    int hotbar_slot_index{-1};
};

/// @brief Encapsulates inventory slot drag state and command planning.
class InventorySlotDragController final {
public:
    [[nodiscard]] bool active() const { return state_.active; }

    void start();
    void clear();

    [[nodiscard]] InventorySlotDragDropPlan resolveDrop(MenuPanelKind target_kind,
                                                        int target_slot_index,
                                                        const SlotGridDragInfo& drag_info,
                                                        const game::component::HotbarComponent* hotbar);
    [[nodiscard]] InventorySlotDragEndPlan resolveDragEnd(MenuPanelKind end_kind,
                                                          int end_slot_index,
                                                          const SlotGridDragInfo& drag_info);

private:
    SlotGridDragState state_{};
};

} // namespace game::ui
