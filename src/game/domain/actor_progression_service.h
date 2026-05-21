#pragma once

#include "game/component/party_equipment_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/data/rpg_data.h"

#include <entt/entity/fwd.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::domain {

/// @brief 单个 actor 在一次经验结算中的成长结果。
struct ActorExperienceGrant {
    std::string actor_id{};
    std::string display_name{};
    int gained_exp{0};
    int old_level{1};
    int new_level{1};
    int total_exp{0};
    int exp_to_next{0};
    int hp_max_delta{0};
    int mp_max_delta{0};

    [[nodiscard]] bool leveledUp() const { return new_level > old_level; }
};

/// @brief 队伍经验结算摘要。
struct PartyExperienceGrantResult {
    int exp_reward{0};
    bool runtime_state_changed{false}; ///< grantExperience 实际改写了 PartyRuntimeStatsComponent 时为 true。
    std::vector<ActorExperienceGrant> actors{};

    [[nodiscard]] bool empty() const { return exp_reward <= 0 && actors.empty(); }
    [[nodiscard]] bool anyLevelUp() const;
};

/// @brief RPG actor 经验曲线、等级推导和队伍经验写回服务。
///
/// `basis/extra` 控制整体经验需求与线性附加量，`acc_a` 主要影响曲线幂次，
/// `acc_b` 主要影响高等级段压缩或拉伸。默认值沿用 RPG Maker 风格的早期成长手感。
class ActorProgressionService final {
public:
    [[nodiscard]] static int expForLevel(const game::data::ExpCurveData& curve, int level);
    [[nodiscard]] static int expForLevel(const game::data::RpgCatalog& catalog,
                                         const game::data::ActorData& actor,
                                         int level);
    [[nodiscard]] static int levelForExp(const game::data::RpgCatalog& catalog,
                                         const game::data::ActorData& actor,
                                         int total_exp);
    [[nodiscard]] static int expToNextLevel(const game::data::RpgCatalog& catalog,
                                            const game::data::ActorData& actor,
                                            int total_exp);

    [[nodiscard]] static game::component::ActorRuntimeState initialState(
        const game::data::RpgCatalog& catalog,
        const game::data::ActorData& actor,
        const game::component::ActorEquipmentLoadout* loadout);

    [[nodiscard]] static game::component::ActorRuntimeState normalizeState(
        const game::data::RpgCatalog& catalog,
        const game::data::ActorData& actor,
        game::component::ActorRuntimeState state,
        const game::component::ActorEquipmentLoadout* loadout);

    [[nodiscard]] static PartyExperienceGrantResult previewExperience(
        const game::data::RpgCatalog& catalog,
        const std::vector<std::string>& actor_ids,
        const std::unordered_map<std::string, game::component::ActorRuntimeState>& runtime_states,
        const std::unordered_map<std::string, game::component::ActorEquipmentLoadout>& actor_equipment,
        int exp_reward);

    [[nodiscard]] static PartyExperienceGrantResult grantExperience(
        entt::registry& registry,
        entt::entity player,
        const game::data::RpgCatalog& catalog,
        const std::vector<std::string>& actor_ids,
        int exp_reward);
};

} // namespace game::domain
