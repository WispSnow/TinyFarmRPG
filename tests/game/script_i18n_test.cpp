#include <gtest/gtest.h>

#include "engine/script/script_host.h"
#include "game/runtime/localization_service.h"
#include "game/script/tinyfarm_script_module.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string manifestPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal().string();
}

} // namespace

TEST(ScriptI18nTest, TinyFarmModuleUsesCapturedLocalizationService) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    engine::script::ScriptHost host(registry);
    const std::vector<engine::script::ScriptModuleInstaller> installers{
        [&localization](sol::state& lua,
                        engine::script::ScriptHost& script_host,
                        entt::registry& script_registry,
                        entt::dispatcher& script_dispatcher) {
            game::script::installTinyFarmScriptModule(
                lua,
                script_host,
                script_registry,
                script_dispatcher,
                &localization);
        },
    };

    ASSERT_TRUE(host.init(dispatcher, installers));
    ASSERT_TRUE(host.exec(R"(
        translated = tf.i18n.tr("options.language")
        formatted = tf.i18n.format("options.battle_speed", {})
        integer_amount = tf.i18n.format("reward.gold_gained", { amount = 50 })
        float_amount = tf.i18n.format("reward.gold_gained", { amount = 50.0 })
    )"));

    EXPECT_EQ(host.luaState()["translated"].get<std::string>(), "语言");
    EXPECT_EQ(host.luaState()["formatted"].get<std::string>(), "战斗速度");
    EXPECT_EQ(host.luaState()["integer_amount"].get<std::string>(), "获得金币 50");
    EXPECT_EQ(host.luaState()["float_amount"].get<std::string>(), "获得金币 50");
}
