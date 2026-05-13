#include "game/data/shop_catalog.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/item_catalog.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace game::data {
namespace {

using Json = nlohmann::json;
constexpr std::uint32_t kSupportedSchemaVersion = 1;

[[nodiscard]] bool parseRequiredString(const Json& object,
                                       const char* key,
                                       const std::string_view owner_label,
                                       std::string& out_value) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        spdlog::error("ShopCatalog: {} 缺少 string 字段 '{}'", owner_label, key);
        return false;
    }

    out_value = it->get<std::string>();
    if (out_value.empty()) {
        spdlog::error("ShopCatalog: {} 的 '{}' 不能为空", owner_label, key);
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
        spdlog::error("ShopCatalog: {} 的 '{}' 必须是 string", owner_label, key);
        return false;
    }
    out_value = it->get<std::string>();
    return true;
}

[[nodiscard]] std::string makeBuyEntryLabel(const std::string_view shop_id, const std::string_view item_id) {
    std::string label{};
    label.reserve(shop_id.size() + item_id.size() + 24);
    label.append("shop '");
    label.append(shop_id);
    label.append("' buy entry '");
    label.append(item_id);
    label.push_back('\'');
    return label;
}

[[nodiscard]] bool parseBuyEntries(const Json& node,
                                   const std::string_view shop_id,
                                   std::vector<ShopBuyEntryData>& out_entries) {
    out_entries.clear();
    if (!node.is_array() || node.empty()) {
        spdlog::error("ShopCatalog: shop '{}' 的 buy_entries 必须是非空数组", shop_id);
        return false;
    }

    std::unordered_set<std::string> item_ids{};
    item_ids.reserve(node.size());

    for (const auto& entry_node : node) {
        if (!entry_node.is_object()) {
            spdlog::error("ShopCatalog: shop '{}' 的 buy_entries 存在非 object 条目", shop_id);
            return false;
        }

        ShopBuyEntryData entry{};
        if (!parseRequiredString(entry_node, "item_id", shop_id, entry.item_id_)) {
            return false;
        }
        if (!item_ids.insert(entry.item_id_).second) {
            spdlog::error("ShopCatalog: shop '{}' 存在重复 buy_entries.item_id '{}'", shop_id, entry.item_id_);
            return false;
        }

        const auto price_it = entry_node.find("buy_price");
        if (price_it == entry_node.end() || !price_it->is_number_integer()) {
            spdlog::error("ShopCatalog: {} 的 buy_price 必须是整数",
                          makeBuyEntryLabel(shop_id, entry.item_id_));
            return false;
        }
        entry.buy_price_ = price_it->get<int>();
        if (entry.buy_price_ <= 0) {
            spdlog::error("ShopCatalog: {} 的 buy_price 必须 > 0",
                          makeBuyEntryLabel(shop_id, entry.item_id_));
            return false;
        }

        if (const auto stock_it = entry_node.find("stock"); stock_it != entry_node.end()) {
            if (!stock_it->is_number_integer()) {
                spdlog::error("ShopCatalog: {} 的 stock 必须是整数",
                              makeBuyEntryLabel(shop_id, entry.item_id_));
                return false;
            }
            const int stock = stock_it->get<int>();
            if (stock <= 0) {
                spdlog::error("ShopCatalog: {} 的 stock 必须 > 0",
                              makeBuyEntryLabel(shop_id, entry.item_id_));
                return false;
            }
            entry.stock_ = stock;
        }

        entry.item_id_hash_ = ShopCatalog::hashId(entry.item_id_);
        out_entries.push_back(std::move(entry));
    }

    return true;
}

[[nodiscard]] bool parseShopNode(const Json& shop_node, ShopData& out_shop) {
    if (!shop_node.is_object()) {
        spdlog::error("ShopCatalog: shops 数组存在非 object 条目");
        return false;
    }

    ShopData parsed{};
    if (!parseRequiredString(shop_node, "id", "shop", parsed.id_)) {
        return false;
    }
    parsed.id_hash_ = ShopCatalog::hashId(parsed.id_);

    if (!parseRequiredString(shop_node, "title", parsed.id_, parsed.title_)) {
        return false;
    }
    if (!parseOptionalString(shop_node, "greeting", parsed.id_, parsed.greeting_)) {
        return false;
    }

    const auto buy_entries_it = shop_node.find("buy_entries");
    if (buy_entries_it == shop_node.end() || !parseBuyEntries(*buy_entries_it, parsed.id_, parsed.buy_entries_)) {
        return false;
    }

    out_shop = std::move(parsed);
    return true;
}

[[nodiscard]] bool parseSellRules(const Json& node, std::unordered_map<entt::id_type, ShopSellRuleData>& out_rules) {
    out_rules.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        spdlog::error("ShopCatalog: sell_rules 必须是数组");
        return false;
    }

    for (const auto& rule_node : node) {
        if (!rule_node.is_object()) {
            spdlog::error("ShopCatalog: sell_rules 存在非 object 条目");
            return false;
        }

        ShopSellRuleData rule{};
        if (!parseRequiredString(rule_node, "item_id", "sell rule", rule.item_id_)) {
            return false;
        }
        rule.item_id_hash_ = ShopCatalog::hashId(rule.item_id_);

        const auto price_it = rule_node.find("sell_price");
        if (price_it == rule_node.end() || !price_it->is_number_integer()) {
            spdlog::error("ShopCatalog: sell rule '{}' 的 sell_price 必须是整数", rule.item_id_);
            return false;
        }

        rule.sell_price_ = price_it->get<int>();
        if (rule.sell_price_ <= 0) {
            spdlog::error("ShopCatalog: sell rule '{}' 的 sell_price 必须 > 0", rule.item_id_);
            return false;
        }

        if (out_rules.contains(rule.item_id_hash_)) {
            spdlog::error("ShopCatalog: sell_rules 存在重复 item_id '{}'", rule.item_id_);
            return false;
        }
        out_rules.insert_or_assign(rule.item_id_hash_, std::move(rule));
    }

    return true;
}

} // namespace

