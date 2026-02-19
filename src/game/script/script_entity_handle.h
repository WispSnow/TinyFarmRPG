#pragma once

#include <entt/entity/entity.hpp>

#include <cstdint>

namespace game::script {

/// Lua 脚本侧传递的实体句柄。
///
/// 校验语义分两层：
/// 1) scene_token：跨场景代际校验，防止旧场景句柄在新场景误用。
/// 2) entt::entity（含 version）+ registry.valid：同场景内有效性校验，防 ABA。
struct ScriptEntityHandle {
    entt::entity entity{entt::null};
    std::uint64_t scene_token{0};
};

[[nodiscard]] bool isNullHandle(const ScriptEntityHandle& handle) noexcept;
[[nodiscard]] std::uint32_t toRawEntity(const ScriptEntityHandle& handle) noexcept;

} // namespace game::script

