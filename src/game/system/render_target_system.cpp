#include "render_target_system.h"
#include "game/component/target_component.h"
#include "engine/render/renderer.h"
#include "engine/component/tags.h"
#include "engine/utils/math.h"
#include "game/defs/constants.h"
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

namespace game::system {

RenderTargetSystem::RenderTargetSystem(entt::registry& registry)
    : registry_(registry) {

}

void RenderTargetSystem::update(engine::render::Renderer& renderer) {
    if (cursor_entity_ == entt::null) {
        createCursorEntity();
        if (cursor_entity_ == entt::null) {
            spdlog::error("创建光标实体失败");
            return;
        }
    }
    auto view = registry_.view<game::component::TargetComponent>(entt::exclude<engine::component::InvisibleTag>);
    for (auto entity : view) {
        const auto& cursor = view.get<game::component::TargetComponent>(entity);
        color_options_.start_color = cursor.color_;
        color_options_.end_color = cursor.color_;
        color_options_.use_gradient = false;

        renderer.drawFilledRect(engine::utils::Rect(cursor.position_, glm::vec2(game::defs::TILE_SIZE)), &color_options_, &transform_options_);
    }
}

void RenderTargetSystem::createCursorEntity() {
    cursor_entity_ = registry_.create();
    registry_.emplace<game::component::TargetComponent>(cursor_entity_);
    // 初始时默认不显示
    registry_.emplace<engine::component::InvisibleTag>(cursor_entity_);
}

} // namespace game::system