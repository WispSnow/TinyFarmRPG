#pragma once
#include <entt/entity/fwd.hpp>

namespace engine::render {
class Renderer;
}

namespace engine::system {

/// 光照系统 —— 收集 PointLight/SpotLight/Emissive 组件，提交到 Renderer 各渲染通道
class LightSystem {
public:
    void update(entt::registry& registry, engine::render::Renderer& renderer);
};

} // namespace engine::system
