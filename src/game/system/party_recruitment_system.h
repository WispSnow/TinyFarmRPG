#pragma once

#include "game/defs/commands.h"
#include "game/system/system_helpers.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <string>

namespace game::data {
class RpgCatalog;
}

namespace game::system {

class PartyRecruitmentSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::RpgCatalog& rpg_catalog_;
    helpers::NotificationTimer notification_{};

public:
    PartyRecruitmentSystem(entt::registry& registry,
                           entt::dispatcher& dispatcher,
                           const game::data::RpgCatalog& rpg_catalog);
    ~PartyRecruitmentSystem();

    void update(float delta_time);

private:
    void onRecruitPartyMemberCommand(const game::defs::RecruitPartyMemberCommand& command);
    void showNotification(entt::entity target, std::string text);
};

} // namespace game::system
