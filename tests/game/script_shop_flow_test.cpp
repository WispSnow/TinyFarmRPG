#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/component/merchant_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/commands_shop.h"
#include "game/defs/events_dialogue.h"
#include "game/script/script_event_bridge.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

constexpr std::string_view MERCHANT_ACTOR_ID = "npc.josh";
constexpr std::string_view QUEST_ID = "quest.village.goblin_cleanup";
constexpr std::string_view DAY_SHOP_ID = "shop.village.general.day";
constexpr std::string_view NIGHT_SHOP_ID = "shop.village.general.night";
constexpr std::string_view POST_QUEST_SHOP_ID = "shop.village.general.post_slime_cleanup";

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] std::string projectScriptRoot() {
    return (projectRoot() / "scripts").string();
}

[[nodiscard]] game::data::ItemCatalog loadItemCatalog() {
    game::data::ItemCatalog catalog{};
    EXPECT_TRUE(catalog.loadItemConfig((projectRoot() / "assets/data/item_config.json").string()));
    return catalog;
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    game::data::QuestCatalog catalog{};
    EXPECT_TRUE(catalog.loadFromFile((projectRoot() / "assets/data/quests.json").string()));
    return catalog;
}

[[nodiscard]] game::data::ShopCatalog loadShopCatalog(const game::data::ItemCatalog& item_catalog) {
    game::data::ShopCatalog catalog{};
    EXPECT_TRUE(catalog.loadFromFile((projectRoot() / "assets/data/shops.json").string()));
    std::string validation_error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, validation_error)) << validation_error;
    return catalog;
}

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    int hide_count{0};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }

    void onHide(const game::defs::DialogueHideEvent&) {
        ++hide_count;
    }
};

struct ShopCommandCapture {
    std::vector<game::defs::OpenShopCommand> commands{};

    void onOpenShop(const game::defs::OpenShopCommand& command) {
        commands.push_back(command);
    }
};

struct ScriptShopFlowEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::GameTime game_time{};
    game::data::ItemCatalog item_catalog{loadItemCatalog()};
    game::data::QuestCatalog quest_catalog{loadQuestCatalog()};
    game::data::ShopCatalog shop_catalog{loadShopCatalog(item_catalog)};
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    DialogueCapture dialogue_capture{};
    ShopCommandCapture shop_capture{};
    entt::entity player{entt::null};
    entt::entity merchant{entt::null};

    explicit ScriptShopFlowEnv(const float hour) : host(registry), bridge(host, registry, dispatcher) {
        game_time.hour_ = hour;
        registry.ctx().emplace<game::data::GameTime>(game_time);
        registry.ctx().emplace<game::data::QuestCatalog*>(&quest_catalog);
        registry.ctx().emplace<game::data::ShopCatalog*>(&shop_catalog);

        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
        registry.emplace<game::component::QuestLogComponent>(player);

        merchant = registry.create();
        registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{24.0F, 0.0F});
        registry.emplace<engine::component::NameComponent>(merchant,
                                                           engine::component::NameComponent{
                                                               .name_id_ = entt::hashed_string{"Josh"}.value(),
                                                               .name_ = "Josh",
                                                           });
        registry.emplace<game::component::ActorIdentityComponent>(
            merchant,
            game::component::ActorIdentityComponent{
                .actor_id_ = std::string{MERCHANT_ACTOR_ID},
                .actor_id_hash_ = entt::hashed_string{MERCHANT_ACTOR_ID.data(), MERCHANT_ACTOR_ID.size()}.value(),
                .blueprint_id_ = "merchant",
            });
        registry.emplace<game::component::MerchantComponent>(
            merchant,
            game::component::MerchantComponent{
                .shop_id_ = "shop.village.general",
                .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value(),
            });
        registry.emplace<game::component::ScriptedInteractionComponent>(merchant);

        dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&dialogue_capture);
        dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue_capture);
        dispatcher.sink<game::defs::OpenShopCommand>().connect<&ShopCommandCapture::onOpenShop>(&shop_capture);

        host.setScriptRoot(projectScriptRoot());
        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
    }

    ~ScriptShopFlowEnv() {
        dispatcher.disconnect(&dialogue_capture);
        dispatcher.disconnect(&shop_capture);
    }

    void loadMerchantScript() {
        EXPECT_TRUE(host.exec(R"(
            tf.script.require("lib.dialogue")
            assert(tf.script.require("npcs.merchant") ~= nil)
        )"));
    }

    void interact() {
        dispatcher.trigger(game::defs::InteractCommand{player, merchant});
        bridge.drainDeferredCommands();
        dispatcher.update();
    }

    void markCleanupCompleted() {
        registry.get<game::component::QuestLogComponent>(player).completed_quests.push_back(std::string{QUEST_ID});
    }
};

