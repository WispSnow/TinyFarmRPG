#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "game/component/npc_component.h"
#include "game/component/party_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/system/recruitment_interaction_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace game::system {
namespace {

struct RecruitOfferCapture {
    std::vector<game::defs::RecruitOfferRequestedEvent> requests{};

    void onRequest(const game::defs::RecruitOfferRequestedEvent& event) {
        requests.push_back(event);
    }
};

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    int hides{0};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }

    void onHide(const game::defs::DialogueHideEvent&) {
        ++hides;
    }
};

[[nodiscard]] game::data::RpgCatalog loadProjectActorCatalog() {
    game::data::RpgCatalog catalog;
    const auto actors_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg/actors.json").lexically_normal();
    EXPECT_TRUE(catalog.loadActors(actors_path.string()));
    return catalog;
}

[[nodiscard]] std::filesystem::path writeDialogueFixture() {
    const auto temp_root = game::test::createUniqueTempDir("recruitment_dialogue_fixture");
    const auto dialogue_path = temp_root / "dialogue.json";
    game::test::writeTextFile(
        dialogue_path,
        R"json({
  "tori_intro": [
    "Let me join you."
  ]
})json");
    return dialogue_path;
}

[[nodiscard]] entt::entity createPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{16.0f, 16.0f});
    registry.emplace<game::component::PartyComponent>(player);
    return player;
}

[[nodiscard]] entt::entity createRecruitableNpc(entt::registry& registry) {
    const entt::entity npc = registry.create();
    registry.emplace<engine::component::TransformComponent>(npc, glm::vec2{32.0f, 16.0f});
    registry.emplace<engine::component::NameComponent>(
        npc,
        engine::component::NameComponent{
            .name_id_ = entt::hashed_string{"Tori"}.value(),
            .name_ = "Tori"});
    registry.emplace<game::component::DialogueComponent>(
        npc,
        game::component::DialogueComponent{entt::hashed_string{"tori_intro"}.value()});
    registry.emplace<game::component::RecruitableComponent>(
        npc,
        game::component::RecruitableComponent{
            .actor_id_ = "actor.tori",
            .actor_id_hash_ = entt::hashed_string{"actor.tori"}.value()});
    return npc;
}

TEST(RecruitmentInteractionSystemTest, RequestsRecruitOfferAfterDialogueCompletes) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity npc = createRecruitableNpc(registry);

    RecruitOfferCapture offers;
    DialogueCapture dialogue;
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&RecruitOfferCapture::onRequest>(&offers);
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&dialogue);
    dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue);

    RecruitmentInteractionSystem system(registry, dispatcher, catalog);
    ASSERT_TRUE(system.loadDialogueFile(writeDialogueFixture().string()));

    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    ASSERT_EQ(dialogue.shows.size(), 1U);
    EXPECT_EQ(dialogue.shows.back().text, "Let me join you.");

    registry.get<game::component::DialogueComponent>(npc).cooldown_timer_ = 0.0f;
    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    dispatcher.update();

    ASSERT_EQ(offers.requests.size(), 1U);
    EXPECT_EQ(offers.requests.front().actor_id, "actor.tori");
    EXPECT_EQ(offers.requests.front().player, player);
    EXPECT_EQ(offers.requests.front().recruiter, npc);
    EXPECT_EQ(dialogue.hides, 1);
}

TEST(RecruitmentInteractionSystemTest, LeavingNpcRangeClosesDialogueWithoutRecruitOffer) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity npc = createRecruitableNpc(registry);

    RecruitOfferCapture offers;
    DialogueCapture dialogue;
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&RecruitOfferCapture::onRequest>(&offers);
    dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue);

    RecruitmentInteractionSystem system(registry, dispatcher, catalog);
    ASSERT_TRUE(system.loadDialogueFile(writeDialogueFixture().string()));

    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    registry.get<engine::component::TransformComponent>(player).position_ = glm::vec2{400.0f, 400.0f};
    system.update(0.016f);
    dispatcher.update();

    EXPECT_TRUE(offers.requests.empty());
    EXPECT_EQ(dialogue.hides, 1);
    EXPECT_FALSE(registry.get<game::component::DialogueComponent>(npc).active_);
}

} // namespace
} // namespace game::system
