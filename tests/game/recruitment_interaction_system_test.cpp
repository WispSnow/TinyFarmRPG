#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "game/component/party_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/scripted_interaction_component.h"
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
#include <vector>

namespace game::system {
namespace {

struct RecruitOfferCapture {
    std::vector<game::defs::RecruitOfferRequestedEvent> requests{};

    void onRequest(const game::defs::RecruitOfferRequestedEvent& event) {
        requests.push_back(event);
    }
};

struct NotificationCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }
};

[[nodiscard]] game::data::RpgCatalog loadProjectActorCatalog() {
    game::data::RpgCatalog catalog;
    const auto actors_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg/actors.json").lexically_normal();
    EXPECT_TRUE(catalog.loadActors(actors_path.string()));
    return catalog;
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
    registry.emplace<game::component::RecruitableComponent>(
        npc,
        game::component::RecruitableComponent{
            .actor_id_ = "actor.tori",
            .actor_id_hash_ = entt::hashed_string{"actor.tori"}.value()});
    return npc;
}

TEST(RecruitmentInteractionSystemTest, RequestsRecruitOfferImmediatelyForNonScriptedRecruitable) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity npc = createRecruitableNpc(registry);

    RecruitOfferCapture offers;
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&RecruitOfferCapture::onRequest>(&offers);

    RecruitmentInteractionSystem system(registry, dispatcher, catalog);

    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    dispatcher.update();

    ASSERT_EQ(offers.requests.size(), 1U);
    EXPECT_EQ(offers.requests.front().actor_id, "actor.tori");
    EXPECT_EQ(offers.requests.front().player, player);
    EXPECT_EQ(offers.requests.front().recruiter, npc);
}

TEST(RecruitmentInteractionSystemTest, ScriptedRecruitableIsIgnored) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity npc = createRecruitableNpc(registry);
    registry.emplace<game::component::ScriptedInteractionComponent>(npc);

    RecruitOfferCapture offers;
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&RecruitOfferCapture::onRequest>(&offers);

    RecruitmentInteractionSystem system(registry, dispatcher, catalog);

    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    dispatcher.update();

    EXPECT_TRUE(offers.requests.empty());
}

TEST(RecruitmentInteractionSystemTest, AlreadyRecruitedShowsNotificationWithoutOffer) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity npc = createRecruitableNpc(registry);
    registry.get<game::component::PartyComponent>(player).recruited_actor_ids_.push_back("actor.tori");

    RecruitOfferCapture offers;
    NotificationCapture notifications;
    dispatcher.sink<game::defs::RecruitOfferRequestedEvent>().connect<&RecruitOfferCapture::onRequest>(&offers);
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&NotificationCapture::onShow>(&notifications);

    RecruitmentInteractionSystem system(registry, dispatcher, catalog);

    dispatcher.trigger(game::defs::InteractCommand{.player = player, .target = npc});
    dispatcher.update();

    EXPECT_TRUE(offers.requests.empty());
    ASSERT_EQ(notifications.shows.size(), 1U);
    EXPECT_EQ(notifications.shows.front().channel, game::defs::DialogueChannel::Notice);
    EXPECT_EQ(notifications.shows.front().text, "Tori is already in the party.");
}

} // namespace
} // namespace game::system
