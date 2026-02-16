#include "render_system.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/tags.h"
#include <spdlog/spdlog.h>
#include <entt/entity/registry.hpp>

namespace engine::system {

void RenderSystem::update(entt::registry& registry, render::Renderer& renderer, const render::Camera& camera) {
    // 对RenderComponent进行排序 (需要自定义RenderComponent的比较运算符)
    registry.sort<component::RenderComponent>([](const auto& lhs, const auto& rhs) {
        return lhs < rhs;
    });

    // 执行渲染：EnTT view 默认会选择“最小”的 storage 进行迭代，因此这里强制使用已排序的 RenderComponent storage。
    renderer.beginFrame(camera);
    auto view = registry.view<component::RenderComponent, component::TransformComponent, component::SpriteComponent>(
        entt::exclude<component::InvisibleTag>
    );
    view.use<component::RenderComponent>();
    for (auto entity : view) {
        const auto& render = view.get<component::RenderComponent>(entity);
        const auto& transform = view.get<component::TransformComponent>(entity);
        const auto& sprite = view.get<component::SpriteComponent>(entity);
        auto size = sprite.size_ * transform.scale_;                    // 大小 = 精灵的大小 * 变换组件的缩放
        auto position = transform.position_ - sprite.pivot_ * size;     // 位置 = 变换组件的位置 - 精灵的锚点 * 大小
        // 绘制时应用Render组件中的颜色调整参数
        color_options_.start_color = render.color_;
        color_options_.end_color = render.color_;
        color_options_.use_gradient = false;

        transform_options_.rotation_radians = transform.rotation_;
        transform_options_.pivot = sprite.pivot_;

        renderer.drawSprite(sprite.sprite_, position, size, &color_options_, &transform_options_);
    }
}

} // namespace engine::system 
