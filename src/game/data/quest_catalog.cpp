#include "game/data/quest_catalog.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace game::data {
namespace {

using Json = nlohmann::json;

[[nodiscard]] bool parseRequiredString(const Json& object,
                                       const char* key,
                                       const std::string_view owner_label,
                                       std::string& out_value) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        spdlog::error("QuestCatalog: {} 缺少 string 字段 '{}'", owner_label, key);
        return false;
    }

    out_value = it->get<std::string>();
    if (out_value.empty()) {
        spdlog::error("QuestCatalog: {} 的 '{}' 不能为空", owner_label, key);
        return false;
    }
    return true;
}

[[nodiscard]] bool parseOptionalString(const Json& object,
                                       const char* key,
                                       const std::string_view owner_label,
                                       std::string& out_value) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_string()) {
        spdlog::error("QuestCatalog: {} 的 '{}' 必须是 string", owner_label, key);
        return false;
    }
    out_value = it->get<std::string>();
    return true;
}

[[nodiscard]] bool validateIdentifier(const std::string_view value,
                                      const std::string_view field_label,
                                      const std::string_view owner_label) {
    if (value.empty()) {
        spdlog::error("QuestCatalog: {} 的 {} 不能为空", owner_label, field_label);
        return false;
    }
    if (containsReservedQuestProgressKeySeparator(value)) {
        spdlog::error("QuestCatalog: {} 的 {} 不能包含保留分隔符 '{}'",
                      owner_label,
                      field_label,
                      kQuestObjectiveProgressKeySeparator);
        return false;
    }
    return true;
}

[[nodiscard]] bool parseQuestObjectiveKind(const std::string_view value, QuestObjectiveKind& out_kind) {
    if (value == "defeat_enemy_count") {
        out_kind = QuestObjectiveKind::DefeatEnemyCount;
        return true;
    }
    return false;
}

[[nodiscard]] std::string makeQuestObjectiveLabel(const std::string_view quest_id, const std::string_view objective_id) {
    std::string label{};
    label.reserve(quest_id.size() + objective_id.size() + 20);
    label.append("quest '");
    label.append(quest_id);
    label.append("' objective '");
    label.append(objective_id);
    label.push_back('\'');
    return label;
}

[[nodiscard]] bool parseObjectiveMarkerPosition(const Json& node,
                                                const std::string_view objective_label,
                                                glm::vec2& out_position) {
    if (!node.is_array() || node.size() != 2U || !node[0].is_number() || !node[1].is_number()) {
        spdlog::error("QuestCatalog: {} 的 marker.position 必须是长度为 2 的数值数组", objective_label);
        return false;
    }

    out_position = glm::vec2{node[0].get<float>(), node[1].get<float>()};
    return true;
}

[[nodiscard]] bool parseObjectiveMarker(const Json& node,
                                        const std::string_view objective_label,
                                        QuestObjectiveMarkerData& out_marker) {
    if (!node.is_object()) {
        spdlog::error("QuestCatalog: {} 的 marker 必须是 object", objective_label);
        return false;
    }

    QuestObjectiveMarkerData marker{};
    if (!parseRequiredString(node, "map", objective_label, marker.map_name_)) {
        return false;
    }
    marker.map_id_hash_ = entt::hashed_string{marker.map_name_.c_str()}.value();

    const auto position_it = node.find("position");
    if (position_it == node.end() || !parseObjectiveMarkerPosition(*position_it, objective_label, marker.position_)) {
        return false;
    }

    if (!parseOptionalString(node, "label", objective_label, marker.label_)) {
        return false;
    }

    out_marker = std::move(marker);
    return true;
}

[[nodiscard]] bool parseRewardItems(const Json& node,
                                    const std::string_view quest_id,
                                    std::vector<QuestRewardItemData>& out_items) {
    out_items.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        spdlog::error("QuestCatalog: quest '{}' 的 rewards.items 必须是 array", quest_id);
        return false;
    }

    for (const auto& item_node : node) {
        if (!item_node.is_object()) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.items 存在非 object 条目", quest_id);
            return false;
        }

        QuestRewardItemData reward_item{};
        if (!parseRequiredString(item_node, "item_id", "reward item", reward_item.item_id_)) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.items.item_id 非法", quest_id);
            return false;
        }

        const auto count_it = item_node.find("count");
        if (count_it == item_node.end() || !count_it->is_number_integer()) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.items.count 必须是整数", quest_id);
            return false;
        }

        reward_item.count_ = count_it->get<int>();
        if (reward_item.count_ <= 0) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.items.count 必须 > 0", quest_id);
            return false;
        }
        reward_item.item_id_hash_ = QuestCatalog::hashId(reward_item.item_id_);
        out_items.push_back(std::move(reward_item));
    }

    return true;
}

