#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "game/component/party_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/system/party_recruitment_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace game::system {
namespace {

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

[[nodiscard]] entt::entity createRecruiter(entt::registry& registry, const std::string_view actor_id) {
    const entt::entity recruiter = registry.create();
    registry.emplace<engine::component::TransformComponent>(recruiter, glm::vec2{32.0f, 16.0f});
    registry.emplace<engine::component::NameComponent>(
        recruiter,
        engine::component::NameComponent{
            .name_id_ = entt::hashed_string{"Recruiter"}.value(),
            .name_ = "Recruiter"});
    registry.emplace<game::component::RecruitableComponent>(
        recruiter,
        game::component::RecruitableComponent{
            .actor_id_ = std::string(actor_id),
            .actor_id_hash_ = entt::hashed_string{actor_id.data(), actor_id.size()}.value()});
    return recruiter;
}

TEST(PartyRecruitmentSystemTest, AddsRecruitToRecruitedAndActiveParty) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity recruiter = createRecruiter(registry, "actor.lyria");

    PartyRecruitmentSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RecruitPartyMemberCommand{
        .player = player,
        .recruiter = recruiter,
        .actor_id_hash = entt::hashed_string{"actor.lyria"}.value(),
        .actor_id = "actor.lyria"});

    const auto& party = registry.get<game::component::PartyComponent>(player);
    EXPECT_EQ(party.recruited_actor_ids_, std::vector<std::string>({"actor.player", "actor.lyria"}));
    EXPECT_EQ(party.active_actor_ids_, std::vector<std::string>({"actor.player", "actor.lyria"}));
}

TEST(PartyRecruitmentSystemTest, IgnoresMismatchedRecruiterComponent) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto catalog = loadProjectActorCatalog();
    const entt::entity player = createPlayer(registry);
    const entt::entity recruiter = createRecruiter(registry, "actor.tori");

    PartyRecruitmentSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RecruitPartyMemberCommand{
        .player = player,
        .recruiter = recruiter,
        .actor_id_hash = entt::hashed_string{"actor.lyria"}.value(),
        .actor_id = "actor.lyria"});

    const auto& party = registry.get<game::component::PartyComponent>(player);
    EXPECT_EQ(party.recruited_actor_ids_, std::vector<std::string>({"actor.player"}));
    EXPECT_EQ(party.active_actor_ids_, std::vector<std::string>({"actor.player"}));
}

} // namespace
} // namespace game::system
