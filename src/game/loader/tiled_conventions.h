#pragma once

#include "engine/loader/tiled_conventions.h"

#include <string_view>

namespace game::loader::tiled {

// Re-export engine-level property names used by game loader.
using engine::loader::tiled::TILE_PROPERTY_ANIM_ID;
using engine::loader::tiled::TILE_PROPERTY_OBJ_TYPE;

// ----------------------------
// Object layer: object "type"
// ----------------------------
inline constexpr std::string_view OBJECT_TYPE_ACTOR = "actor";
inline constexpr std::string_view OBJECT_TYPE_ANIMAL = "animal";
inline constexpr std::string_view OBJECT_TYPE_MAP_TRIGGER = "map_trigger";
inline constexpr std::string_view OBJECT_TYPE_REST = "rest";
inline constexpr std::string_view OBJECT_TYPE_LIGHT = "light";

// Object layer: light "name"
inline constexpr std::string_view LIGHT_NAME_POINT = "point";
inline constexpr std::string_view LIGHT_NAME_SPOT = "spot";
inline constexpr std::string_view LIGHT_NAME_EMISSIVE = "emissive";

// ----------------------------
// Tile properties: obj_type values
// ----------------------------
inline constexpr std::string_view TILE_OBJ_TYPE_CHEST = "chest";
inline constexpr std::string_view TILE_OBJ_TYPE_CROP = "crop";
inline constexpr std::string_view TILE_OBJ_TYPE_TREE = "tree";
inline constexpr std::string_view TILE_OBJ_TYPE_ROCK = "rock";
inline constexpr std::string_view TILE_OBJ_TYPE_WATER = "water";

// Some maps encode tool hint via anim_id
inline constexpr std::string_view ANIM_ID_AXE = "axe";
inline constexpr std::string_view ANIM_ID_PICKAXE = "pickaxe";

// ----------------------------
// Map trigger properties
// ----------------------------
inline constexpr std::string_view TRIGGER_PROP_SELF_ID = "self_id";
inline constexpr std::string_view TRIGGER_PROP_TARGET_ID = "target_id";
inline constexpr std::string_view TRIGGER_PROP_TARGET_MAP = "target_map";
inline constexpr std::string_view TRIGGER_PROP_START_OFFSET = "start_offset";

// start_offset values
inline constexpr std::string_view START_OFFSET_LEFT = "left";
inline constexpr std::string_view START_OFFSET_RIGHT = "right";
inline constexpr std::string_view START_OFFSET_TOP = "top";
inline constexpr std::string_view START_OFFSET_UP = "up";
inline constexpr std::string_view START_OFFSET_BOTTOM = "bottom";
inline constexpr std::string_view START_OFFSET_DOWN = "down";

} // namespace game::loader::tiled
