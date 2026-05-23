#pragma once

#include "game/component/party_runtime_stats_component.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::domain {

struct RestRecoveryMemberPreview {
    std::string actor_id{};
    std::string display_name{};
    int current_hp{0};
    int after_hp{0};
    int max_hp{1};
    int current_mp{0};
    int after_mp{0};
    int max_mp{1};

    [[nodiscard]] int hpGain() const { return after_hp - current_hp; }
    [[nodiscard]] int mpGain() const { return after_mp - current_mp; }
    [[nodiscard]] bool recovered() const { return hpGain() > 0 || mpGain() > 0; }
};

struct RestRecoveryPreview {
    int hours{0};
    int recovery_percent{0};
    bool full_recovery{false};
    std::vector<RestRecoveryMemberPreview> members{};

    [[nodiscard]] bool anyRecovered() const;
    [[nodiscard]] bool empty() const { return members.empty(); }
};

struct PartyRestApplyResult {
    RestRecoveryPreview preview{};
    bool runtime_state_changed{false};
};

/// @brief Computes and applies party HP/MP recovery caused by resting.
class PartyRestService final {
public:
    [[nodiscard]] static RestRecoveryPreview previewActivePartyRecovery(
        const entt::registry& registry,
        entt::entity player,
        const game::data::RpgCatalog& rpg_catalog,
        int hours);

    [[nodiscard]] static PartyRestApplyResult applyActivePartyRecovery(
        entt::registry& registry,
        entt::entity player,
        const game::data::RpgCatalog& rpg_catalog,
        int hours);
};

} // namespace game::domain
