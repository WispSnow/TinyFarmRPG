#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace game::component {
struct QuestLogComponent;
}

namespace game::data {
class QuestCatalog;
struct QuestData;
struct QuestObjectiveData;
}

namespace game::debug {

enum class QuestDebugFilter : std::uint8_t {
    All = 0,
    Offerable,
    Active,
    ReadyToTurnIn,
    Completed
};

enum class QuestDebugState : std::uint8_t {
    Offerable = 0,
    Active,
    ReadyToTurnIn,
    Completed
};

struct QuestDebugSelectionResult {
    const game::data::QuestData* quest{nullptr};
    std::string error{};

    [[nodiscard]] bool ok() const {
        return quest != nullptr;
    }
};

[[nodiscard]] std::string_view questDebugFilterLabel(QuestDebugFilter filter);
[[nodiscard]] std::string_view questDebugStateLabel(QuestDebugState state);

namespace detail {

[[nodiscard]] QuestDebugState resolveQuestDebugState(const game::component::QuestLogComponent& quest_log,
                                                     const game::data::QuestData& quest);
[[nodiscard]] bool matchesQuestDebugFilter(QuestDebugState state, QuestDebugFilter filter);
[[nodiscard]] int clampQuestObjectiveProgress(const game::component::QuestLogComponent& quest_log,
                                              const game::data::QuestData& quest,
                                              const game::data::QuestObjectiveData& objective);
[[nodiscard]] QuestDebugSelectionResult resolveQuestDebugSelection(const game::data::QuestCatalog* quest_catalog,
                                                                  std::string_view quest_id);

} // namespace detail

} // namespace game::debug
