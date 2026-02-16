// NOLINTBEGIN
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/ui/ui_preset_manager.h"

namespace {

[[nodiscard]] std::filesystem::path makeTempJsonPath(std::string_view prefix) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return temp_dir / (std::string(prefix) + std::to_string(timestamp) + ".json");
}

} // namespace

TEST(UIPresetManagerInvalidInputTest, LoadButtonPresetsDoesNotThrowOnInvalidNumbers) {
    const auto config_path = makeTempJsonPath("ui_preset_manager_invalid_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "test_button": {
            "images": {
                "normal": {
                    "path": "assets/ui/nonexistent.png",
                    "source": ["x", "y", "w", "h"]
                }
            }
        }
    })";
    config_file.close();

    engine::ui::UIPresetManager manager;
    EXPECT_NO_THROW({
        (void)manager.loadButtonPresets(config_path.string());
    });

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

TEST(UIPresetManagerInvalidInputTest, LoadImagePresetsRejectsAtlasAlias) {
    const auto config_path = makeTempJsonPath("ui_image_presets_atlas_alias_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "test_image": {
            "atlas": "assets/ui/nonexistent.png",
            "source": [0, 0, 16, 16]
        }
    })";
    config_file.close();

    engine::ui::UIPresetManager manager;
    EXPECT_FALSE(manager.loadImagePresets(config_path.string()));
    EXPECT_TRUE(manager.listImagePresetIds().empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

TEST(UIPresetManagerInvalidInputTest, LoadImagePresetsRejectsTextureAlias) {
    const auto config_path = makeTempJsonPath("ui_image_presets_texture_alias_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "test_image": {
            "texture": "assets/ui/nonexistent.png",
            "source": [0, 0, 16, 16]
        }
    })";
    config_file.close();

    engine::ui::UIPresetManager manager;
    EXPECT_FALSE(manager.loadImagePresets(config_path.string()));
    EXPECT_TRUE(manager.listImagePresetIds().empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

TEST(UIPresetManagerInvalidInputTest, LoadButtonPresetsRejectsAtlasAlias) {
    const auto config_path = makeTempJsonPath("ui_button_presets_atlas_alias_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "test_button": {
            "images": {
                "normal": {
                    "atlas": "assets/ui/nonexistent.png",
                    "source": [0, 0, 16, 16]
                }
            }
        }
    })";
    config_file.close();

    engine::ui::UIPresetManager manager;
    EXPECT_FALSE(manager.loadButtonPresets(config_path.string()));
    EXPECT_TRUE(manager.listButtonPresetIds().empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

TEST(UIPresetManagerInvalidInputTest, LoadButtonPresetsRejectsTextureAlias) {
    const auto config_path = makeTempJsonPath("ui_button_presets_texture_alias_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "test_button": {
            "images": {
                "normal": {
                    "texture": "assets/ui/nonexistent.png",
                    "source": [0, 0, 16, 16]
                }
            }
        }
    })";
    config_file.close();

    engine::ui::UIPresetManager manager;
    EXPECT_FALSE(manager.loadButtonPresets(config_path.string()));
    EXPECT_TRUE(manager.listButtonPresetIds().empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}
// NOLINTEND
