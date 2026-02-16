#pragma once
#include "engine/utils/defs.h"
#include <entt/entity/fwd.hpp>

namespace engine::render {
    class Renderer;
    class Camera;
}

namespace engine::system {

/**
 * @brief 渲染系统
 * 
 * 负责遍历所有带有 `RenderComponent + TransformComponent + SpriteComponent` 的实体，
 * 按 `(layer, depth)` 排序后调用 `render::Renderer` 进行绘制。
 *
 * 注意：EnTT view 默认会选择“最小”的 storage 进行迭代，因此如果要让 `registry.sort<RenderComponent>()` 生效，
 * 必须对 view 调用 `view.use<RenderComponent>()` 强制使用已排序的 storage（本项目有对应测试约束这一点）。
 */
class RenderSystem {
    // 用于绘制的颜色选项和变换选项
    engine::utils::ColorOptions color_options_{};
    engine::utils::TransformOptions transform_options_{};
    
public:
    /**
     * @brief 更新渲染系统
     * 
     * @param registry entt::registry 的引用
     * @param renderer Renderer 的引用
     * @param camera Camera 的引用
     */
    void update(entt::registry& registry, render::Renderer& renderer, const render::Camera& camera);
};

} // namespace engine::system 
