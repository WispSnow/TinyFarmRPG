#pragma once

#include <string>
#include <entt/entity/entity.hpp>

namespace engine::component {

/**
 * @brief 名称组件，可用于标记实体名称。
 */
struct NameComponent {
    entt::id_type name_id_{entt::null};   ///< @brief 名称ID（缓存 entt::hashed_string(name_) 以加速查找）
    std::string name_;                    ///< @brief 名称
};

} // namespace engine::component