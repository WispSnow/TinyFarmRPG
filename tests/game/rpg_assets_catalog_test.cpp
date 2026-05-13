#include <gtest/gtest.h>

#include "engine/resource/asset_registry.h"
#include "engine/utils/json_file_loader.h"
#include "engine/vfx/vfx_catalog.h"
#include "game/data/audio_cue_catalog.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace game::data {
namespace {

using Json = nlohmann::json;

void expectSkillTargetSfx(const Json& sound_mapping,
                          const std::filesystem::path& project_root,
                          const SkillData& skill,
                          std::string_view expected_sfx_id,
                          std::string_view expected_path) {
    EXPECT_EQ(skill.presentation_.target_sfx_id_, std::string{expected_sfx_id});
    EXPECT_EQ(skill.presentation_.target_sfx_id_hash_, RpgCatalog::hashId(expected_sfx_id));

    const auto sfx_it = sound_mapping.find(std::string{expected_sfx_id});
    ASSERT_NE(sfx_it, sound_mapping.end()) << expected_sfx_id;
    ASSERT_TRUE(sfx_it->is_string()) << expected_sfx_id;
    EXPECT_EQ(sfx_it->get<std::string>(), std::string{expected_path});
    EXPECT_TRUE(std::filesystem::exists(project_root / sfx_it->get<std::string>()));
}

TEST(RpgAssetsCatalogTest, ProjectRpgAssetsLoadAndResolveBattlePresentationReferences) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path rpg_root = project_root / "assets/data/rpg";

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadManifest((rpg_root / "manifest.json").string()));
    ASSERT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    ASSERT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    ASSERT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    ASSERT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    ASSERT_TRUE(catalog.loadEquipment((rpg_root / "equipment.json").string()));
    ASSERT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    ASSERT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadIconConfig((project_root / "assets/data/icon_config.json").string()));
    ASSERT_TRUE(item_catalog.loadItemConfig((project_root / "assets/data/item_config.json").string()));

    std::string error{};
    ASSERT_TRUE(catalog.validateReferences(error, &item_catalog)) << error;

    struct ExpectedEquipmentEntry {
        std::string_view item_id{};
        EquipmentSlotId slot{EquipmentSlotId::Unknown};
        std::string_view icon_key{};
    };
    constexpr std::array<ExpectedEquipmentEntry, 12> kExpectedEquipment{{
        {"equip_wooden_sword", EquipmentSlotId::Weapon, "equipment/wooden_sword"},
        {"equip_wooden_staff", EquipmentSlotId::Weapon, "equipment/wooden_staff"},
        {"equip_wooden_helmet", EquipmentSlotId::Head, "equipment/wooden_helmet"},
        {"equip_wooden_armor", EquipmentSlotId::Body, "equipment/wooden_armor"},
        {"equip_wooden_boots", EquipmentSlotId::Boot, "equipment/wooden_boots"},
        {"equip_wooden_accessory", EquipmentSlotId::Accessory, "equipment/wooden_accessory"},
        {"equip_iron_sword", EquipmentSlotId::Weapon, "equipment/iron_sword"},
        {"equip_iron_staff", EquipmentSlotId::Weapon, "equipment/iron_staff"},
        {"equip_iron_helmet", EquipmentSlotId::Head, "equipment/iron_helmet"},
        {"equip_iron_armor", EquipmentSlotId::Body, "equipment/iron_armor"},
        {"equip_iron_boots", EquipmentSlotId::Boot, "equipment/iron_boots"},
        {"equip_iron_accessory", EquipmentSlotId::Accessory, "equipment/iron_accessory"},
    }};
    for (const auto& expected : kExpectedEquipment) {
        const auto* equipment = catalog.findEquipmentByItem(expected.item_id);
        ASSERT_NE(equipment, nullptr) << expected.item_id;
        EXPECT_EQ(equipment->slot_, expected.slot) << expected.item_id;

        const auto* item = item_catalog.findItem(RpgCatalog::hashId(expected.item_id));
        ASSERT_NE(item, nullptr) << expected.item_id;
        const auto* icon_key = item_catalog.findIconKey(item->icon_id_);
        ASSERT_NE(icon_key, nullptr) << expected.item_id;
        EXPECT_EQ(*icon_key, expected.icon_key) << expected.item_id;
    }
    for (const auto* equipment : catalog.listEquipment()) {
        ASSERT_NE(equipment, nullptr);
        EXPECT_NE(equipment->slot_, EquipmentSlotId::Offhand) << equipment->item_id_;
    }

    engine::vfx::VfxCatalog vfx_catalog;
    ASSERT_TRUE(vfx_catalog.loadFromFile((project_root / "assets/data/vfx_catalog.json").string()));

    Json resource_mapping{};
    ASSERT_TRUE(engine::utils::loadJsonObjectFile(
        (project_root / "assets/data/resource_mapping.json").string(),
        resource_mapping,
        "RpgAssetsCatalogTest"));
    const auto sound_mapping_it = resource_mapping.find("sound");
    ASSERT_NE(sound_mapping_it, resource_mapping.end());
    ASSERT_TRUE(sound_mapping_it->is_object());
    const auto music_mapping_it = resource_mapping.find("music");
    ASSERT_NE(music_mapping_it, resource_mapping.end());
    ASSERT_TRUE(music_mapping_it->is_object());

    AudioCueCatalog audio_cue_catalog;
    ASSERT_TRUE(audio_cue_catalog.loadFromFile((project_root / "assets/data/audio_cues.json").string()));
    engine::resource::AssetRegistry asset_registry;
    for (const auto& [music_id, music_path] : music_mapping_it->items()) {
        ASSERT_TRUE(music_path.is_string()) << music_id;
        asset_registry.registerMusic(AudioCueCatalog::hashId(music_id), music_path.get<std::string>());
        EXPECT_TRUE(std::filesystem::exists(project_root / music_path.get<std::string>())) << music_path.get<std::string>();
    }
    std::string audio_reference_error{};
    EXPECT_TRUE(audio_cue_catalog.validateReferences(asset_registry, audio_reference_error)) << audio_reference_error;
    const auto* battle_music = audio_cue_catalog.defaultMusicCue(SceneAudioContext::Battle);
    ASSERT_NE(battle_music, nullptr);
    EXPECT_EQ(battle_music->music_id_, "music.battle.boss_2");

    const auto* alex = catalog.findActor("actor.player");
    ASSERT_NE(alex, nullptr);
    ASSERT_EQ(alex->skill_ids_.size(), 2U);
    EXPECT_EQ(alex->skill_ids_[0], "skill.attack");
    EXPECT_EQ(alex->skill_ids_[1], "skill.bash");

    const auto* attack = catalog.findSkill("skill.attack");
    ASSERT_NE(attack, nullptr);
    EXPECT_EQ(attack->presentation_.target_vfx_id_, "battle.hit_physical");
    EXPECT_GT(attack->presentation_.target_vfx_scale_, 0.0F);
    EXPECT_TRUE(std::isfinite(attack->presentation_.target_vfx_offset_.x));
    EXPECT_TRUE(std::isfinite(attack->presentation_.target_vfx_offset_.y));
    EXPECT_GT(attack->presentation_.impact_time_seconds_, 0.0F);
    EXPECT_GE(attack->presentation_.duration_seconds_,
              attack->presentation_.impact_time_seconds_ + attack->presentation_.target_vfx_tail_seconds_);
    const auto* attack_hit_path = vfx_catalog.findEffectPath(attack->presentation_.target_vfx_id_hash_);
    ASSERT_NE(attack_hit_path, nullptr);
    EXPECT_EQ(*attack_hit_path, "assets/vfx/effects/HitEffect.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *attack_hit_path));
    expectSkillTargetSfx(*sound_mapping_it, project_root, *attack, "sfx.battle.physical_hit", "assets/audio/Damage1.ogg");

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
    EXPECT_EQ(fire->presentation_.target_vfx_id_, "battle.fire_one_1");
    expectSkillTargetSfx(*sound_mapping_it, project_root, *fire, "sfx.battle.fire_1", "assets/audio/Fire1.ogg");
    const auto* fire_path = vfx_catalog.findEffectPath(fire->presentation_.target_vfx_id_hash_);
    ASSERT_NE(fire_path, nullptr);
    EXPECT_EQ(*fire_path, "assets/vfx/effects/FireOne1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *fire_path));

    const auto* thunder = catalog.findSkill("skill.thunder_1");
    ASSERT_NE(thunder, nullptr);
    EXPECT_EQ(thunder->presentation_.target_vfx_id_, "battle.thunder_one_1");
    expectSkillTargetSfx(*sound_mapping_it, project_root, *thunder, "sfx.battle.thunder_1", "assets/audio/Thunder1.ogg");
    const auto* thunder_path = vfx_catalog.findEffectPath(thunder->presentation_.target_vfx_id_hash_);
    ASSERT_NE(thunder_path, nullptr);
    EXPECT_EQ(*thunder_path, "assets/vfx/effects/ThunderOne1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *thunder_path));

    const auto* heal = catalog.findSkill("skill.heal_1");
    ASSERT_NE(heal, nullptr);
    EXPECT_EQ(heal->presentation_.target_vfx_id_, "battle.heal_all_1");
    expectSkillTargetSfx(*sound_mapping_it, project_root, *heal, "sfx.battle.heal_1", "assets/audio/Heal1.ogg");
    const auto* heal_path = vfx_catalog.findEffectPath(heal->presentation_.target_vfx_id_hash_);
    ASSERT_NE(heal_path, nullptr);
    EXPECT_EQ(*heal_path, "assets/vfx/effects/HealAll1.efkefc");
    EXPECT_TRUE(std::filesystem::exists(project_root / *heal_path));

    const entt::id_type physical_hit_id = entt::hashed_string{"battle.hit_physical"}.value();
    EXPECT_EQ(attack->presentation_.target_vfx_id_hash_, physical_hit_id);
}

} // namespace
} // namespace game::data
