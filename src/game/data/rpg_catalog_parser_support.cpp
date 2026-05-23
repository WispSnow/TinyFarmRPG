#include "game/data/rpg_catalog_parser_support.h"

#include "game/data/rpg_catalog.h"

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace game::data {
namespace {

[[nodiscard]] std::optional<ParamCurveShape> paramCurveShapeFromString(const std::string_view value) {
    if (value == "linear" || value.empty()) {
        return ParamCurveShape::Linear;
    }
    if (value == "early") {
        return ParamCurveShape::Early;
    }
    if (value == "late") {
        return ParamCurveShape::Late;
    }
    return std::nullopt;
}

[[nodiscard]] bool parseVec2(const RpgCatalogJson& node, glm::vec2& out_value) {
    out_value = glm::vec2{0.0F, 0.0F};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    const auto x_it = node.find("x");
    if (x_it != node.end()) {
        if (!x_it->is_number()) {
            return false;
        }
        out_value.x = x_it->get<float>();
    }

    const auto y_it = node.find("y");
    if (y_it != node.end()) {
        if (!y_it->is_number()) {
            return false;
        }
        out_value.y = y_it->get<float>();
    }
    return true;
}

} // namespace

bool isValidRpgRate(const int value) {
    return value >= 0 && value <= 100;
}

bool isValidRpgChance(const float value) {
    return value >= 0.0F && value <= 1.0F;
}

bool parseRpgParamArray(const RpgCatalogJson& node, ParamArray& out_params) {
    out_params.fill(0);
    if (node.is_array()) {
        if (node.size() != kParamCount) {
            return false;
        }
        for (std::size_t i = 0; i < kParamCount; ++i) {
            if (!node[i].is_number_integer()) {
                return false;
            }
            out_params[i] = node[i].get<int>();
        }
        return true;
    }

    if (!node.is_object()) {
        return false;
    }

    for (std::size_t i = 0; i < kParamCount; ++i) {
        const auto index = static_cast<ParamIndex>(i);
        const auto key = std::string(toString(index));
        const auto it = node.find(key);
        if (it == node.end() || !it->is_number_integer()) {
            return false;
        }
        out_params[i] = it->get<int>();
    }
    return true;
}

bool parseRpgExpCurve(const RpgCatalogJson& node, ExpCurveData& out_curve) {
    out_curve = {};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    out_curve.basis_ = std::max(1, node.value("basis", out_curve.basis_));
    out_curve.extra_ = std::max(0, node.value("extra", out_curve.extra_));
    out_curve.acc_a_ = std::max(0, node.value("acc_a", out_curve.acc_a_));
    out_curve.acc_b_ = std::max(1, node.value("acc_b", out_curve.acc_b_));
    return true;
}

bool parseRpgParamCurves(const RpgCatalogJson& node,
                         const ParamArray& base_params,
                         std::array<ParamCurveData, kParamCount>& out_curves,
                         bool& out_has_curves) {
    out_has_curves = false;
    for (std::size_t i = 0; i < kParamCount; ++i) {
        out_curves[i] = ParamCurveData{
            .level_1_ = base_params[i],
            .level_max_ = base_params[i],
            .shape_ = ParamCurveShape::Linear,
            .configured_ = false,
        };
    }

    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    for (const auto& [key, value] : node.items()) {
        const auto index = paramIndexFromString(key);
        if (!index.has_value() || !value.is_object()) {
            return false;
        }

        auto& curve = out_curves[static_cast<std::size_t>(*index)];
        curve.level_1_ = value.value("level_1", base_params[static_cast<std::size_t>(*index)]);
        curve.level_max_ = value.value("level_99", curve.level_1_);
        if (const auto level_max_it = value.find("level_max");
            level_max_it != value.end() && level_max_it->is_number_integer()) {
            curve.level_max_ = level_max_it->get<int>();
        }

        const auto shape = paramCurveShapeFromString(value.value("shape", std::string{"linear"}));
        if (!shape.has_value()) {
            return false;
        }
        curve.shape_ = *shape;
        curve.configured_ = true;
        out_has_curves = true;
    }
    return true;
}

bool parseRpgParamBonusArray(const RpgCatalogJson& node, ParamArray& out_params) {
    out_params.fill(0);
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    for (const auto& [key, value] : node.items()) {
        const auto index = paramIndexFromString(key);
        if (!index.has_value() || !value.is_number_integer()) {
            return false;
        }
        out_params[static_cast<std::size_t>(*index)] = value.get<int>();
    }
    return true;
}

bool parseRpgTraitList(const RpgCatalogJson& node, std::vector<TraitData>& out_traits) {
    out_traits.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        return false;
    }

    for (const auto& trait_node : node) {
        if (!trait_node.is_object()) {
            return false;
        }

        TraitData trait{};
        const auto type = traitTypeFromString(trait_node.value("type", std::string{}));
        if (!type.has_value()) {
            return false;
        }
        trait.type = *type;
        trait.target = trait_node.value("target", std::string{});
        trait.value = trait_node.value("value", 0.0F);
        out_traits.push_back(std::move(trait));
    }
    return true;
}

