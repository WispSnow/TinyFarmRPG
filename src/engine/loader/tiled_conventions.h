#pragma once

#include <string_view>

namespace engine::loader::tiled {

// ----------------------------
// Layer / object semantics
// ----------------------------
inline constexpr std::string_view LAYER_NAME_COLLIDER = "collider";
inline constexpr std::string_view LAYER_NAME_SOLID = "solid";

inline constexpr std::string_view OBJECT_TYPE_COLLIDER = "collider";

// ----------------------------
// Property names
// ----------------------------
inline constexpr std::string_view LAYER_PROPERTY_ORDER = "order";

inline constexpr std::string_view TILE_PROPERTY_TILE_FLAG = "tile_flag";
inline constexpr std::string_view TILE_PROPERTY_OBJ_TYPE = "obj_type";
inline constexpr std::string_view TILE_PROPERTY_ANIM_ID = "anim_id";

inline constexpr std::string_view MAP_PROPERTY_AMBIENT = "ambient";

// ----------------------------
// Tile flag tokens (tile_flag)
// ----------------------------
inline constexpr std::string_view TILE_FLAG_BLOCK_NORTH = "BLOCK_N";
inline constexpr std::string_view TILE_FLAG_BLOCK_SOUTH = "BLOCK_S";
inline constexpr std::string_view TILE_FLAG_BLOCK_WEST = "BLOCK_W";
inline constexpr std::string_view TILE_FLAG_BLOCK_EAST = "BLOCK_E";
inline constexpr std::string_view TILE_FLAG_HAZARD = "HAZARD";
inline constexpr std::string_view TILE_FLAG_WATER = "WATER";
inline constexpr std::string_view TILE_FLAG_INTERACT = "INTERACT";
inline constexpr std::string_view TILE_FLAG_ARABLE = "ARABLE";
inline constexpr std::string_view TILE_FLAG_SOLID = "SOLID";
inline constexpr std::string_view TILE_FLAG_OCCUPIED = "OCCUPIED";

} // namespace engine::loader::tiled
