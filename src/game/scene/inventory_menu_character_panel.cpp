#include "game/scene/inventory_menu_character_panel.h"

#include "game/battle/actor_stats_resolver.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/party_ids.h"
#include "game/domain/actor_progression_service.h"

#include <entt/entity/registry.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace game::scene {
namespace {

constexpr std::size_t kMaxPartyMembers = 4;

[[nodiscard]] std::string portraitDecoratorForActor(const game::data::ActorData* actor) {
    if (!actor || actor->portrait_.decorator_.empty()) {
        return "none";
    }
    return fmt::format("image({})", actor->portrait_.decorator_);
}

[[nodiscard]] std::vector<std::string> activeActorIds(const entt::registry& registry, entt::entity player) {
    if (const auto* party = registry.try_get<game::component::PartyComponent>(player);
        party && !party->active_actor_ids_.empty()) {
        return party->active_actor_ids_;
    }
    return {std::string{game::defs::kDefaultPlayerActorId}};
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(const entt::registry& registry,
                                                                        entt::entity player,
                                                                        std::string_view actor_id) {
    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment) {
        return nullptr;
    }
    const auto it = equipment->loadouts_by_actor_id_.find(std::string{actor_id});
    return it == equipment->loadouts_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] const game::component::ActorRuntimeState* findRuntimeState(const entt::registry& registry,
                                                                         entt::entity player,
                                                                         std::string_view actor_id) {
    const auto* runtime = registry.try_get<game::component::PartyRuntimeStatsComponent>(player);
    if (!runtime) {
        return nullptr;
    }
    const auto it = runtime->states_by_actor_id_.find(std::string{actor_id});
    return it == runtime->states_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] int paramValue(const game::data::ParamArray& params, const game::data::ParamIndex index) {
    return params[static_cast<std::size_t>(index)];
}

} // namespace

InventoryMenuPartyPanelData buildInventoryMenuPartyPanelData(
    const entt::registry& registry,
    entt::entity player,
    const game::data::RpgCatalog* rpg_catalog,
    const std::string_view selected_actor_id,
    const bool target_mode) {
    InventoryMenuPartyPanelData data{};
    data.gold_label = "Gold: 0";
    data.farm_label = "TinyFarm";

    if (const auto* wallet = registry.try_get<game::component::PlayerWalletComponent>(player)) {
        data.gold_label = fmt::format("Gold: {}", wallet->gold_);
    } else {
        spdlog::warn("InventoryMenuScene: 玩家缺少 PlayerWalletComponent，金币显示回退为 0。");
    }

    auto actor_ids = activeActorIds(registry, player);
    if (actor_ids.size() > kMaxPartyMembers) {
        actor_ids.resize(kMaxPartyMembers);
    }

    data.party_members.reserve(kMaxPartyMembers);
    for (std::size_t i = 0; i < kMaxPartyMembers; ++i) {
        PartyMemberPanelViewModel member{};
        member.slot_index = static_cast<int>(i);

        if (i >= actor_ids.size()) {
            member.display_name = "---";
            member.class_label = "Empty";
            member.level_label = "";
            member.exp_label = "";
            member.hp_text = "";
            member.mp_text = "";
            data.party_members.push_back(std::move(member));
            continue;
        }

        member.empty = false;
        member.targetable = target_mode;
        member.actor_id = actor_ids[i];
        member.selected = selected_actor_id.empty() ? i == 0 : selected_actor_id == member.actor_id;

        const auto* actor = rpg_catalog ? rpg_catalog->findActor(member.actor_id) : nullptr;
        const auto* klass = actor && rpg_catalog ? rpg_catalog->findClass(actor->class_id_) : nullptr;
        if (!actor) {
            spdlog::warn("InventoryMenuScene: actor_id='{}' 未在 RpgCatalog 中找到。", member.actor_id);
        }

        member.display_name = actor && !actor->display_name_.empty() ? actor->display_name_ : member.actor_id;
        member.class_label = klass && !klass->display_name_.empty() ? klass->display_name_ : "Adventurer";
        game::component::ActorRuntimeState runtime_snapshot{};
        const auto* runtime_state = findRuntimeState(registry, player, member.actor_id);
        if (actor && rpg_catalog) {
            runtime_snapshot = runtime_state
                ? game::domain::ActorProgressionService::normalizeState(
                      *rpg_catalog,
                      *actor,
                      *runtime_state,
                      findLoadout(registry, player, member.actor_id))
                : game::domain::ActorProgressionService::initialState(
                      *rpg_catalog,
                      *actor,
                      findLoadout(registry, player, member.actor_id));
        } else {
            runtime_snapshot.level = 1;
        }
        const int level = runtime_snapshot.level;
        member.level_label = fmt::format("Lv.{}", level);
        if (actor && rpg_catalog) {
            const int exp_to_next =
                game::domain::ActorProgressionService::expToNextLevel(*rpg_catalog, *actor, runtime_snapshot.total_exp);
            member.exp_label = exp_to_next > 0 ? fmt::format("Next {}", exp_to_next) : "Max";
        }
        member.portrait_decorator = portraitDecoratorForActor(actor);

        int max_hp = 1;
        int max_mp = 1;
        if (actor && rpg_catalog) {
            const auto resolved = game::battle::resolveActorStats(
                *rpg_catalog,
                *actor,
                level,
                findLoadout(registry, player, member.actor_id));
            max_hp = paramValue(resolved.params, game::data::ParamIndex::Mhp);
            max_mp = paramValue(resolved.params, game::data::ParamIndex::Mmp);
        }
        const int current_hp = actor && rpg_catalog ? std::clamp(runtime_snapshot.current_hp, 0, max_hp) : max_hp;
        const int current_mp = actor && rpg_catalog ? std::clamp(runtime_snapshot.current_mp, 0, max_mp) : max_mp;
        member.hp_text = fmt::format("HP {}/{}", current_hp, max_hp);
        member.mp_text = fmt::format("MP {}/{}", current_mp, max_mp);

        data.party_members.push_back(std::move(member));
    }

    return data;
}

} // namespace game::scene
