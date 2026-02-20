#pragma once

#include "engine/resource/default_resource_ids.h"
#include <entt/entity/entity.hpp>

namespace engine::ui {

inline constexpr std::string_view DEFAULT_UI_FONT_PATH{engine::resource::defaults::UI_DEFAULT_FONT_PATH};
inline constexpr int DEFAULT_UI_FONT_SIZE_PX{engine::resource::defaults::UI_DEFAULT_FONT_SIZE_PX};
inline constexpr entt::id_type DEFAULT_UI_FONT_ID{engine::resource::defaults::UI_DEFAULT_FONT_ID};

[[nodiscard]] inline entt::id_type resolveUIFontId(entt::id_type font_id) {
    if (font_id != entt::null && font_id != entt::id_type{}) {
        return font_id;
    }
    return DEFAULT_UI_FONT_ID;
}

} // namespace engine::ui
