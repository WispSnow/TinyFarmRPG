#pragma once

#include "engine/script/script_module.h"
#include "game/script/tinyfarm_script_module.h"

#include <vector>

namespace game::script::test {

inline std::vector<engine::script::ScriptModuleInstaller> tinyFarmInstallers() {
    return {
        game::script::installTinyFarmScriptModule,
    };
}

} // namespace game::script::test
