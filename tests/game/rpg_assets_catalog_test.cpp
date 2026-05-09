#include <gtest/gtest.h>

#include "engine/vfx/vfx_catalog.h"
#include "game/data/rpg_catalog.h"

#include <entt/core/hashed_string.hpp>

#include <cmath>
#include <filesystem>
#include <string>

namespace game::data {
namespace {

TEST(RpgAssetsCatalogTest, ProjectRpgAssetsLoadAndResolveSkillTargetVfxReferences) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path rpg_root = project_root / "assets/data/rpg";

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadManifest((rpg_root / "manifest.json").string()));
    ASSERT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    ASSERT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    ASSERT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    ASSERT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    ASSERT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    ASSERT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));

    std::string error{};
    ASSERT_TRUE(catalog.validateReferences(error)) << error;

    engine::vfx::VfxCatalog vfx_catalog;
    ASSERT_TRUE(vfx_catalog.loadFromFile((project_root / "assets/data/vfx_catalog.json").string()));

    const auto* alex = catalog.findActor("actor.player");
    ASSERT_NE(alex, nullptr);
    ASSERT_EQ(alex->skill_ids_.size(), 2U);
    EXPECT_EQ(alex->skill_ids_[0], "skill.attack");
    EXPECT_EQ(alex->skill_ids_[1], "skill.bash");

    const auto* attack = catalog.findSkill("skill.attack");
    ASSERT_NE(attack, nullptr);
    EXPECT_EQ(attack->target_vfx_id_, "battle.hit_physical");
    EXPECT_GT(attack->target_vfx_scale_, 0.0F);
    EXPECT_TRUE(std::isfinite(attack->target_vfx_offset_.x));
    EXPECT_TRUE(std::isfinite(attack->target_vfx_offset_.y));
    const auto* attack_hit_path = vfx_catalog.findEffectPath(attack->target_vfx_id_hash_);
    ASSERT_NE(attack_hit_path, nullptr);
    EXPECT_EQ(*attack_hit_path, "assets/vfx/effects/HitEffect.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *attack_hit_path));

    const auto* lyria = catalog.findActor("actor.lyria");
    ASSERT_NE(lyria, nullptr);
    EXPECT_EQ(lyria->class_id_, "class.mage");
    ASSERT_EQ(lyria->skill_ids_.size(), 3U);
    EXPECT_EQ(lyria->skill_ids_[0], "skill.attack");
    EXPECT_EQ(lyria->skill_ids_[1], "skill.fire_1");
    EXPECT_EQ(lyria->skill_ids_[2], "skill.thunder_1");

    const auto* tori = catalog.findActor("actor.tori");
    ASSERT_NE(tori, nullptr);
    ASSERT_EQ(tori->skill_ids_.size(), 2U);
    EXPECT_EQ(tori->skill_ids_[0], "skill.attack");
    EXPECT_EQ(tori->skill_ids_[1], "skill.heal_1");

    const auto* fire = catalog.findSkill("skill.fire_1");
    ASSERT_NE(fire, nullptr);
    EXPECT_EQ(fire->target_vfx_id_, "battle.fire_one_1");
    const auto* fire_path = vfx_catalog.findEffectPath(fire->target_vfx_id_hash_);
    ASSERT_NE(fire_path, nullptr);
    EXPECT_EQ(*fire_path, "assets/vfx/effects/FireOne1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *fire_path));

    const auto* thunder = catalog.findSkill("skill.thunder_1");
    ASSERT_NE(thunder, nullptr);
    EXPECT_EQ(thunder->target_vfx_id_, "battle.thunder_one_1");
    const auto* thunder_path = vfx_catalog.findEffectPath(thunder->target_vfx_id_hash_);
    ASSERT_NE(thunder_path, nullptr);
    EXPECT_EQ(*thunder_path, "assets/vfx/effects/ThunderOne1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *thunder_path));

    const auto* heal = catalog.findSkill("skill.heal_1");
    ASSERT_NE(heal, nullptr);
    EXPECT_EQ(heal->target_vfx_id_, "battle.heal_all_1");
    const auto* heal_path = vfx_catalog.findEffectPath(heal->target_vfx_id_hash_);
    ASSERT_NE(heal_path, nullptr);
    EXPECT_EQ(*heal_path, "assets/vfx/effects/HealAll1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *heal_path));

    const entt::id_type physical_hit_id = entt::hashed_string{"battle.hit_physical"}.value();
    EXPECT_EQ(attack->target_vfx_id_hash_, physical_hit_id);
}

} // namespace
} // namespace game::data
