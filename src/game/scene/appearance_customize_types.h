#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::component {
struct AppearanceComponent;
}

namespace game::data {
class AppearanceCatalog;
struct AppearanceProfile;
}

namespace game::scene {

struct AppearanceSelection {
    std::string profile_id{};
    std::string gender{"male"};
    std::unordered_map<std::string, std::string> slot_variants{};
};

/// Builds a mutable appearance selection from a catalog profile.
[[nodiscard]] AppearanceSelection makeSelectionFromProfile(const game::data::AppearanceProfile& profile);

/// Returns the catalog default profile as a mutable selection.
[[nodiscard]] AppearanceSelection makeDefaultAppearanceSelection(const game::data::AppearanceCatalog& catalog);

/// Creates a selection from a live component, falling back to catalog defaults for missing fields.
[[nodiscard]] AppearanceSelection makeSelectionFromComponent(const game::component::AppearanceComponent& appearance,
                                                            const game::data::AppearanceCatalog& catalog);

/// Lists catalog slots that are safe to switch at runtime, preserving layer order.
[[nodiscard]] std::vector<std::string> runtimeAppearanceSlots(const game::data::AppearanceCatalog& catalog);

/// Lists editable appearance controls for the customization scene.
[[nodiscard]] std::vector<std::string> appearanceControlSlots(const game::data::AppearanceCatalog& catalog,
                                                              bool include_gender);

/// Steps one runtime-switchable slot forward or backward through its variant list.
[[nodiscard]] bool stepAppearanceSlot(AppearanceSelection& selection,
                                      const game::data::AppearanceCatalog& catalog,
                                      std::string_view slot,
                                      int direction);

/// Steps an appearance control, including the gender pseudo-slot when enabled.
[[nodiscard]] bool stepAppearanceControl(AppearanceSelection& selection,
                                         const game::data::AppearanceCatalog& catalog,
                                         std::string_view slot,
                                         int direction,
                                         bool allow_gender);

/// Resets runtime-switchable slots to the selected profile defaults.
[[nodiscard]] bool resetSelectionToProfile(AppearanceSelection& selection,
                                           const game::data::AppearanceCatalog& catalog);

/// Chooses a random variant for each runtime-switchable slot.
[[nodiscard]] bool randomizeSelection(AppearanceSelection& selection,
                                      const game::data::AppearanceCatalog& catalog,
                                      std::mt19937& rng,
                                      bool include_gender = false);

/// Copies a selection onto an AppearanceComponent and marks it dirty for cache rebuild.
void applySelectionToComponent(const AppearanceSelection& selection,
                               game::component::AppearanceComponent& appearance);

/// Applies a selection to an entity and emits one RefreshAppearanceCommand after the batch write.
[[nodiscard]] bool applySelectionToEntity(entt::registry& registry,
                                          entt::dispatcher& dispatcher,
                                          entt::entity entity,
                                          const AppearanceSelection& selection);

} // namespace game::scene
