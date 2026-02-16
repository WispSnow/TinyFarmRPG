#pragma once
#include <entt/entity/fwd.hpp>

namespace engine::system {

/**
 * @brief y-sort 排序系统
 *
 * 约定：把 `TransformComponent.position_.y` 写入 `RenderComponent.depth_`，让更靠下（y 更大）的对象更晚绘制，从而在视觉上遮挡在前。
 *
 * 注意：若希望遮挡自然，`TransformComponent.position_` 通常应代表“脚底/接地位置”，并配合精灵 `SpriteComponent.pivot_` 设为接近底部（如 (0.5, 1.0)）。
 * 否则高个子精灵可能出现“头在前、脚在后”等不自然遮挡。
 */
class YSortSystem {
public:
    void update(entt::registry& registry);
};

} // namespace engine::system