bool parseRpgEffectList(const RpgCatalogJson& node, std::vector<EffectData>& out_effects) {
    out_effects.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        return false;
    }

    for (const auto& effect_node : node) {
        if (!effect_node.is_object()) {
            return false;
        }

        EffectData effect{};
        const auto type = effectTypeFromString(effect_node.value("type", std::string{}));
        if (!type.has_value()) {
            return false;
        }

        effect.type = *type;
        effect.target_id = effect_node.value("target_id", std::string{});
        effect.value1 = effect_node.value("value1", 0.0F);
        effect.value2 = effect_node.value("value2", 0.0F);
        effect.count = effect_node.value("count", 0);
        out_effects.push_back(std::move(effect));
    }
    return true;
}

bool parseRpgDamageData(const RpgCatalogJson& node, DamageFormulaData& out_damage) {
    if (!node.is_object()) {
        return false;
    }

    const auto damage_type = damageTypeFromString(node.value("type", std::string{"none"}));
    if (!damage_type.has_value()) {
        return false;
    }

    out_damage.type = *damage_type;
    out_damage.formula = node.value("formula", std::string{});
    out_damage.variance = node.value("variance", 0);
    out_damage.critical = node.value("critical", false);
    return true;
}

bool parseRpgSkillPresentation(const RpgCatalogJson& node, SkillPresentationData& out_presentation) {
    out_presentation = {};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    out_presentation.configured_ = true;

    const auto motion_style = skillMotionStyleFromString(node.value("motion_style", std::string{"auto"}));
    if (!motion_style.has_value()) {
        return false;
    }
    out_presentation.motion_style_ = *motion_style;

    out_presentation.duration_seconds_ = node.value("duration", 0.0F);
    out_presentation.impact_time_seconds_ = node.value("impact_time", 0.0F);
    out_presentation.recovery_duration_seconds_ = node.value("recovery_duration", 0.0F);
    out_presentation.target_vfx_tail_seconds_ = node.value("target_vfx_tail", 0.0F);

    if (out_presentation.duration_seconds_ <= 0.0F ||
        out_presentation.impact_time_seconds_ < 0.0F ||
        out_presentation.impact_time_seconds_ > out_presentation.duration_seconds_ ||
        out_presentation.recovery_duration_seconds_ < 0.0F ||
        out_presentation.target_vfx_tail_seconds_ < 0.0F ||
        out_presentation.duration_seconds_ <
            out_presentation.impact_time_seconds_ + out_presentation.recovery_duration_seconds_ ||
        out_presentation.duration_seconds_ <
            out_presentation.impact_time_seconds_ + out_presentation.target_vfx_tail_seconds_) {
        return false;
    }

    out_presentation.target_vfx_id_ = node.value("target_vfx_id", std::string{});
    out_presentation.target_vfx_id_hash_ = out_presentation.target_vfx_id_.empty()
        ? entt::id_type{}
        : RpgCatalog::hashId(out_presentation.target_vfx_id_);
    out_presentation.target_sfx_id_ = node.value("target_sfx_id", std::string{});
    out_presentation.target_sfx_id_hash_ = out_presentation.target_sfx_id_.empty()
        ? entt::id_type{}
        : RpgCatalog::hashId(out_presentation.target_sfx_id_);
    out_presentation.target_vfx_scale_ = node.value("target_vfx_scale", 1.0F);
    if (out_presentation.target_vfx_scale_ <= 0.0F) {
        return false;
    }

    const auto target_vfx_offset_it = node.find("target_vfx_offset");
    if (target_vfx_offset_it != node.end() &&
        !parseVec2(*target_vfx_offset_it, out_presentation.target_vfx_offset_)) {
        return false;
    }

    return true;
}

bool parseRpgStringList(const RpgCatalogJson& node, std::vector<std::string>& out_values) {
    out_values.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        return false;
    }

    out_values.reserve(node.size());
    for (const auto& value_node : node) {
        if (!value_node.is_string()) {
            return false;
        }
        out_values.push_back(value_node.get<std::string>());
    }
    return true;
}

bool parseRpgPortraitRef(const RpgCatalogJson& node, PortraitRefData& out_portrait) {
    out_portrait = {};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    out_portrait.path_ = node.value("path", std::string{});
    out_portrait.path_hash_ = out_portrait.path_.empty() ? entt::null : RpgCatalog::hashId(out_portrait.path_);
    out_portrait.decorator_ = node.value("decorator", std::string{});
    out_portrait.x_ = node.value("x", 0);
    out_portrait.y_ = node.value("y", 0);
    out_portrait.width_ = node.value("width", 0);
    out_portrait.height_ = node.value("height", 0);

    return out_portrait.path_.empty() || out_portrait.valid();
}

bool parseRpgBattleVisual(const RpgCatalogJson& node, BattleVisualData& out_visual) {
    out_visual = {};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        return false;
    }

    out_visual.sprite_blueprint_id_ = node.value("sprite_blueprint_id", std::string{});
    out_visual.idle_animation_ = node.value("idle_animation", std::string{"idle_right"});
    out_visual.sprite_scale_ = node.value("sprite_scale", 1.0F);
    if (out_visual.sprite_scale_ <= 0.0F) {
        return false;
    }
    if (const auto shadow_offset_it = node.find("shadow_offset");
        shadow_offset_it != node.end() && !parseVec2(*shadow_offset_it, out_visual.shadow_offset_)) {
        return false;
    }

    out_visual.sprite_blueprint_id_hash_ = out_visual.sprite_blueprint_id_.empty()
        ? entt::null
        : RpgCatalog::hashId(out_visual.sprite_blueprint_id_);
    return out_visual.sprite_blueprint_id_.empty() || !out_visual.idle_animation_.empty();
}

} // namespace game::data
