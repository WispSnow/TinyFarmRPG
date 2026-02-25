#pragma once
#include "engine/component/sprite_component.h"
#include "engine/utils/defs.h"
#include <vector>
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
    struct DrawRequest {
        int layer{0};
        float depth{0.0f};
        component::Sprite sprite{};
        glm::vec2 position{0.0f, 0.0f};
        glm::vec2 size{0.0f, 0.0f};
        engine::utils::FColor color{engine::utils::FColor::white()};
        engine::utils::TransformOptions transform{};
    };

    // 用于绘制的颜色选项和变换选项
    engine::utils::ColorOptions color_options_{};
    engine::utils::TransformOptions transform_options_{};
    // 复用缓冲，避免每帧分层 draw request 反复分配
    std::vector<DrawRequest> draw_requests_{};
    
public:
    /**
     * @brief 渲染阶段入口
     * 
     * @param registry entt::registry 的引用
     * @param renderer Renderer 的引用
     * @param camera Camera 的引用
     */
    void render(entt::registry& registry, render::Renderer& renderer, const render::Camera& camera, float interpolation_alpha);
};

} // namespace engine::system 
