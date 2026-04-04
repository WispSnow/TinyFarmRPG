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

/// @brief 为物品生成可直接绑定到 RmlUi `decorator` 样式上的值。
/// @details
/// 输入是 gameplay 层的 `item_id`，输出是形如 `image(item-tools-axe)` 的样式字符串，
/// 供 RML 中 `data-style-decorator="slot.icon_decorator"` 直接使用。
///
/// 生成流程为：
/// 1. 通过 `item_id` 在 `ItemCatalog` 中找到物品数据；
/// 2. 从物品数据取出 `icon_id_` 并解析为 icon key，例如 `tools/axe`；
/// 3. 将 icon key 规范化为 spritesheet 名称，例如 `item-tools-axe`；
/// 4. 包装成 RmlUi decorator 值 `image(item-tools-axe)`。
///
/// 上述 spritesheet 名称需要在 `ui/rmlui/theme/spritesheet.rcss` 中有对应定义。
/// 若任一中间数据缺失，则返回 `none`，表示该槽位不绘制图标 decorator。
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

    // RmlUi 这里要的不是图片路径，而是可绑定到 `decorator` 属性的样式值。
    std::string decorator{"image("};
    decorator += spriteNameFromIconKey(*icon_key);
    decorator += ')';
    return decorator;
}

} // namespace game::ui
