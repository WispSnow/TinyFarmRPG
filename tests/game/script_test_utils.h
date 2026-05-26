#pragma once

#include "engine/script/script_module.h"
#include "game/runtime/localization_service.h"
#include "game/script/tinyfarm_script_module.h"

#include <filesystem>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::script::test {

inline game::runtime::LocalizationService& projectEnglishLocalization() {
    static game::runtime::LocalizationService localization = [] {
        game::runtime::LocalizationService service{};
        const auto manifest =
            (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal();
        (void)service.loadLanguageIndex(manifest.string());
        (void)service.setLanguage("en-US");
        return service;
    }();
    return localization;
}

inline std::vector<engine::script::ScriptModuleInstaller> tinyFarmInstallers(
    game::runtime::LocalizationService* localization) {
    return {
        [localization](sol::state& lua,
                       engine::script::ScriptHost& host,
                       entt::registry& registry,
                       entt::dispatcher& dispatcher) {
            game::script::installTinyFarmScriptModule(lua, host, registry, dispatcher, localization);
        },
    };
}

inline std::vector<engine::script::ScriptModuleInstaller> tinyFarmInstallers() {
    return tinyFarmInstallers(&projectEnglishLocalization());
}

} // namespace game::script::test