entt::id_type ShopCatalog::hashId(const std::string_view id) {
    return entt::hashed_string{id.data(), id.size()}.value();
}

bool ShopCatalog::loadFromFile(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "ShopCatalog", spdlog::level::err)) {
        return false;
    }

    const auto schema_version = root.value("schema_version", 0U);
    if (schema_version != kSupportedSchemaVersion) {
        spdlog::error("ShopCatalog: '{}' 的 schema_version 必须为 {}", file_path, kSupportedSchemaVersion);
        return false;
    }

    const auto shops_it = root.find("shops");
    if (shops_it == root.end() || !shops_it->is_array() || shops_it->empty()) {
        spdlog::error("ShopCatalog: '{}' 缺少非空 shops 数组", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, ShopData> parsed_shops{};
    parsed_shops.reserve(shops_it->size());
    for (const auto& shop_node : *shops_it) {
        ShopData shop{};
        if (!parseShopNode(shop_node, shop)) {
            return false;
        }
        if (parsed_shops.contains(shop.id_hash_)) {
            spdlog::error("ShopCatalog: 存在重复 shop id '{}'", shop.id_);
            return false;
        }
        parsed_shops.insert_or_assign(shop.id_hash_, std::move(shop));
    }

    std::unordered_map<entt::id_type, ShopSellRuleData> parsed_sell_rules{};
    if (const auto sell_rules_it = root.find("sell_rules"); sell_rules_it != root.end()) {
        if (!parseSellRules(*sell_rules_it, parsed_sell_rules)) {
            return false;
        }
    }

    schema_version_ = schema_version;
    shops_ = std::move(parsed_shops);
    sell_rules_ = std::move(parsed_sell_rules);
    logSellPriceWarnings();
    return true;
}

bool ShopCatalog::validateReferences(const ItemCatalog* item_catalog, std::string& out_error) const {
    out_error.clear();
    if (item_catalog == nullptr) {
        out_error = "ShopCatalog requires ItemCatalog";
        return false;
    }

    for (const auto& [_, shop] : shops_) {
        for (const auto& entry : shop.buy_entries_) {
            if (!item_catalog->hasItem(entry.item_id_hash_)) {
                out_error = "shop '" + shop.id_ + "' references missing item '" + entry.item_id_ + "'";
                return false;
            }
        }
    }

    for (const auto& [_, rule] : sell_rules_) {
        if (!item_catalog->hasItem(rule.item_id_hash_)) {
            out_error = "sell rule references missing item '" + rule.item_id_ + "'";
            return false;
        }
    }

    return true;
}

const ShopData* ShopCatalog::findShop(const entt::id_type id_hash) const {
    if (const auto it = shops_.find(id_hash); it != shops_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ShopData* ShopCatalog::findShop(const std::string_view id) const {
    return findShop(hashId(id));
}

const ShopBuyEntryData* ShopCatalog::findBuyEntry(const entt::id_type shop_id_hash, const entt::id_type item_id_hash) const {
    const auto* shop = findShop(shop_id_hash);
    if (shop == nullptr) {
        return nullptr;
    }

    const auto it = std::find_if(
        shop->buy_entries_.begin(),
        shop->buy_entries_.end(),
        [item_id_hash](const ShopBuyEntryData& entry) { return entry.item_id_hash_ == item_id_hash; });
    return it != shop->buy_entries_.end() ? &(*it) : nullptr;
}

const ShopBuyEntryData* ShopCatalog::findBuyEntry(const std::string_view shop_id, const entt::id_type item_id_hash) const {
    return findBuyEntry(hashId(shop_id), item_id_hash);
}

const ShopSellRuleData* ShopCatalog::findSellRule(const entt::id_type item_id_hash) const {
    if (const auto it = sell_rules_.find(item_id_hash); it != sell_rules_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ShopSellRuleData* ShopCatalog::findSellRule(const std::string_view item_id) const {
    return findSellRule(hashId(item_id));
}

std::vector<const ShopData*> ShopCatalog::listShops() const {
    std::vector<const ShopData*> result{};
    result.reserve(shops_.size());
    for (const auto& [_, shop] : shops_) {
        result.push_back(&shop);
    }
    return result;
}

void ShopCatalog::logSellPriceWarnings() const {
    std::unordered_map<entt::id_type, int> min_buy_prices{};
    for (const auto& [_, shop] : shops_) {
        for (const auto& entry : shop.buy_entries_) {
            const auto [it, inserted] = min_buy_prices.emplace(entry.item_id_hash_, entry.buy_price_);
            if (!inserted) {
                it->second = std::min(it->second, entry.buy_price_);
            }
        }
    }

    for (const auto& [item_id_hash, rule] : sell_rules_) {
        const auto it = min_buy_prices.find(item_id_hash);
        if (it == min_buy_prices.end()) {
            continue;
        }
        if (rule.sell_price_ > it->second) {
            spdlog::warn("ShopCatalog: item '{}' 的 sell_price={} 高于最低 buy_price={}",
                         rule.item_id_,
                         rule.sell_price_,
                         it->second);
        }
    }
}

} // namespace game::data