[[nodiscard]] bool parseRewards(const Json& node, const std::string_view quest_id, QuestRewardData& out_rewards) {
    out_rewards = QuestRewardData{};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        spdlog::error("QuestCatalog: quest '{}' 的 rewards 必须是 object", quest_id);
        return false;
    }

    if (const auto gold_it = node.find("gold"); gold_it != node.end()) {
        if (!gold_it->is_number_integer()) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.gold 必须是整数", quest_id);
            return false;
        }
        out_rewards.gold_ = gold_it->get<int>();
        if (out_rewards.gold_ < 0) {
            spdlog::error("QuestCatalog: quest '{}' 的 rewards.gold 必须 >= 0", quest_id);
            return false;
        }
    }

    if (const auto items_it = node.find("items"); items_it != node.end()) {
        if (!parseRewardItems(*items_it, quest_id, out_rewards.items_)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool parseGiverText(const Json& node, const std::string_view quest_id, QuestGiverTextData& out_text) {
    out_text = QuestGiverTextData{};
    if (node.is_null()) {
        return true;
    }
    if (!node.is_object()) {
        spdlog::error("QuestCatalog: quest '{}' 的 giver_text 必须是 object", quest_id);
        return false;
    }

    if (!parseOptionalString(node, "offer", quest_id, out_text.offer_)) {
        return false;
    }
    if (!parseOptionalString(node, "progress", quest_id, out_text.progress_)) {
        return false;
    }
    if (!parseOptionalString(node, "ready_to_turn_in", quest_id, out_text.ready_to_turn_in_)) {
        return false;
    }
    if (!parseOptionalString(node, "completed", quest_id, out_text.completed_)) {
        return false;
    }

    return true;
}

[[nodiscard]] bool parseObjectives(const Json& node,
                                   const std::string_view quest_id,
                                   std::vector<QuestObjectiveData>& out_objectives) {
    out_objectives.clear();
    if (!node.is_array() || node.empty()) {
        spdlog::error("QuestCatalog: quest '{}' 的 objectives 必须是非空数组", quest_id);
        return false;
    }

    std::unordered_set<std::string> objective_ids{};
    objective_ids.reserve(node.size());

    for (const auto& objective_node : node) {
        if (!objective_node.is_object()) {
            spdlog::error("QuestCatalog: quest '{}' 的 objectives 存在非 object 条目", quest_id);
            return false;
        }

        QuestObjectiveData objective{};
        if (!parseRequiredString(objective_node, "id", quest_id, objective.id_)) {
            return false;
        }
        if (!validateIdentifier(objective.id_, "objective id", quest_id)) {
            return false;
        }
        if (!objective_ids.insert(objective.id_).second) {
            spdlog::error("QuestCatalog: quest '{}' 存在重复 objective id '{}'", quest_id, objective.id_);
            return false;
        }

        const std::string objective_label = makeQuestObjectiveLabel(quest_id, objective.id_);
        std::string kind_str{};
        if (!parseRequiredString(objective_node, "kind", objective_label, kind_str)) {
            return false;
        }
        if (!parseQuestObjectiveKind(kind_str, objective.kind_)) {
            spdlog::error("QuestCatalog: quest '{}' 的 objective '{}' kind 非法: {}",
                          quest_id,
                          objective.id_,
                          kind_str);
            return false;
        }

        if (!parseRequiredString(objective_node, "enemy_id", objective_label, objective.enemy_id_)) {
            return false;
        }
        objective.enemy_id_hash_ = QuestCatalog::hashId(objective.enemy_id_);

        const auto required_count_it = objective_node.find("required_count");
        if (required_count_it == objective_node.end() || !required_count_it->is_number_integer()) {
            spdlog::error("QuestCatalog: quest '{}' 的 objective '{}' required_count 必须是整数",
                          quest_id,
                          objective.id_);
            return false;
        }
        objective.required_count_ = required_count_it->get<int>();
        if (objective.required_count_ <= 0) {
            spdlog::error("QuestCatalog: quest '{}' 的 objective '{}' required_count 必须 > 0",
                          quest_id,
                          objective.id_);
            return false;
        }

        if (const auto marker_it = objective_node.find("marker"); marker_it != objective_node.end()) {
            QuestObjectiveMarkerData marker{};
            if (!parseObjectiveMarker(*marker_it, objective_label, marker)) {
                return false;
            }
            objective.marker_ = std::move(marker);
        }

        out_objectives.push_back(std::move(objective));
    }

    return true;
}

[[nodiscard]] bool parseQuestNode(const Json& quest_node, QuestData& out_quest) {
    if (!quest_node.is_object()) {
        spdlog::error("QuestCatalog: quests 数组存在非 object 条目");
        return false;
    }

    QuestData parsed{};
    if (!parseRequiredString(quest_node, "id", "quest", parsed.id_)) {
        return false;
    }
    if (!validateIdentifier(parsed.id_, "quest id", parsed.id_)) {
        return false;
    }

    parsed.id_hash_ = QuestCatalog::hashId(parsed.id_);
    if (!parseRequiredString(quest_node, "title", parsed.id_, parsed.title_)) {
        return false;
    }
    if (!parseOptionalString(quest_node, "description", parsed.id_, parsed.description_)) {
        return false;
    }

    const auto objectives_it = quest_node.find("objectives");
    if (objectives_it == quest_node.end() || !parseObjectives(*objectives_it, parsed.id_, parsed.objectives_)) {
        return false;
    }

    if (const auto rewards_it = quest_node.find("rewards"); rewards_it != quest_node.end()) {
        if (!parseRewards(*rewards_it, parsed.id_, parsed.rewards_)) {
            return false;
        }
    }

    if (const auto giver_text_it = quest_node.find("giver_text"); giver_text_it != quest_node.end()) {
        if (!parseGiverText(*giver_text_it, parsed.id_, parsed.giver_text_)) {
            return false;
        }
    }

    out_quest = std::move(parsed);
    return true;
}

} // namespace

entt::id_type QuestCatalog::hashId(const std::string_view id) {
    return entt::hashed_string{id.data(), id.size()}.value();
}

bool QuestCatalog::loadFromFile(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "QuestCatalog", spdlog::level::err)) {
        return false;
    }

    const auto schema_version = root.value("schema_version", 0U);
    if (schema_version == 0U) {
        spdlog::error("QuestCatalog: '{}' 缺少有效 schema_version", file_path);
        return false;
    }

    const auto quests_it = root.find("quests");
    if (quests_it == root.end() || !quests_it->is_array()) {
        spdlog::error("QuestCatalog: '{}' 缺少 quests 数组", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, QuestData> parsed_quests{};
    parsed_quests.reserve(quests_it->size());

    for (const auto& quest_node : *quests_it) {
        QuestData quest{};
        if (!parseQuestNode(quest_node, quest)) {
            return false;
        }
        if (parsed_quests.contains(quest.id_hash_)) {
            spdlog::error("QuestCatalog: '{}' 存在重复 quest id '{}'", file_path, quest.id_);
            return false;
        }
        parsed_quests.emplace(quest.id_hash_, std::move(quest));
    }

    schema_version_ = schema_version;
    quests_ = std::move(parsed_quests);
    return true;
}

bool QuestCatalog::validateReferences(const RpgCatalog* rpg_catalog,
                                      const ItemCatalog* item_catalog,
                                      std::string& out_error) const {
    out_error.clear();

    for (const auto& [_, quest] : quests_) {
        for (const auto& objective : quest.objectives_) {
            if (objective.kind_ == QuestObjectiveKind::DefeatEnemyCount) {
                if (!rpg_catalog) {
                    out_error = "QuestCatalog validateReferences requires RpgCatalog for defeat_enemy_count objectives";
                    return false;
                }
                if (!rpg_catalog->findEnemy(objective.enemy_id_hash_)) {
                    out_error = "Quest '" + quest.id_ + "' objective '" + objective.id_ +
                                "' references unknown enemy '" + objective.enemy_id_ + "'";
                    return false;
                }
            }
        }

        for (const auto& item : quest.rewards_.items_) {
            if (!item_catalog) {
                out_error = "QuestCatalog validateReferences requires ItemCatalog for item rewards";
                return false;
            }
            if (!item_catalog->hasItem(item.item_id_hash_)) {
                out_error = "Quest '" + quest.id_ + "' reward references unknown item '" + item.item_id_ + "'";
                return false;
            }
        }
    }

    return true;
}

const QuestData* QuestCatalog::findQuest(const entt::id_type id_hash) const {
    const auto it = quests_.find(id_hash);
    return it != quests_.end() ? &it->second : nullptr;
}

const QuestData* QuestCatalog::findQuest(const std::string_view id) const {
    return findQuest(hashId(id));
}

std::vector<const QuestData*> QuestCatalog::listQuests() const {
    std::vector<const QuestData*> quests{};
    quests.reserve(quests_.size());
    for (const auto& [_, quest] : quests_) {
        quests.push_back(&quest);
    }
    std::sort(quests.begin(), quests.end(), [](const QuestData* lhs, const QuestData* rhs) {
        return lhs->id_ < rhs->id_;
    });
    return quests;
}

} // namespace game::data
