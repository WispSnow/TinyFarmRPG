#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <sol/sol.hpp>

#include <functional>

namespace engine::script {

class ScriptHost;

using ScriptModuleInstaller = std::function<void(sol::state& lua,
                                                 ScriptHost& host,
                                                 entt::registry& registry,
                                                 entt::dispatcher& dispatcher)>;

} // namespace engine::script
