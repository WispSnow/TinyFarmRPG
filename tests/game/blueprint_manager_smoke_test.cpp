#include <gtest/gtest.h>

#include "game/component/party_component.h"
#include "game/component/quest_log_component.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <filesystem>
#include <fstream>
#include <glm/vec2.hpp>
#include <chrono>

namespace game::factory {

TEST(BlueprintManagerTest, LoadActorBlueprints_LoadsProjectAssetFile) {
    BlueprintManager manager;
    const std::string path = std::string(PROJECT_SOURCE_DIR) + "/assets/data/actor_blueprint.json";

    ASSERT_TRUE(manager.loadActorBlueprints(path));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"player"}.value()));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"lyria"}.value()));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"tori"}.value()));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"goblin"}.value()));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"gnome"}.value()));
    EXPECT_TRUE(manager.hasActorBlueprint(entt::hashed_string{"slime"}.value()));

    const auto& tori = manager.getActorBlueprint(entt::hashed_string{"tori"}.value());
    EXPECT_EQ(tori.name_, "Tori");
    EXPECT_EQ(tori.sprite_.path_, "assets/farm-rpg/Character and Portrait/Character/Pre-made/Tori/Idle.png");
    EXPECT_FLOAT_EQ(tori.wander_radius_, 48.0f);

    const auto findToriAnimation = [&tori](const char* name) -> const AnimationBlueprint* {
        const auto it = tori.animations_.find(entt::hashed_string{name}.value());
        return it == tori.animations_.end() ? nullptr : &it->second;
    };
    ASSERT_NE(findToriAnimation("idle_down"), nullptr);
    ASSERT_NE(findToriAnimation("idle_up"), nullptr);
    ASSERT_NE(findToriAnimation("idle_right"), nullptr);
    ASSERT_NE(findToriAnimation("walk_down"), nullptr);

    const auto& slime = manager.getActorBlueprint(entt::hashed_string{"slime"}.value());
    EXPECT_EQ(slime.name_, "Slime");
    EXPECT_EQ(slime.sprite_.path_, "assets/farm-rpg/Enemy/Slimes/Blue/Slime/Idle.png");
    EXPECT_FLOAT_EQ(slime.sprite_.src_rect_.pos.y, 32.0f);
    EXPECT_FLOAT_EQ(slime.wander_radius_, 48.0f);

    const auto findAnimation = [&slime](const char* name) -> const AnimationBlueprint* {
        const auto it = slime.animations_.find(entt::hashed_string{name}.value());
        return it == slime.animations_.end() ? nullptr : &it->second;
    };
    const auto* idle_left = findAnimation("idle_left");
    const auto* idle_down = findAnimation("idle_down");
    const auto* idle_up = findAnimation("idle_up");
    const auto* idle_right = findAnimation("idle_right");
    const auto* walk_down = findAnimation("walk_down");
    ASSERT_NE(idle_left, nullptr);
    ASSERT_NE(idle_down, nullptr);
    ASSERT_NE(idle_up, nullptr);
    ASSERT_NE(idle_right, nullptr);
    ASSERT_NE(walk_down, nullptr);
    EXPECT_FLOAT_EQ(idle_left->position_.y, 0.0f);
    EXPECT_FLOAT_EQ(idle_down->position_.y, 32.0f);
    EXPECT_FLOAT_EQ(idle_up->position_.y, 64.0f);
    EXPECT_FLOAT_EQ(idle_right->position_.y, 0.0f);
    EXPECT_TRUE(idle_right->flip_horizontal_);
    EXPECT_FLOAT_EQ(walk_down->position_.y, 32.0f);

    const auto& goblin = manager.getActorBlueprint(entt::hashed_string{"goblin"}.value());
    EXPECT_EQ(goblin.sprite_.path_, "assets/farm-rpg/Enemy/Goblins/Archer Goblin/Idle.png");
    EXPECT_NE(goblin.animations_.find(entt::hashed_string{"idle_right"}.value()), goblin.animations_.end());
    EXPECT_NE(goblin.animations_.find(entt::hashed_string{"idle_left"}.value()), goblin.animations_.end());

    const auto& gnome = manager.getActorBlueprint(entt::hashed_string{"gnome"}.value());
    EXPECT_EQ(gnome.sprite_.path_, "assets/farm-rpg/Enemy/Goblins/Spear Goblin/Idle.png");
    EXPECT_NE(gnome.animations_.find(entt::hashed_string{"idle_right"}.value()), gnome.animations_.end());
    EXPECT_NE(gnome.animations_.find(entt::hashed_string{"idle_left"}.value()), gnome.animations_.end());
}

