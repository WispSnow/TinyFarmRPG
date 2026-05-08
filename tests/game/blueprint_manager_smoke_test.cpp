#include <gtest/gtest.h>

#include "game/component/party_component.h"
#include "game/component/quest_log_component.h"
#include "game/data/rpg_catalog.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

namespace game::factory {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Keep this normalization in sync with the battle_scene.cpp helper of the same name.
[[nodiscard]] std::string battleEnemyIconSpriteName(std::string_view sprite_blueprint_id) {
    std::string normalized;
    bool previous_was_separator = true;

    for (const unsigned char character : sprite_blueprint_id) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
            previous_was_separator = false;
            continue;
        }

        if (!previous_was_separator) {
            normalized.push_back('-');
            previous_was_separator = true;
        }
    }

    while (!normalized.empty() && normalized.back() == '-') {
        normalized.pop_back();
    }

    return normalized.empty() ? std::string{} : "battle-enemy-icon-" + normalized;
}

} // namespace

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
    EXPECT_NE(goblin.animations_.find(entt::hashed_string{"idle_down"}.value()), goblin.animations_.end());
    EXPECT_NE(goblin.animations_.find(entt::hashed_string{"idle_right"}.value()), goblin.animations_.end());
    EXPECT_NE(goblin.animations_.find(entt::hashed_string{"idle_left"}.value()), goblin.animations_.end());

    const auto& gnome = manager.getActorBlueprint(entt::hashed_string{"gnome"}.value());
    EXPECT_EQ(gnome.sprite_.path_, "assets/farm-rpg/Enemy/Goblins/Spear Goblin/Idle.png");
    EXPECT_NE(gnome.animations_.find(entt::hashed_string{"idle_down"}.value()), gnome.animations_.end());
    EXPECT_NE(gnome.animations_.find(entt::hashed_string{"idle_right"}.value()), gnome.animations_.end());
    EXPECT_NE(gnome.animations_.find(entt::hashed_string{"idle_left"}.value()), gnome.animations_.end());
}

TEST(BlueprintManagerTest, ProjectBattleEnemiesExposeTurnOrderIdleDownIcons) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR};
    const std::filesystem::path enemy_path = (project_root / "assets/data/rpg/enemies.json").lexically_normal();
    const std::filesystem::path actor_blueprint_path = (project_root / "assets/data/actor_blueprint.json").lexically_normal();
    const std::filesystem::path icon_rcss_path = (project_root / "ui/rmlui/theme/battle_enemy_icons.rcss").lexically_normal();

    BlueprintManager manager;
    ASSERT_TRUE(manager.loadActorBlueprints(actor_blueprint_path.string()));

    const std::string icon_rcss = readTextFile(icon_rcss_path);
    ASSERT_FALSE(icon_rcss.empty());

    std::ifstream enemy_file(enemy_path);
    ASSERT_TRUE(enemy_file.is_open()) << enemy_path;
    const nlohmann::json root = nlohmann::json::parse(enemy_file, nullptr, false);
    ASSERT_FALSE(root.is_discarded()) << enemy_path;
    ASSERT_TRUE(root.contains("enemies"));
    ASSERT_TRUE(root["enemies"].is_array());

    std::size_t checked_enemy_count = 0;
    for (const auto& enemy_node : root["enemies"]) {
        ASSERT_TRUE(enemy_node.is_object());
        const auto visual_it = enemy_node.find("battle_visual");
        if (visual_it == enemy_node.end() || !visual_it->is_object()) {
            continue;
        }

        const std::string sprite_blueprint_id = visual_it->value("sprite_blueprint_id", std::string{});
        if (sprite_blueprint_id.empty()) {
            continue;
        }

        const entt::id_type blueprint_id = game::data::RpgCatalog::hashId(sprite_blueprint_id);
        ASSERT_TRUE(manager.hasActorBlueprint(blueprint_id)) << sprite_blueprint_id;

        const auto& blueprint = manager.getActorBlueprint(blueprint_id);
        const auto idle_down_it = blueprint.animations_.find(game::data::RpgCatalog::hashId("idle_down"));
        ASSERT_NE(idle_down_it, blueprint.animations_.end()) << sprite_blueprint_id;
        EXPECT_FALSE(idle_down_it->second.frames_.empty()) << sprite_blueprint_id;

        const std::string sprite_name = battleEnemyIconSpriteName(sprite_blueprint_id);
        ASSERT_FALSE(sprite_name.empty()) << sprite_blueprint_id;
        EXPECT_NE(icon_rcss.find(sprite_name), std::string::npos) << sprite_name;
        ++checked_enemy_count;
    }

    EXPECT_GT(checked_enemy_count, 0U);
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
