#include "game/battle/actor_stats_resolver.h"

#include "game/component/party_equipment_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"

#include <algorithm>

namespace game::battle {
namespace {

[[nodiscard]] game::data::ParamArray baseParamsForActor(const game::data::RpgCatalog& catalog,
                                                        const game::data::ActorData& actor) {
    game::data::ParamArray params{};
    params.fill(1);
    if (const auto* klass = catalog.findClass(actor.class_id_)) {
        params = klass->base_params_;
    }
    return params;
}

void applyEquipmentBonuses(const game::data::RpgCatalog& catalog,
                           const game::component::ActorEquipmentLoadout* loadout,
                           game::data::ParamArray& params) {
    if (!loadout) {
        return;
    }

    for (const auto& [slot, item_id] : loadout->equipped_item_ids_) {
        (void)slot;
        if (const auto* equipment = catalog.findEquipmentByItem(item_id)) {
            for (std::size_t i = 0; i < game::data::kParamCount; ++i) {
                params[i] += equipment->param_bonuses_[i];
            }
        }
    }
}

void clampMinimumStats(game::data::ParamArray& params) {
    for (auto& value : params) {
        value = std::max(1, value);
    }
}

} // namespace

ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                     const game::data::ActorData& actor,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    ActorResolvedStats result{};
    result.params = baseParamsForActor(catalog, actor);
    applyEquipmentBonuses(catalog, loadout, result.params);
    clampMinimumStats(result.params);
    return result;
}

ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                     const std::string_view actor_id,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    if (const auto* actor = catalog.findActor(actor_id)) {
        return resolveActorStats(catalog, *actor, loadout);
    }

    ActorResolvedStats result{};
    result.params.fill(1);
    return result;
}

} // namespace game::battle
