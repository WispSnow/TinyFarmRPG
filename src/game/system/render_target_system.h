#pragma once
#include "engine/utils/defs.h"
#include <entt/entity/fwd.hpp>
#include <entt/entity/entity.hpp>

namespace engine::render {
    class Renderer;
}

namespace game::system {

class RenderTargetSystem {
    entt::registry& registry_;

    entt::entity cursor_entity_{entt::null};
    // 用于绘制的颜色选项和变换选项
    engine::utils::ColorOptions color_options_{};
    engine::utils::TransformOptions transform_options_{};

public:
    explicit RenderTargetSystem(entt::registry& registry);

    void update(engine::render::Renderer& renderer);

private:
    void createCursorEntity();
};

} // namespace game::system