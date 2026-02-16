#pragma once

#include "engine/render/image.h"
#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include <entt/entity/entity.hpp>
#include <entt/core/hashed_string.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::data {

/// @brief 物品类别
enum class ItemCategory {
    Tool,
    Crop,
    Seed,
    Material,
    Consumable,
    Unknown
};

enum class ItemUseEffectType {
    AddItem,
    Unknown
};

struct ItemUseEffect {
    ItemUseEffectType type{ItemUseEffectType::Unknown};
    entt::id_type item_id{entt::null};
    int count{0};
};

struct ItemUseConfig {
    int consume{1};
    std::vector<ItemUseEffect> effects{};
};

struct ItemData {
    entt::id_type id_{entt::null};
    std::string display_name_{};
    std::string category_str_{};   ///< @brief 配置文件中的原始 category 字符串（用于UI展示）
    std::string description_{};    ///< @brief 配置文件中的 description（用于tooltip）
    ItemCategory category_{ItemCategory::Unknown};
    entt::id_type icon_id_{entt::null};
    int stack_limit_{999};
    game::defs::Tool tool_type_{game::defs::Tool::None};              ///< 仅 Tool 类别使用
    game::defs::CropType crop_type_{game::defs::CropType::Unknown};   ///< 仅 Crop 类别使用
    std::optional<ItemUseConfig> on_use_{};
};

class ItemCatalog final {
    
    std::unordered_map<entt::id_type, engine::render::Image> icons_{};
    std::unordered_map<entt::id_type, ItemData> items_{};
    engine::render::Image fallback_icon_{};

public:
    ItemCatalog() = default;
    ~ItemCatalog() = default;

    bool loadIconConfig(std::string_view file_path);
    bool loadItemConfig(std::string_view file_path);

    [[nodiscard]] const ItemData* findItem(entt::id_type item_id) const;
    [[nodiscard]] const engine::render::Image* findIcon(entt::id_type icon_id) const;
    [[nodiscard]] const engine::render::Image& getIconOrFallback(entt::id_type icon_id) const;
    /// @brief 根据 item_id 直接获取其图标（内部查 ItemData → icon_id → Image），找不到则返回 fallback
    [[nodiscard]] engine::render::Image getItemIcon(entt::id_type item_id) const;
    [[nodiscard]] std::vector<const ItemData*> listItems() const;

    [[nodiscard]] bool hasItem(entt::id_type item_id) const { return items_.find(item_id) != items_.end(); }

};

} // namespace game::data
