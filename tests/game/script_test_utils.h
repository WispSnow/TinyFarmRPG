#pragma once

#include "engine/script/script_module.h"
#include "game/script/tinyfarm_script_module.h"

#include <vector>

namespace game::script::test {

inline std::vector<engine::script::ScriptModuleInstaller> tinyFarmInstallers() {
    return {
        [](sol::state& lua,
           engine::script::ScriptHost& host,
           entt::registry& registry,
           entt::dispatcher& dispatcher) {
            game::script::installTinyFarmScriptModule(lua, host, registry, dispatcher);
        },
    };
}

} // namespace game::script::test
