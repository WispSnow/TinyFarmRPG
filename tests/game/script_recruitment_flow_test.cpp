#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/script/script_event_bridge.h"
#include "game/system/party_recruitment_system.h"
#include "game/system/recruitment_interaction_system.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

constexpr std::string_view LYRIA_ACTOR_ID = "actor.lyria";

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] std::string projectScriptRoot() {
    return (projectRoot() / "scripts").string();
}

[[nodiscard]] game::data::RpgCatalog loadRpgCatalog() {
    const auto rpg_root = projectRoot() / "assets/data/rpg";
    game::data::RpgCatalog catalog{};
    EXPECT_TRUE(catalog.loadManifest((rpg_root / "manifest.json").string()));
    EXPECT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    EXPECT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    EXPECT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    EXPECT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    EXPECT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    EXPECT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));
    return catalog;
}

[[nodiscard]] bool containsString(const std::vector<std::string>& values, std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
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

    [[nodiscard]] std::vector<std::string> textsFor(game::defs::DialogueChannel channel) const {
        std::vector<std::string> result{};
        for (const auto& show : shows) {
            if (show.channel == channel) {
                result.push_back(show.text);
            }
        }
        return result;
    }
};

struct RecruitOfferCapture {
    std::vector<game::defs::RecruitOfferRequestedEvent> requests{};

    void onRequest(const game::defs::RecruitOfferRequestedEvent& event) {
        requests.push_back(event);
    }
};

struct ScriptRecruitmentFlowEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::RpgCatalog rpg_catalog{loadRpgCatalog()};
    game::system::RecruitmentInteractionSystem recruitment_interaction_system;
    game::system::PartyRecruitmentSystem party_recruitment_system;
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    DialogueCapture capture{};
    entt::entity player{entt::null};
    entt::entity recruiter{entt::null};

    ScriptRecruitmentFlowEnv()
        : recruitment_interaction_system(registry, dispatcher, rpg_catalog),
          party_recruitment_system(registry, dispatcher, rpg_catalog),
          host(registry),
          bridge(host, registry, dispatcher) {
        registry.ctx().emplace<game::data::RpgCatalog*>(&rpg_catalog);

        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
        registry.emplace<game::component::PartyComponent>(player);

        recruiter = registry.create();
        registry.emplace<engine::component::TransformComponent>(recruiter, glm::vec2{24.0F, 0.0F});
        registry.emplace<engine::component::NameComponent>(
            recruiter,
            engine::component::NameComponent{
                .name_id_ = entt::hashed_string{"Lyria"}.value(),
                .name_ = "Lyria",
            });
        registry.emplace<game::component::ActorIdentityComponent>(
            recruiter,
            game::component::ActorIdentityComponent{
                .actor_id_ = std::string{LYRIA_ACTOR_ID},
                .actor_id_hash_ = entt::hashed_string{LYRIA_ACTOR_ID.data(), LYRIA_ACTOR_ID.size()}.value(),
                .blueprint_id_ = "lyria",
            });
        registry.emplace<game::component::RecruitableComponent>(
            recruiter,
            game::component::RecruitableComponent{
                .actor_id_ = std::string{LYRIA_ACTOR_ID},
                .actor_id_hash_ = entt::hashed_string{LYRIA_ACTOR_ID.data(), LYRIA_ACTOR_ID.size()}.value(),
            });
        registry.emplace<game::component::ScriptedInteractionComponent>(recruiter);

        dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);
        dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&capture);

        host.setScriptRoot(projectScriptRoot());
        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
    }

    ~ScriptRecruitmentFlowEnv() {
        dispatcher.disconnect(&capture);
    }

    void loadLyriaScript() {
        EXPECT_TRUE(host.exec(R"(
            tf.script.require("lib.dialogue")
            assert(tf.script.require("npcs.lyria") ~= nil)
        )"));
    }

    void interact() {
        ASSERT_TRUE(registry.valid(recruiter));
        dispatcher.trigger(game::defs::InteractCommand{player, recruiter});
        bridge.drainDeferredCommands();
        dispatcher.update();
    }

    [[nodiscard]] const game::component::PartyComponent& party() const {
        return registry.get<game::component::PartyComponent>(player);
    }
};

} // namespace

namespace game::script {

TEST(ScriptRecruitmentFlowTest, LyriaLuaDialogueRequestsOfferThenAcceptsAndRemovesMapNpc) {
    ScriptRecruitmentFlowEnv env{};
    RecruitOfferCapture offers{};
    env.dispatcher.sink<game::defs::RecruitOfferRequestedEvent>()
        .connect<&RecruitOfferCapture::onRequest>(&offers);

    env.loadLyriaScript();

    env.interact();
    EXPECT_EQ(env.capture.textsFor(game::defs::DialogueChannel::Conversation),
              std::vector<std::string>({"Hey there! Welcome to the valley."}));
    EXPECT_FALSE(containsString(env.party().recruited_actor_ids_, LYRIA_ACTOR_ID));
    EXPECT_TRUE(offers.requests.empty());

    env.interact();
    EXPECT_EQ(env.capture.textsFor(game::defs::DialogueChannel::Conversation),
              std::vector<std::string>({
                  "Hey there! Welcome to the valley.",
                  "I can help if you are heading into danger.",
              }));
    EXPECT_FALSE(containsString(env.party().recruited_actor_ids_, LYRIA_ACTOR_ID));
    EXPECT_TRUE(offers.requests.empty());

    env.interact();

    ASSERT_EQ(offers.requests.size(), 1U);
    EXPECT_EQ(offers.requests.front().player, env.player);
    EXPECT_EQ(offers.requests.front().recruiter, env.recruiter);
    EXPECT_EQ(offers.requests.front().actor_id, LYRIA_ACTOR_ID);
    EXPECT_TRUE(env.registry.valid(env.recruiter));
    EXPECT_FALSE(containsString(env.party().recruited_actor_ids_, LYRIA_ACTOR_ID));

    env.dispatcher.trigger(game::defs::RecruitPartyMemberCommand{
        .player = offers.requests.front().player,
        .recruiter = offers.requests.front().recruiter,
        .actor_id_hash = offers.requests.front().actor_id_hash,
        .actor_id = offers.requests.front().actor_id,
    });

    EXPECT_TRUE(containsString(env.party().recruited_actor_ids_, LYRIA_ACTOR_ID));
    EXPECT_TRUE(containsString(env.party().active_actor_ids_, LYRIA_ACTOR_ID));
    EXPECT_FALSE(env.registry.valid(env.recruiter));

    const auto* runtime_stats =
        env.registry.try_get<game::component::PartyRuntimeStatsComponent>(env.player);
    ASSERT_NE(runtime_stats, nullptr);
    EXPECT_TRUE(runtime_stats->states_by_actor_id_.contains(std::string{LYRIA_ACTOR_ID}));

    const auto notice_texts = env.capture.textsFor(game::defs::DialogueChannel::Notice);
    ASSERT_FALSE(notice_texts.empty());
    EXPECT_EQ(notice_texts.back(), "Lyria joined the party.");
    EXPECT_GE(env.capture.hide_count, 1);
}

} // namespace game::script
