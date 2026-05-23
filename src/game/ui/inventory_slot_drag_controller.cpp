#include "game/ui/inventory_slot_drag_controller.h"

#include "game/component/hotbar_component.h"

namespace game::ui {

InventorySlotDragCommand InventorySlotDragCommand::inventoryMove(const int source_inventory_slot,
                                                                 const int target_inventory_slot,
                                                                 const bool merge_stacks) {
    return InventorySlotDragCommand{
        .kind = InventorySlotDragCommandKind::InventoryMove,
        .inventory_slot_index = source_inventory_slot,
        .target_inventory_slot_index = target_inventory_slot,
        .allow_merge = merge_stacks,
    };
}

InventorySlotDragCommand InventorySlotDragCommand::hotbarBind(const int hotbar_slot, const int inventory_slot) {
    return InventorySlotDragCommand{
        .kind = InventorySlotDragCommandKind::HotbarBind,
        .inventory_slot_index = inventory_slot,
        .hotbar_slot_index = hotbar_slot,
    };
}

void InventorySlotDragController::start() {
    state_.start();
}

void InventorySlotDragController::clear() {
    state_.clear();
}

InventorySlotDragDropPlan InventorySlotDragController::resolveDrop(
    const MenuPanelKind target_kind,
    const int target_slot_index,
    const SlotGridDragInfo& drag_info,
    const game::component::HotbarComponent* hotbar) {
    if (!state_.active) {
        return {};
    }

    InventorySlotDragDropPlan plan{.handled = true};
    state_.drop_handled = true;

    if (target_kind == MenuPanelKind::Backpack) {
        if (drag_info.fromHotbar()) {
            if (!hotbar || drag_info.slot_index >= game::component::HotbarComponent::SLOT_COUNT) {
                return plan;
            }

            const int source_inventory_slot = hotbar->slot(drag_info.slot_index).inventory_slot_index_;
            if (source_inventory_slot >= 0 && source_inventory_slot != target_slot_index) {
                plan.commands.push_back(
                    InventorySlotDragCommand::inventoryMove(source_inventory_slot, target_slot_index, true));
            }
            return plan;
        }

        if (drag_info.slot_index != target_slot_index) {
            plan.commands.push_back(
                InventorySlotDragCommand::inventoryMove(drag_info.slot_index, target_slot_index, true));
        }
        return plan;
    }

    if (target_kind != MenuPanelKind::Hotbar) {
        return plan;
    }

    if (drag_info.fromHotbar()) {
        if (drag_info.slot_index == target_slot_index) {
            return plan;
        }
        if (!hotbar || drag_info.slot_index >= game::component::HotbarComponent::SLOT_COUNT) {
            return plan;
        }

        const int source_inventory_slot = hotbar->slot(drag_info.slot_index).inventory_slot_index_;
        if (source_inventory_slot < 0) {
            return plan;
        }
        if (hotbar->slot(target_slot_index).empty()) {
            plan.commands.push_back(InventorySlotDragCommand::hotbarBind(target_slot_index, source_inventory_slot));
            return plan;
        }

        const int target_inventory_slot = hotbar->slot(target_slot_index).inventory_slot_index_;
        plan.commands.push_back(InventorySlotDragCommand::hotbarBind(target_slot_index, source_inventory_slot));
        plan.commands.push_back(InventorySlotDragCommand::hotbarBind(drag_info.slot_index, target_inventory_slot));
        return plan;
    }

    plan.commands.push_back(InventorySlotDragCommand::hotbarBind(target_slot_index, drag_info.slot_index));
    return plan;
}

InventorySlotDragEndPlan InventorySlotDragController::resolveDragEnd(const MenuPanelKind end_kind,
                                                                     const int end_slot_index,
                                                                     const SlotGridDragInfo& drag_info) {
    if (!state_.active) {
        return {};
    }

    if (end_kind == MenuPanelKind::Backpack) {
        if (!drag_info.fromInventory() || end_slot_index != drag_info.slot_index) {
            return {};
        }

        clear();
        return InventorySlotDragEndPlan{.handled = true};
    }

    if (end_kind != MenuPanelKind::Hotbar || !drag_info.fromHotbar() || end_slot_index != drag_info.slot_index) {
        return {};
    }

    const bool should_unbind = !state_.drop_handled;
    clear();
    return InventorySlotDragEndPlan{
        .handled = true,
        .unbind_hotbar = should_unbind,
        .hotbar_slot_index = drag_info.slot_index,
    };
}

} // namespace game::ui
