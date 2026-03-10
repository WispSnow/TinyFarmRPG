#pragma once

#include "game/data/item_catalog.h"

#include <cctype>
#include <string>
#include <string_view>

namespace game::ui {

constexpr std::string_view kNoDecorator = "none";

[[nodiscard]] inline std::string spriteNameFromIconKey(std::string_view icon_key) {
    std::string sprite_name{"item-"};
    sprite_name.reserve(icon_key.size() + 5);
    for (const char ch : icon_key) {
        if (ch == '/' || ch == '_') {
            sprite_name.push_back('-');
        } else {
            sprite_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return sprite_name;
}

[[nodiscard]] inline bool hasDecorator(std::string_view decorator) {
    return !decorator.empty() && decorator != kNoDecorator;
}

[[nodiscard]] inline std::string buildItemIconDecorator(const game::data::ItemCatalog* item_catalog,
                                                        entt::id_type item_id) {
    if (!item_catalog) {
        return std::string{kNoDecorator};
    }

    const auto* item = item_catalog->findItem(item_id);
    if (!item || item->icon_id_ == entt::null) {
        return std::string{kNoDecorator};
    }

    const auto* icon_key = item_catalog->findIconKey(item->icon_id_);
    if (!icon_key || icon_key->empty()) {
        return std::string{kNoDecorator};
    }

    std::string decorator{"image("};
    decorator += spriteNameFromIconKey(*icon_key);
    decorator += ')';
    return decorator;
}

} // namespace game::ui
