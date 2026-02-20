#pragma once

#include <string_view>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>

namespace engine::ui {

inline constexpr std::string_view DEFAULT_UI_FONT_PATH{"assets/fonts/VonwaonBitmap-16px.ttf"};
inline constexpr int DEFAULT_UI_FONT_SIZE_PX{16};

[[nodiscard]] inline entt::id_type resolveUIFontId(entt::id_type font_id) {
    if (font_id != entt::null && font_id != entt::id_type{}) {
        return font_id;
    }
    return entt::hashed_string{DEFAULT_UI_FONT_PATH.data(), DEFAULT_UI_FONT_PATH.size()}.value();
}

} // namespace engine::ui
