#include "game/system/equipment_system.h"

#include "game/defs/commands.h"
#include "game/domain/equipment_domain_service.h"

#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace game::system {

EquipmentSystem::EquipmentSystem(entt::dispatcher& dispatcher,
                                 game::domain::EquipmentDomainService& equipment_domain_service)
    : dispatcher_(dispatcher),
      equipment_domain_service_(equipment_domain_service) {
    dispatcher_.sink<game::defs::EquipItemCommand>().connect<&EquipmentSystem::onEquipItem>(this);
    dispatcher_.sink<game::defs::UnequipItemCommand>().connect<&EquipmentSystem::onUnequipItem>(this);
}

EquipmentSystem::~EquipmentSystem() {
    dispatcher_.disconnect(this);
}

void EquipmentSystem::onEquipItem(const game::defs::EquipItemCommand& command) {
    const auto result = equipment_domain_service_.equipItem(
        command.player,
        command.actor_id,
        command.inventory_slot_index,
        command.target_slot);
    if (!result.success && !result.message.empty()) {
        spdlog::warn("EquipmentSystem: equip rejected: {}", result.message);
    }
}

void EquipmentSystem::onUnequipItem(const game::defs::UnequipItemCommand& command) {
    const auto result = equipment_domain_service_.unequipItem(
        command.player,
        command.actor_id,
        command.slot,
        command.preferred_inventory_slot);
    if (!result.success && !result.message.empty()) {
        spdlog::warn("EquipmentSystem: unequip rejected: {}", result.message);
    }
}

} // namespace game::system