void runMerchantDialogue(ScriptShopFlowEnv& env) {
    env.interact();
    ASSERT_EQ(env.dialogue_capture.shows.size(), 1U);
    EXPECT_TRUE(env.shop_capture.commands.empty());

    env.interact();
    ASSERT_EQ(env.shop_capture.commands.size(), 1U);
    EXPECT_EQ(env.shop_capture.commands.front().merchant, env.merchant);
    EXPECT_EQ(env.shop_capture.commands.front().player, env.player);
    EXPECT_EQ(env.dialogue_capture.hide_count, 1);
}

void expectMerchantShopAtHour(
    const float hour,
    const std::string_view expected_shop_id,
    const std::string_view expected_line) {
    SCOPED_TRACE(testing::Message() << "hour=" << hour);
    ScriptShopFlowEnv env{hour};
    env.loadMerchantScript();

    runMerchantDialogue(env);
    EXPECT_EQ(env.dialogue_capture.shows.front().text, expected_line);
    EXPECT_EQ(env.shop_capture.commands.front().shop_id, expected_shop_id);
}

} // namespace

namespace game::script {

TEST(ScriptShopFlowTest, MerchantLuaChoosesDayAndNightShopPresets) {
    ScriptShopFlowEnv day_env{10.0F};
    ASSERT_NE(day_env.shop_catalog.findShop(DAY_SHOP_ID), nullptr);
    day_env.loadMerchantScript();

    runMerchantDialogue(day_env);
    EXPECT_EQ(day_env.dialogue_capture.shows.front().text, "Welcome. Let me open the day shelf.");
    EXPECT_EQ(day_env.shop_capture.commands.front().shop_id, DAY_SHOP_ID);

    ScriptShopFlowEnv night_env{21.0F};
    ASSERT_NE(night_env.shop_catalog.findShop(NIGHT_SHOP_ID), nullptr);
    night_env.loadMerchantScript();

    runMerchantDialogue(night_env);
    EXPECT_EQ(night_env.dialogue_capture.shows.front().text,
              "Welcome. Let me open the night shelf.");
    EXPECT_EQ(night_env.shop_capture.commands.front().shop_id, NIGHT_SHOP_ID);
}

TEST(ScriptShopFlowTest, MerchantLuaUsesDocumentedDayNightBoundaryHours) {
    constexpr std::array cases{
        std::tuple{5.999F, NIGHT_SHOP_ID, std::string_view{"Welcome. Let me open the night shelf."}},
        std::tuple{6.0F, DAY_SHOP_ID, std::string_view{"Welcome. Let me open the day shelf."}},
        std::tuple{17.999F, DAY_SHOP_ID, std::string_view{"Welcome. Let me open the day shelf."}},
        std::tuple{18.0F, NIGHT_SHOP_ID, std::string_view{"Welcome. Let me open the night shelf."}},
    };

    for (const auto& [hour, expected_shop_id, expected_line] : cases) {
        expectMerchantShopAtHour(hour, expected_shop_id, expected_line);
    }
}

TEST(ScriptShopFlowTest, MerchantLuaChoosesPostQuestPresetWhenCleanupCompleted) {
    ScriptShopFlowEnv env{10.0F};
    ASSERT_NE(env.shop_catalog.findShop(POST_QUEST_SHOP_ID), nullptr);
    env.markCleanupCompleted();
    env.loadMerchantScript();

    runMerchantDialogue(env);
    EXPECT_EQ(env.dialogue_capture.shows.front().text, "Welcome back. I've set aside better stock.");
    EXPECT_EQ(env.shop_capture.commands.front().shop_id, POST_QUEST_SHOP_ID);
}

TEST(ScriptShopFlowTest, MerchantLuaShowsFallbackWhenShopOpenFails) {
    ScriptShopFlowEnv env{10.0F};
    env.loadMerchantScript();
    env.registry.ctx().erase<game::data::ShopCatalog*>();

    env.interact();
    ASSERT_EQ(env.dialogue_capture.shows.size(), 1U);
    EXPECT_EQ(env.dialogue_capture.shows.front().text, "Welcome. Let me open the day shelf.");

    env.interact();

    EXPECT_TRUE(env.shop_capture.commands.empty());
    ASSERT_EQ(env.dialogue_capture.shows.size(), 2U);
    EXPECT_EQ(env.dialogue_capture.shows.back().text, "Sorry, the shop is closed.");
    EXPECT_EQ(env.dialogue_capture.hide_count, 1);
}

} // namespace game::script
