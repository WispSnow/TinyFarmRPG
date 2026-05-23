#pragma once

#include "game/data/rpg_data.h"

#include <nlohmann/json.hpp>

#include <array>
#include <string>
#include <vector>

namespace game::data {

using RpgCatalogJson = nlohmann::json;

[[nodiscard]] bool isValidRpgRate(int value);
[[nodiscard]] bool isValidRpgChance(float value);
[[nodiscard]] bool parseRpgParamArray(const RpgCatalogJson& node, ParamArray& out_params);
[[nodiscard]] bool parseRpgExpCurve(const RpgCatalogJson& node, ExpCurveData& out_curve);
[[nodiscard]] bool parseRpgParamCurves(const RpgCatalogJson& node,
                                       const ParamArray& base_params,
                                       std::array<ParamCurveData, kParamCount>& out_curves,
                                       bool& out_has_curves);
[[nodiscard]] bool parseRpgParamBonusArray(const RpgCatalogJson& node, ParamArray& out_params);
[[nodiscard]] bool parseRpgTraitList(const RpgCatalogJson& node, std::vector<TraitData>& out_traits);
[[nodiscard]] bool parseRpgEffectList(const RpgCatalogJson& node, std::vector<EffectData>& out_effects);
[[nodiscard]] bool parseRpgDamageData(const RpgCatalogJson& node, DamageFormulaData& out_damage);
[[nodiscard]] bool parseRpgSkillPresentation(const RpgCatalogJson& node, SkillPresentationData& out_presentation);
[[nodiscard]] bool parseRpgStringList(const RpgCatalogJson& node, std::vector<std::string>& out_values);
[[nodiscard]] bool parseRpgPortraitRef(const RpgCatalogJson& node, PortraitRefData& out_portrait);
[[nodiscard]] bool parseRpgBattleVisual(const RpgCatalogJson& node, BattleVisualData& out_visual);

} // namespace game::data
