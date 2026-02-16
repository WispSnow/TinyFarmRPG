#include <gtest/gtest.h>

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

} // namespace game::factory
