#pragma once

#include <entt/entity/fwd.hpp>

namespace engine::system {

/// 实体移除系统 —— 帧末统一销毁带有 NeedRemoveTag 的实体
class RemoveEntitySystem {
public:
    void update(entt::registry& registry);
};

} // namespace engine::system