TEST(BlueprintManagerTest, LoadActorBlueprints_MissingFileReturnsFalse) {
    BlueprintManager manager;
    EXPECT_FALSE(manager.loadActorBlueprints("this_file_should_not_exist.json"));
}

TEST(BlueprintManagerTest, LoadActorBlueprints_InvalidJsonReturnsFalse) {
    BlueprintManager manager;
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("tinyfarm_invalid_actor_blueprint_" + std::to_string(tick) + ".json");

    {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open());
        file << "{ invalid json }";
    }

    EXPECT_FALSE(manager.loadActorBlueprints(path.string()));
    std::filesystem::remove(path);
}

TEST(BlueprintManagerTest, LoadAnimalBlueprints_MissingFileReturnsFalse) {
    BlueprintManager manager;
    EXPECT_FALSE(manager.loadAnimalBlueprints("this_file_should_not_exist.json"));
}

TEST(BlueprintManagerTest, LoadAnimalBlueprints_InvalidJsonReturnsFalse) {
    BlueprintManager manager;
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("tinyfarm_invalid_animal_blueprint_" + std::to_string(tick) + ".json");

    {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open());
        file << "{ invalid json }";
    }

    EXPECT_FALSE(manager.loadAnimalBlueprints(path.string()));
    std::filesystem::remove(path);
}

TEST(BlueprintManagerTest, LoadAnimalBlueprints_LoadsIdleSoundConfigs) {
    BlueprintManager manager;
    const std::string path = std::string(PROJECT_SOURCE_DIR) + "/assets/data/animal_blueprint.json";

    ASSERT_TRUE(manager.loadAnimalBlueprints(path));

    const entt::id_type idle_id = entt::hashed_string{"idle"}.value();

    const auto& cow = manager.getAnimalBlueprint(entt::hashed_string{"cow"}.value());
    const auto cow_it = cow.sounds_.triggers_.find(idle_id);
    ASSERT_NE(cow_it, cow.sounds_.triggers_.end());
    EXPECT_EQ(cow_it->second.sound_id_, entt::hashed_string{"cow_moo"}.value());
    EXPECT_FLOAT_EQ(cow_it->second.probability_, 0.4f);
    EXPECT_FLOAT_EQ(cow_it->second.cooldown_seconds_, 6.0f);

    const auto& sheep = manager.getAnimalBlueprint(entt::hashed_string{"sheep"}.value());
    const auto sheep_it = sheep.sounds_.triggers_.find(idle_id);
    ASSERT_NE(sheep_it, sheep.sounds_.triggers_.end());
    EXPECT_EQ(sheep_it->second.sound_id_, entt::hashed_string{"sheep_baa"}.value());
    EXPECT_FLOAT_EQ(sheep_it->second.probability_, 0.35f);
    EXPECT_FLOAT_EQ(sheep_it->second.cooldown_seconds_, 7.0f);
}

TEST(EntityFactoryTest, CreateActor_MissingBlueprintReturnsNull) {
    entt::registry registry;
    BlueprintManager manager;
    EntityFactory factory(registry, manager, nullptr, nullptr);

    const entt::entity entity = factory.createActor(entt::hashed_string{"missing_actor"}.value(), glm::vec2{0.0f, 0.0f});
    EXPECT_EQ(entity, entt::entity{entt::null});
}

TEST(EntityFactoryTest, CreateActor_PlayerGetsQuestLogComponent) {
    entt::registry registry;
    BlueprintManager manager;
    const std::string path = std::string(PROJECT_SOURCE_DIR) + "/assets/data/actor_blueprint.json";
    ASSERT_TRUE(manager.loadActorBlueprints(path));

    EntityFactory factory(registry, manager, nullptr, nullptr);
    const entt::entity entity = factory.createActor(entt::hashed_string{"player"}.value(), glm::vec2{16.0f, 24.0f});

    ASSERT_NE(entity, entt::entity{entt::null});
    EXPECT_TRUE(registry.all_of<game::component::QuestLogComponent>(entity));
    EXPECT_TRUE(registry.all_of<game::component::PartyComponent>(entity));
}

} // namespace game::factory
