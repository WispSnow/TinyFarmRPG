#include "script_entity_handle.h"

namespace engine::script {

bool isNullHandle(const ScriptEntityHandle& handle) noexcept {
    return handle.entity == entt::null || handle.scene_token == 0;
}

std::uint32_t toRawEntity(const ScriptEntityHandle& handle) noexcept {
    return static_cast<std::uint32_t>(handle.entity);
}

} // namespace engine::script
