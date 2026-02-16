#pragma once

#include <entt/core/hashed_string.hpp>

namespace game::defs {

namespace spatial_layer {

using namespace entt::literals;

inline constexpr entt::id_type SOIL = "soil"_hs;
inline constexpr entt::id_type WET = "wet"_hs;
inline constexpr entt::id_type CROP = "crop"_hs;
inline constexpr entt::id_type ROCK = "rock"_hs;
inline constexpr entt::id_type REST = "rest"_hs;
inline constexpr entt::id_type MAIN = "main"_hs;

} // namespace spatial_layer

/// 自动图块规则 ID（soil_tilled / soil_wet）
namespace auto_tile_rule {

using namespace entt::literals;

inline constexpr entt::id_type SOIL_TILLED = "soil_tilled"_hs;
inline constexpr entt::id_type SOIL_WET    = "soil_wet"_hs;

} // namespace auto_tile_rule

} // namespace game::defs
