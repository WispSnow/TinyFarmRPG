#pragma once

#include <entt/signal/fwd.hpp>

namespace game::defs {
struct EquipItemCommand;
struct UnequipItemCommand;
}

namespace game::domain {
class EquipmentDomainService;
}

namespace game::system {

class EquipmentSystem final {
    entt::dispatcher& dispatcher_;
    game::domain::EquipmentDomainService& equipment_domain_service_;

public:
    EquipmentSystem(entt::dispatcher& dispatcher,
                    game::domain::EquipmentDomainService& equipment_domain_service);
    ~EquipmentSystem();

private:
    void onEquipItem(const game::defs::EquipItemCommand& command);
    void onUnequipItem(const game::defs::UnequipItemCommand& command);
};

} // namespace game::system
