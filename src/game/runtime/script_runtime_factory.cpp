#include "script_runtime_factory.h"

#include "engine/core/context.h"
#include "engine/script/script_host.h"
#include "game/runtime/game_content_manifest.h"
#include "game/script/tinyfarm_script_module.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

namespace game::runtime {

void ScriptRuntimeFactory::tryInitScriptHost(entt::registry& registry,
                                             engine::core::Context& context,
                                             GameRuntimeServices& services) {
    services.script_host = std::make_unique<engine::script::ScriptHost>(registry);
    auto* localization = services.localization_service.get();
    const std::vector<engine::script::ScriptModuleInstaller> installers{
        [localization](sol::state& lua,
                       engine::script::ScriptHost& host,
                       entt::registry& registry,
                       entt::dispatcher& dispatcher) {
            game::script::installTinyFarmScriptModule(lua, host, registry, dispatcher, localization);
        },
    };
    if (!services.script_host->init(context.getDispatcher(), installers)) {
        spdlog::warn("ScriptHost 初始化失败，脚本功能将禁用。");
        services.script_host.reset();
        return;
    }

    const std::filesystem::path bootstrap_script = GameContentManifest::ScriptBootstrap;
    if (!std::filesystem::exists(bootstrap_script)) {
        spdlog::info("ScriptHost: 未找到启动脚本 {}", bootstrap_script.string());
        return;
    }

    if (!services.script_host->loadFile(bootstrap_script.string())) {
        spdlog::warn("ScriptHost: 启动脚本执行失败，将继续游戏主流程。");
    } else {
        spdlog::info("ScriptHost: 启动脚本执行成功 {}", bootstrap_script.string());
    }
}

} // namespace game::runtime
