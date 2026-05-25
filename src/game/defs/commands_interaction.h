#pragma once

#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

// 交互意图命令（InteractionSystem 的“扩展点”）
// - InteractionSystem 只负责：选目标 + 发布命令（不写具体交互逻辑）
// - 具体交互由订阅者系统各自处理（Dialogue/Chest/Rest...）
// - 想加新交互对象时：优先新增订阅者，而不是改 InteractionSystem
struct InteractCommand {
    entt::entity player{entt::null};
    entt::entity target{entt::null};
};

/// @brief Script-owned chest opening request; Lua owns rewards, C++ owns chest visual/persistence lifecycle.
struct OpenScriptedChestCommand {
    entt::entity player{entt::null};
    entt::entity chest{entt::null};
    std::string notification_text{};
};

} // namespace game::defs
