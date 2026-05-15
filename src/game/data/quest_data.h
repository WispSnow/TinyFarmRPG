#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace game::data {

inline constexpr std::string_view kQuestObjectiveProgressKeySeparator = "::";

[[nodiscard]] inline bool containsReservedQuestProgressKeySeparator(const std::string_view value) {
    return value.find(kQuestObjectiveProgressKeySeparator) != std::string_view::npos;
}

[[nodiscard]] inline std::string makeQuestObjectiveProgressKey(const std::string_view quest_id,
                                                               const std::string_view objective_id) {
    std::string key{};
    key.reserve(quest_id.size() + kQuestObjectiveProgressKeySeparator.size() + objective_id.size());
    key.append(quest_id);
    key.append(kQuestObjectiveProgressKeySeparator);
    key.append(objective_id);
    return key;
}

enum class QuestObjectiveKind : std::uint8_t {
    DefeatEnemyCount = 0
};

struct QuestObjectiveMarkerData {
    std::string map_name_{};
    entt::id_type map_id_hash_{entt::null};
    glm::vec2 position_{};
    std::string label_{};
};

struct QuestObjectiveData {
    std::string id_{};
    QuestObjectiveKind kind_{QuestObjectiveKind::DefeatEnemyCount};
    std::string enemy_id_{};
    entt::id_type enemy_id_hash_{entt::null};
    int required_count_{0};
    std::optional<QuestObjectiveMarkerData> marker_{};
};

struct QuestRewardItemData {
    std::string item_id_{};
    entt::id_type item_id_hash_{entt::null};
    int count_{0};
};

struct QuestRewardData {
    int gold_{0};
    std::vector<QuestRewardItemData> items_{};

    [[nodiscard]] bool empty() const { return gold_ == 0 && items_.empty(); }
};

struct QuestGiverTextData {
    std::string offer_{};
    std::string progress_{};
    std::string ready_to_turn_in_{};
    std::string completed_{};
};

struct QuestData {
    std::string id_{};
    entt::id_type id_hash_{entt::null};
    std::string title_{};
    std::string description_{};
    std::vector<QuestObjectiveData> objectives_{};
    QuestRewardData rewards_{};
    QuestGiverTextData giver_text_{};
};

} // namespace game::data
