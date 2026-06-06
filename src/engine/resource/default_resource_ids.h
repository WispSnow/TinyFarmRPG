#pragma once

#include <string_view>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/fwd.hpp>

namespace engine::resource::defaults {

inline constexpr std::string_view CIRCLE_TEXTURE_KEY{"engine/texture/circle"};
inline constexpr std::string_view CIRCLE_TEXTURE_PATH{"assets/textures/UI/circle.png"};
inline constexpr entt::id_type CIRCLE_TEXTURE_ID{
    entt::hashed_string{CIRCLE_TEXTURE_KEY.data(), CIRCLE_TEXTURE_KEY.size()}.value()
};

inline constexpr std::string_view UI_DEFAULT_FONT_KEY{"engine/font/ui_default"};
#if defined(__EMSCRIPTEN__)
inline constexpr std::string_view UI_DEFAULT_FONT_PATH{"assets/fonts/VonwaonBitmap-16px.ttf"};
#else
inline constexpr std::string_view UI_DEFAULT_FONT_PATH{"assets/fonts/LXGWBright-Regular.ttf"};
#endif
inline constexpr int UI_DEFAULT_FONT_SIZE_PX{16};
inline constexpr entt::id_type UI_DEFAULT_FONT_ID{
    entt::hashed_string{UI_DEFAULT_FONT_KEY.data(), UI_DEFAULT_FONT_KEY.size()}.value()
};

} // namespace engine::resource::defaults
