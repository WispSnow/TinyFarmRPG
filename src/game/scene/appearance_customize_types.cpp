#include "game/scene/appearance_customize_types.h"

#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <utility>

namespace game::scene {
namespace {

[[nodiscard]] const game::data::AppearanceProfile* resolveProfile(
    const game::data::AppearanceCatalog& catalog,
    std::string_view profile_id) {
    if (!profile_id.empty()) {
        if (const auto* profile = catalog.findProfile(profile_id)) {
            return profile;
        }
    }
    return catalog.defaultProfile();
}

[[nodiscard]] std::size_t currentVariantIndex(const std::vector<std::string>& variants,
                                              std::string_view current_variant) {
    const auto it = std::find(variants.begin(), variants.end(), current_variant);
    if (it == variants.end()) {
        return 0U;
    }
    return static_cast<std::size_t>(std::distance(variants.begin(), it));
}

} // namespace

AppearanceSelection makeSelectionFromProfile(const game::data::AppearanceProfile& profile) {
    return AppearanceSelection{
        .profile_id = profile.id_,
        .gender = profile.gender_,
        .slot_variants = profile.slots_,
    };
}

AppearanceSelection makeDefaultAppearanceSelection(const game::data::AppearanceCatalog& catalog) {
    if (const auto* profile = catalog.defaultProfile()) {
        return makeSelectionFromProfile(*profile);
    }
    return {};
}

AppearanceSelection makeSelectionFromComponent(const game::component::AppearanceComponent& appearance,
                                               const game::data::AppearanceCatalog& catalog) {
    AppearanceSelection selection{};
    if (const auto* profile = resolveProfile(catalog, appearance.profile_id_)) {
        selection = makeSelectionFromProfile(*profile);
    }

    if (!appearance.profile_id_.empty()) {
        selection.profile_id = appearance.profile_id_;
    }
    if (!appearance.gender_.empty()) {
        selection.gender = appearance.gender_;
    }
    for (const auto& [slot, variant] : appearance.slot_variants_) {
        selection.slot_variants[slot] = variant;
    }
    return selection;
}

std::vector<std::string> runtimeAppearanceSlots(const game::data::AppearanceCatalog& catalog) {
    std::vector<std::string> slots;
    for (const auto& slot : catalog.layerOrder()) {
        if (!catalog.isRuntimeSwitchableSlot(slot)) {
            continue;
        }
        if (catalog.variantsForSlot(slot).empty()) {
            continue;
        }
        slots.push_back(slot);
    }
    return slots;
}

bool stepAppearanceSlot(AppearanceSelection& selection,
                        const game::data::AppearanceCatalog& catalog,
                        std::string_view slot,
                        int direction) {
    if (!catalog.isRuntimeSwitchableSlot(slot)) {
        return false;
    }

    const auto& variants = catalog.variantsForSlot(slot);
    if (variants.empty()) {
        return false;
    }

    const auto slot_key = std::string(slot);
    const auto current_it = selection.slot_variants.find(slot_key);
    const std::string_view current_variant =
        current_it == selection.slot_variants.end() ? std::string_view{} : std::string_view(current_it->second);
    const std::size_t current_index = currentVariantIndex(variants, current_variant);
    const int normalized_direction = direction < 0 ? -1 : 1;
    const std::size_t next_index = normalized_direction < 0
                                       ? (current_index == 0U ? variants.size() - 1U : current_index - 1U)
                                       : ((current_index + 1U) % variants.size());
    selection.slot_variants[slot_key] = variants[next_index];
    return true;
}

bool resetSelectionToProfile(AppearanceSelection& selection,
                             const game::data::AppearanceCatalog& catalog) {
    const auto* profile = resolveProfile(catalog, selection.profile_id);
    if (!profile) {
        return false;
    }

    selection.gender = profile->gender_;
    for (const auto& [slot, variant] : profile->slots_) {
        if (!catalog.isRuntimeSwitchableSlot(slot)) {
            continue;
        }
        selection.slot_variants[slot] = variant;
    }
    return true;
}

bool randomizeSelection(AppearanceSelection& selection,
                        const game::data::AppearanceCatalog& catalog,
                        std::mt19937& rng) {
    bool changed = false;
    for (const auto& slot : runtimeAppearanceSlots(catalog)) {
        const auto& variants = catalog.variantsForSlot(slot);
        if (variants.empty()) {
            continue;
        }
        std::uniform_int_distribution<std::size_t> distribution(0U, variants.size() - 1U);
        selection.slot_variants[slot] = variants[distribution(rng)];
        changed = true;
    }
    return changed;
}

void applySelectionToComponent(const AppearanceSelection& selection,
                               game::component::AppearanceComponent& appearance) {
    appearance.profile_id_ = selection.profile_id;
    if (!selection.gender.empty()) {
        appearance.gender_ = selection.gender;
    }
    for (const auto& [slot, variant] : selection.slot_variants) {
        appearance.slot_variants_[slot] = variant;
    }
    appearance.dirty_ = true;
}

bool applySelectionToEntity(entt::registry& registry,
                            entt::dispatcher& dispatcher,
                            entt::entity entity,
                            const AppearanceSelection& selection) {
    if (entity == entt::null || !registry.valid(entity)) {
        return false;
    }
    auto* appearance = registry.try_get<game::component::AppearanceComponent>(entity);
    if (!appearance) {
        return false;
    }

    applySelectionToComponent(selection, *appearance);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
    return true;
}

} // namespace game::scene
