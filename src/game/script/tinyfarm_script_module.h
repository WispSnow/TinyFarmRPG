#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <sol/sol.hpp>

namespace engine::script {
class ScriptHost;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::script {
/// 安装 TinyFarm 的 tf.* 脚本 API 扩展模块。
///
/// 该函数用于作为 ScriptModuleInstaller 注入到 engine::script::ScriptHost。
void installTinyFarmScriptModule(sol::state& lua,
                                 engine::script::ScriptHost& host,
                                 entt::registry& registry,
                                 entt::dispatcher& dispatcher,
                                 game::runtime::LocalizationService* localization = nullptr);

} // namespace game::script
