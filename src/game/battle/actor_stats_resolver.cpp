#include "game/battle/actor_stats_resolver.h"

#include "game/component/party_equipment_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"

#include <algorithm>
#include <cmath>

namespace game::battle {
namespace {

[[nodiscard]] float curveT(const int level, const int max_level) {
    if (max_level <= 1) {
        return 0.0F;
    }
    return std::clamp(
        static_cast<float>(level - 1) / static_cast<float>(max_level - 1),
        0.0F,
        1.0F);
}

[[nodiscard]] float shapedT(const float t, const game::data::ParamCurveShape shape) {
    switch (shape) {
        case game::data::ParamCurveShape::Linear:
            return t;
        case game::data::ParamCurveShape::Early:
            return std::pow(t, 0.6F);
        case game::data::ParamCurveShape::Late:
            return std::pow(t, 1.5F);
    }
    return t;
}

[[nodiscard]] int interpolateParamCurve(const game::data::ParamCurveData& curve,
                                        const int level,
                                        const int max_level) {
    const float t = shapedT(curveT(level, max_level), curve.shape_);
    const float value = static_cast<float>(curve.level_1_) +
        static_cast<float>(curve.level_max_ - curve.level_1_) * t;
    return static_cast<int>(std::round(value));
}

[[nodiscard]] game::data::ParamArray baseParamsForActor(const game::data::RpgCatalog& catalog,
                                                        const game::data::ActorData& actor,
                                                        const int level) {
    game::data::ParamArray params{};
    params.fill(1);
    if (const auto* klass = catalog.findClass(actor.class_id_)) {
        params = klass->base_params_;
        if (klass->has_param_curves_) {
            const int max_level = std::max(actor.initial_level_, actor.max_level_);
            const int clamped_level = std::clamp(level, 1, max_level);
            for (std::size_t i = 0; i < game::data::kParamCount; ++i) {
                const auto& curve = klass->param_curves_[i];
                if (curve.configured_) {
                    params[i] = interpolateParamCurve(curve, clamped_level, max_level);
                }
            }
        }
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
                                     const int level,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    ActorResolvedStats result{};
    result.params = baseParamsForActor(catalog, actor, level);
    applyEquipmentBonuses(catalog, loadout, result.params);
    clampMinimumStats(result.params);
    return result;
}

ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                     const game::data::ActorData& actor,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    return resolveActorStats(catalog, actor, actor.initial_level_, loadout);
}

ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                     const std::string_view actor_id,
                                     const int level,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    if (const auto* actor = catalog.findActor(actor_id)) {
        return resolveActorStats(catalog, *actor, level, loadout);
    }

    ActorResolvedStats result{};
    result.params.fill(1);
    return result;
}

ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                     const std::string_view actor_id,
                                     const game::component::ActorEquipmentLoadout* loadout) {
    if (const auto* actor = catalog.findActor(actor_id)) {
        return resolveActorStats(catalog, *actor, actor->initial_level_, loadout);
    }

    ActorResolvedStats result{};
    result.params.fill(1);
    return result;
}

} // namespace game::battle
