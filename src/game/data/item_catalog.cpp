#include "item_catalog.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <glm/vec2.hpp>
#include <algorithm>

namespace game::data {

namespace {

constexpr int DEFAULT_STACK_LIMIT = 999;
constexpr int TOOL_STACK_LIMIT = 1;

ItemCategory categoryFromString(std::string_view value) {
    if (value == "tool") return ItemCategory::Tool;
    if (value == "crop") return ItemCategory::Crop;
    if (value == "seed") return ItemCategory::Seed;
    if (value == "material") return ItemCategory::Material;
    if (value == "consumable") return ItemCategory::Consumable;
    return ItemCategory::Unknown;
}

game::defs::Tool toolFromString(std::string_view value) {
    if (value == "hoe") return game::defs::Tool::Hoe;
    if (value == "watering_can") return game::defs::Tool::WateringCan;
    if (value == "pickaxe") return game::defs::Tool::Pickaxe;
    if (value == "axe") return game::defs::Tool::Axe;
    if (value == "sickle") return game::defs::Tool::Sickle;
    return game::defs::Tool::None;
}

ItemUseEffectType useEffectTypeFromString(std::string_view value) {
    if (value == "add_item") return ItemUseEffectType::AddItem;
    return ItemUseEffectType::Unknown;
}

int defaultStackLimit(ItemCategory category) {
    return category == ItemCategory::Tool ? TOOL_STACK_LIMIT : DEFAULT_STACK_LIMIT;
}

entt::id_type makeId(std::string_view value) {
    return entt::hashed_string(value.data());
}

engine::render::Image parseImage(const nlohmann::json& json) {
    auto path = json.value("path", std::string{});
    const auto source = json.value("source", std::vector<float>{});
    if (path.empty() || source.size() < 4) {
        spdlog::warn("Icon config 缺少 path 或 source 信息: {}", json.dump());
        return {};
    }
    const float x = source[0];
    const float y = source[1];
    const float w = source[2];
    const float h = source[3];
    return engine::render::Image(path, engine::utils::Rect{glm::vec2{x, y}, glm::vec2{w, h}});
}

} // namespace

bool ItemCatalog::loadIconConfig(std::string_view file_path) {
    std::ifstream file(file_path.data());
    if (!file.is_open()) {
        spdlog::error("无法打开 icon 配置文件: {}", file_path);
        return false;
    }

    nlohmann::json json;
    file >> json;

    icons_.clear();

    for (const auto& [category, icon_group] : json.items()) {
        if (!icon_group.is_object()) continue;
        for (const auto& [icon_name, icon_obj] : icon_group.items()) {
            auto image = parseImage(icon_obj);
            if (image.getTexturePath().empty()) {
                continue;
            }
            const std::string full_key = category + "/" + icon_name;
            auto icon_id = makeId(full_key);
            icons_.insert_or_assign(icon_id, image);
            if (full_key == "indicator/cursor" || fallback_icon_.getTexturePath().empty()) {
                fallback_icon_ = image;  // 设置默认占位图标
            }
        }
    }

    if (fallback_icon_.getTexturePath().empty() && !icons_.empty()) {
        fallback_icon_ = icons_.begin()->second;
    }

    return !icons_.empty();
}

bool ItemCatalog::loadItemConfig(std::string_view file_path) {
    std::ifstream file(file_path.data());
    if (!file.is_open()) {
        spdlog::error("无法打开 item 配置文件: {}", file_path);
        return false;
    }

    nlohmann::json json;
    file >> json;

    if (!json.contains("items") || !json["items"].is_array()) {
        spdlog::error("item 配置缺少 items 数组: {}", file_path);
        return false;
    }

    items_.clear();

    for (const auto& item_obj : json["items"]) {
        const std::string id_str = item_obj.value("id", "");
        if (id_str.empty()) {
            spdlog::warn("跳过无效物品，缺少 id: {}", item_obj.dump());
            continue;
        }

        ItemData data;
        data.id_ = makeId(id_str);
        data.display_name_ = item_obj.value("display_name", id_str);

        const std::string category_str = item_obj.value("category", "");
        data.category_str_ = category_str;
        data.category_ = categoryFromString(category_str);

        data.description_ = item_obj.value("description", "");

        const std::string icon_str = item_obj.value("icon_id", "");
        data.icon_id_ = icon_str.empty() ? entt::null : makeId(icon_str);

        data.stack_limit_ = item_obj.value("stack_limit", defaultStackLimit(data.category_));
        if (data.category_ == ItemCategory::Tool) {
            data.stack_limit_ = TOOL_STACK_LIMIT;  // 工具一律不可堆叠
            data.tool_type_ = toolFromString(item_obj.value("tool", ""));
        }
        else if (data.category_ == ItemCategory::Crop || data.category_ == ItemCategory::Seed) {
            data.crop_type_ = game::defs::cropTypeFromString(item_obj.value("crop_type", ""));
        }

        if (auto on_use_it = item_obj.find("on_use"); on_use_it != item_obj.end() && on_use_it->is_object()) {
            ItemUseConfig use{};
            use.consume = std::max(1, on_use_it->value("consume", 1));

            if (auto effects_it = on_use_it->find("effects"); effects_it != on_use_it->end() && effects_it->is_array()) {
                for (const auto& effect_obj : *effects_it) {
                    if (!effect_obj.is_object()) continue;
                    const std::string type_str = effect_obj.value("type", "");
                    const auto type = useEffectTypeFromString(type_str);
                    if (type == ItemUseEffectType::Unknown) {
                        spdlog::warn("物品 '{}' 的 on_use.effects 存在未知类型 '{}'", id_str, type_str);
                        continue;
                    }

                    const std::string effect_item_str = effect_obj.value("id", "");
                    const int effect_count = effect_obj.value("count", 0);
                    if (effect_item_str.empty() || effect_count <= 0) {
                        spdlog::warn("物品 '{}' 的 on_use.effects 缺少 id/count: {}", id_str, effect_obj.dump());
                        continue;
                    }

                    ItemUseEffect effect{};
                    effect.type = type;
                    effect.item_id = makeId(effect_item_str);
                    effect.count = effect_count;
                    use.effects.push_back(effect);
                }
            }

            if (!use.effects.empty()) {
                data.on_use_ = std::move(use);
            }
        }

        items_.insert_or_assign(data.id_, std::move(data));
    }

    return !items_.empty();
}

const ItemData* ItemCatalog::findItem(entt::id_type item_id) const {
    if (auto it = items_.find(item_id); it != items_.end()) {
        return &it->second;
    }
    return nullptr;
}

const engine::render::Image* ItemCatalog::findIcon(entt::id_type icon_id) const {
    if (auto it = icons_.find(icon_id); it != icons_.end()) {
        return &it->second;
    }
    return nullptr;
}

const engine::render::Image& ItemCatalog::getIconOrFallback(entt::id_type icon_id) const {
    if (auto it = icons_.find(icon_id); it != icons_.end()) {
        return it->second;
    }
    return fallback_icon_;
}

engine::render::Image ItemCatalog::getItemIcon(entt::id_type item_id) const {
    const auto* data = findItem(item_id);
    const entt::id_type icon_id = data ? data->icon_id_ : entt::null;
    return getIconOrFallback(icon_id);
}

std::vector<const ItemData*> ItemCatalog::listItems() const {
    std::vector<const ItemData*> result;
    result.reserve(items_.size());
    for (const auto& [_, item] : items_) {
        result.push_back(&item);
    }
    return result;
}

} // namespace game::data
