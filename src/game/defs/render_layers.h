#pragma once

#include "engine/component/render_component.h"

namespace game::defs {

// RenderComponent.layer_ 的约定层级（用于 gameplay 动态实体，通常应高于 Tiled 图层的 layer index）。
namespace render_layer {

// 角色/成熟作物等主要动态实体的默认渲染层。
inline constexpr int ACTOR = engine::component::RenderComponent::MAIN_LAYER;

// 种子阶段作物：在角色之下（避免遮挡玩家脚底）。
inline constexpr int CROP_SEED = engine::component::RenderComponent::MAIN_LAYER - 1;

// 存档恢复时使用的耕地/湿润耕地层：介于地图图层与角色之间。
inline constexpr int SOIL = engine::component::RenderComponent::MAIN_LAYER - 3;
inline constexpr int SOIL_WET = engine::component::RenderComponent::MAIN_LAYER - 2;

} // namespace render_layer

} // namespace game::defs
