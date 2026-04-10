#pragma once

#include "game/data/quest_data.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>

namespace game::data {

class ItemCatalog;
class RpgCatalog;

class QuestCatalog final {
public:
    [[nodiscard]] static entt::id_type hashId(std::string_view id);

    [[nodiscard]] bool loadFromFile(std::string_view file_path);
    [[nodiscard]] bool validateReferences(const RpgCatalog* rpg_catalog,
                                          const ItemCatalog* item_catalog,
                                          std::string& out_error) const;

    [[nodiscard]] std::uint32_t schemaVersion() const { return schema_version_; }
    [[nodiscard]] const QuestData* findQuest(entt::id_type id_hash) const;
    [[nodiscard]] const QuestData* findQuest(std::string_view id) const;
    [[nodiscard]] std::vector<const QuestData*> listQuests() const;

private:
    std::uint32_t schema_version_{0};
    std::unordered_map<entt::id_type, QuestData> quests_{};
};

} // namespace game::data
