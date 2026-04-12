#pragma once

#include "game/data/shop_data.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>

namespace game::data {

class ItemCatalog;

class ShopCatalog final {
public:
    [[nodiscard]] static entt::id_type hashId(std::string_view id);

    [[nodiscard]] bool loadFromFile(std::string_view file_path);
    [[nodiscard]] bool validateReferences(const ItemCatalog* item_catalog, std::string& out_error) const;

    [[nodiscard]] std::uint32_t schemaVersion() const { return schema_version_; }
    [[nodiscard]] const ShopData* findShop(entt::id_type id_hash) const;
    [[nodiscard]] const ShopData* findShop(std::string_view id) const;
    [[nodiscard]] const ShopBuyEntryData* findBuyEntry(entt::id_type shop_id_hash, entt::id_type item_id_hash) const;
    [[nodiscard]] const ShopBuyEntryData* findBuyEntry(std::string_view shop_id, entt::id_type item_id_hash) const;
    [[nodiscard]] const ShopSellRuleData* findSellRule(entt::id_type item_id_hash) const;
    [[nodiscard]] const ShopSellRuleData* findSellRule(std::string_view item_id) const;
    [[nodiscard]] std::vector<const ShopData*> listShops() const;

private:
    void logSellPriceWarnings() const;

    std::uint32_t schema_version_{0};
    std::unordered_map<entt::id_type, ShopData> shops_{};
    std::unordered_map<entt::id_type, ShopSellRuleData> sell_rules_{};
};

} // namespace game::data
