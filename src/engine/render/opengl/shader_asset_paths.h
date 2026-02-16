#pragma once

#include <string_view>

namespace engine::render::opengl::shader_assets {

inline constexpr std::string_view SPRITE_VERTEX_PATH = "assets/shaders/quad.vert";
inline constexpr std::string_view SPRITE_FRAGMENT_PATH = "assets/shaders/texture.frag";
inline constexpr std::string_view LIGHT_VERTEX_PATH = "assets/shaders/light.vert";
inline constexpr std::string_view LIGHT_FRAGMENT_PATH = "assets/shaders/light.frag";
inline constexpr std::string_view EMISSIVE_VERTEX_PATH = "assets/shaders/emissive.vert";
inline constexpr std::string_view EMISSIVE_FRAGMENT_PATH = "assets/shaders/emissive.frag";
inline constexpr std::string_view BLUR_VERTEX_PATH = "assets/shaders/quad_uv.vert";
inline constexpr std::string_view BLUR_FRAGMENT_PATH = "assets/shaders/blur.frag";
inline constexpr std::string_view COMPOSITE_VERTEX_PATH = "assets/shaders/composite.vert";
inline constexpr std::string_view COMPOSITE_FRAGMENT_PATH = "assets/shaders/composite.frag";

} // namespace engine::render::opengl::shader_assets
