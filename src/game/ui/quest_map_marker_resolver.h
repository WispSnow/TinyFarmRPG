#pragma once

#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/ui/map_marker_provider.h"

#include <glm/vec2.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>

namespace game::runtime {
class LocalizationService;
}

namespace game::ui {

enum class QuestRuntimeMarkerKind : std::uint8_t {
    Offer = 0,
    Objective,
    TurnIn,
};

/// @brief Fully resolved quest marker ready for Map tab rendering.
///
/// Instances are derived from the current quest log, quest catalog, objective marker data, and
/// current-map quest giver locations. They are never persisted.
struct QuestRuntimeMarker {
    QuestRuntimeMarkerKind kind{QuestRuntimeMarkerKind::Offer};
    int object_id{0};
    std::string quest_id{};
    std::string objective_id{};
    glm::vec2 map_position{};
    std::string title{};
    std::string type_label{};
    std::string description{};
};

/// @brief Derive current-map quest runtime markers for offer, objective, and turn-in states.
///
/// This is a pure read-only layer: it does not mutate @p quest_log, does not depend on WorldState,
/// and treats a missing objective progress key as zero progress.
[[nodiscard]] std::vector<QuestRuntimeMarker> resolveQuestMapMarkers(
    entt::id_type current_map_id,
    const game::component::QuestLogComponent& quest_log,
    const game::data::QuestCatalog& quest_catalog,
    std::span<const QuestGiverLocation> quest_givers,
    const game::runtime::LocalizationService* localization = nullptr);

} // namespace game::ui
