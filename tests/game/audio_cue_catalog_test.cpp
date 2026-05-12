#include <gtest/gtest.h>

#include "engine/resource/asset_registry.h"
#include "game/data/audio_cue_catalog.h"
#include "../shared/test_file_utils.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace game::data {
namespace {

[[nodiscard]] std::filesystem::path writeAudioCueFixture(std::string_view body) {
    const auto root = test::utils::createUniqueTempDir("tinyfarm_audio_cue_catalog_test");
    const auto path = root / "audio_cues.json";
    test::utils::writeTextFile(path, body);
    return path;
}

TEST(AudioCueCatalogTest, LoadFromFileParsesMusicCuesAndDefaults) {
    const auto path = writeAudioCueFixture(R"json({
  "schema_version": 1,
  "music_cues": {
    "cue.music.battle.default": {
      "music_id": "music.battle.boss_2",
      "loop": true,
      "fade_in_ms": 250,
      "volume_scale": 0.8
    },
    "cue.music.gameplay.default": {
      "music_id": "scene-bg-music",
      "loop": true,
      "fade_in_ms": 200,
      "volume_scale": 1.0
    }
  },
  "sfx_cues": {},
  "scene_defaults": {
    "gameplay": "cue.music.gameplay.default",
    "battle": "cue.music.battle.default"
  }
})json");

    AudioCueCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));
    EXPECT_EQ(catalog.schemaVersion(), 1U);

    const auto* gameplay = catalog.defaultMusicCue(SceneAudioContext::Gameplay);
    ASSERT_NE(gameplay, nullptr);
    EXPECT_EQ(gameplay->music_id_, "scene-bg-music");
    EXPECT_TRUE(gameplay->loop_);
    EXPECT_EQ(gameplay->fade_in_ms_, 200);
    EXPECT_FLOAT_EQ(gameplay->volume_scale_, 1.0F);

    const auto* battle = catalog.defaultMusicCue(SceneAudioContext::Battle);
    ASSERT_NE(battle, nullptr);
    EXPECT_EQ(battle->music_id_, "music.battle.boss_2");
    EXPECT_EQ(battle->music_id_hash_, AudioCueCatalog::hashId("music.battle.boss_2"));
    EXPECT_EQ(battle->fade_in_ms_, 250);
    EXPECT_FLOAT_EQ(battle->volume_scale_, 0.8F);

    const auto cues = catalog.listMusicCues();
    ASSERT_EQ(cues.size(), 2U);
    EXPECT_EQ(cues[0]->id_, "cue.music.battle.default");
    EXPECT_EQ(cues[1]->id_, "cue.music.gameplay.default");
}

TEST(AudioCueCatalogTest, ValidateReferencesRequiresRegisteredMusicIds) {
    const auto path = writeAudioCueFixture(R"json({
  "schema_version": 1,
  "music_cues": {
    "cue.music.battle.default": { "music_id": "music.battle.boss_2" },
    "cue.music.gameplay.default": { "music_id": "scene-bg-music" }
  },
  "scene_defaults": {
    "gameplay": "cue.music.gameplay.default",
    "battle": "cue.music.battle.default"
  }
})json");

    AudioCueCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));

    engine::resource::AssetRegistry registry;
    registry.registerMusic(AudioCueCatalog::hashId("scene-bg-music"), "assets/audio/01_spring_journey.ogg");

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(registry, error));
    EXPECT_NE(error.find("music.battle.boss_2"), std::string::npos);

    registry.registerMusic(AudioCueCatalog::hashId("music.battle.boss_2"), "assets/audio/BATTLE BOSS 2.mp3");
    EXPECT_TRUE(catalog.validateReferences(registry, error)) << error;
}

TEST(AudioCueCatalogTest, LoadFromFileFailsOnInvalidDefaultsAndVolume) {
    const auto missing_default_path = writeAudioCueFixture(R"json({
  "schema_version": 1,
  "music_cues": {
    "cue.music.gameplay.default": { "music_id": "scene-bg-music" }
  },
  "scene_defaults": {
    "gameplay": "cue.music.gameplay.default",
    "battle": "cue.music.missing"
  }
})json");

    AudioCueCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(missing_default_path.string()));

    const auto bad_volume_path = writeAudioCueFixture(R"json({
  "schema_version": 1,
  "music_cues": {
    "cue.music.battle.default": { "music_id": "music.battle.boss_2", "volume_scale": 1.5 },
    "cue.music.gameplay.default": { "music_id": "scene-bg-music" }
  },
  "scene_defaults": {
    "gameplay": "cue.music.gameplay.default",
    "battle": "cue.music.battle.default"
  }
})json");

    EXPECT_FALSE(catalog.loadFromFile(bad_volume_path.string()));
}

TEST(AudioCueCatalogTest, LoadFromFileRejectsNonObjectSfxCues) {
    const auto path = writeAudioCueFixture(R"json({
  "schema_version": 1,
  "music_cues": {
    "cue.music.battle.default": { "music_id": "music.battle.boss_2" },
    "cue.music.gameplay.default": { "music_id": "scene-bg-music" }
  },
  "sfx_cues": [],
  "scene_defaults": {
    "gameplay": "cue.music.gameplay.default",
    "battle": "cue.music.battle.default"
  }
})json");

    AudioCueCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

} // namespace
} // namespace game::data
