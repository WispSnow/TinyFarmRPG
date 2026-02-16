// NOLINTBEGIN
#include <gtest/gtest.h>

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/render/image.h"
#include "engine/ui/ui_preset_manager.h"

namespace engine::ui {
namespace {

engine::render::Image makeTestImage(std::string_view path) {
    return engine::render::Image(path, engine::utils::Rect{{0.0f, 0.0f}, {16.0f, 16.0f}}, false);
}

[[nodiscard]] std::filesystem::path makeTempJsonPath(std::string_view prefix) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return temp_dir / (std::string(prefix) + std::to_string(timestamp) + ".json");
}

} // namespace

TEST(UIPresetManagerDebugApiTest, ListButtonPresetIdsSortedAndMutableAccessWorks) {
    UIPresetManager manager;

    const entt::id_type alpha_id = entt::hashed_string{"alpha"}.value();
    const entt::id_type beta_id = entt::hashed_string{"beta"}.value();

    UIButtonSkin alpha{};
    alpha.normal_image = makeTestImage("assets/ui/alpha.png");
    UIButtonLabelStyle alpha_label{};
    alpha_label.text = "Alpha";
    alpha_label.font_path = "assets/fonts/VonwaonBitmap-16px.ttf";
    alpha_label.font_size = 16;
    alpha_label.color = engine::utils::FColor{1.0f, 1.0f, 1.0f, 1.0f};
    alpha_label.offset = {0.0f, 0.0f};
    alpha.normal_label = alpha_label;

    UIButtonSkin beta{};
    beta.normal_image = makeTestImage("assets/ui/beta.png");
    UIButtonLabelStyle beta_label{};
    beta_label.text = "Beta";
    beta_label.font_path = "assets/fonts/VonwaonBitmap-16px.ttf";
    beta_label.font_size = 16;
    beta_label.color = engine::utils::FColor{0.0f, 1.0f, 0.0f, 1.0f};
    beta_label.offset = {1.0f, 2.0f};
    beta.normal_label = beta_label;

    ASSERT_TRUE(manager.registerButtonPreset(beta_id, beta));
    ASSERT_TRUE(manager.registerButtonPreset(alpha_id, alpha));

    const auto ids = manager.listButtonPresetIds();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));

    auto* mutable_beta = manager.getButtonPresetMutable(beta_id);
    ASSERT_NE(mutable_beta, nullptr);
    ASSERT_TRUE(mutable_beta->normal_label.has_value());
    mutable_beta->normal_label->color = engine::utils::FColor{0.25f, 0.5f, 0.75f, 1.0f};

    const auto* updated_beta = manager.getButtonPreset(beta_id);
    ASSERT_NE(updated_beta, nullptr);
    ASSERT_TRUE(updated_beta->normal_label.has_value());
    EXPECT_FLOAT_EQ(updated_beta->normal_label->color.r, 0.25f);
    EXPECT_FLOAT_EQ(updated_beta->normal_label->color.g, 0.5f);
    EXPECT_FLOAT_EQ(updated_beta->normal_label->color.b, 0.75f);
    EXPECT_FLOAT_EQ(updated_beta->normal_label->color.a, 1.0f);
}

TEST(UIPresetManagerDebugApiTest, RegisterButtonPresetPropagatesNineSliceMarginsToAllImages) {
    UIPresetManager manager;

    UIButtonSkin skin{};
    skin.normal_image = makeTestImage("assets/ui/normal.png");
    skin.hover_image = makeTestImage("assets/ui/hover.png");
    skin.pressed_image = makeTestImage("assets/ui/pressed.png");
    skin.disabled_image = makeTestImage("assets/ui/disabled.png");

    engine::render::NineSliceMargins margins{};
    margins.left = 1.0f;
    margins.top = 2.0f;
    margins.right = 3.0f;
    margins.bottom = 4.0f;
    skin.nine_slice_margins = margins;

    const entt::id_type preset_id = entt::hashed_string{"nine_slice"}.value();
    ASSERT_TRUE(manager.registerButtonPreset(preset_id, std::move(skin)));

    const auto* preset = manager.getButtonPreset(preset_id);
    ASSERT_NE(preset, nullptr);
    ASSERT_TRUE(preset->normal_image.has_value());
    ASSERT_TRUE(preset->hover_image.has_value());
    ASSERT_TRUE(preset->pressed_image.has_value());
    ASSERT_TRUE(preset->disabled_image.has_value());

    const auto& normal_margins = preset->normal_image->getNineSliceMargins();
    const auto& hover_margins = preset->hover_image->getNineSliceMargins();
    const auto& pressed_margins = preset->pressed_image->getNineSliceMargins();
    const auto& disabled_margins = preset->disabled_image->getNineSliceMargins();

    ASSERT_TRUE(normal_margins.has_value());
    ASSERT_TRUE(hover_margins.has_value());
    ASSERT_TRUE(pressed_margins.has_value());
    ASSERT_TRUE(disabled_margins.has_value());

    EXPECT_FLOAT_EQ(normal_margins->left, 1.0f);
    EXPECT_FLOAT_EQ(normal_margins->top, 2.0f);
    EXPECT_FLOAT_EQ(normal_margins->right, 3.0f);
    EXPECT_FLOAT_EQ(normal_margins->bottom, 4.0f);

    EXPECT_FLOAT_EQ(hover_margins->left, 1.0f);
    EXPECT_FLOAT_EQ(hover_margins->top, 2.0f);
    EXPECT_FLOAT_EQ(hover_margins->right, 3.0f);
    EXPECT_FLOAT_EQ(hover_margins->bottom, 4.0f);

    EXPECT_FLOAT_EQ(pressed_margins->left, 1.0f);
    EXPECT_FLOAT_EQ(pressed_margins->top, 2.0f);
    EXPECT_FLOAT_EQ(pressed_margins->right, 3.0f);
    EXPECT_FLOAT_EQ(pressed_margins->bottom, 4.0f);

    EXPECT_FLOAT_EQ(disabled_margins->left, 1.0f);
    EXPECT_FLOAT_EQ(disabled_margins->top, 2.0f);
    EXPECT_FLOAT_EQ(disabled_margins->right, 3.0f);
    EXPECT_FLOAT_EQ(disabled_margins->bottom, 4.0f);
}

TEST(UIPresetManagerDebugApiTest, ListImagePresetIdsSortedAndMutableAccessWorks) {
    UIPresetManager manager;

    const entt::id_type first_id = entt::hashed_string{"img_first"}.value();
    const entt::id_type second_id = entt::hashed_string{"img_second"}.value();

    ASSERT_TRUE(manager.registerImagePreset(second_id, makeTestImage("assets/ui/second.png")));
    ASSERT_TRUE(manager.registerImagePreset(first_id, makeTestImage("assets/ui/first.png")));

    const auto ids = manager.listImagePresetIds();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));

    auto* mutable_first = manager.getImagePresetMutable(first_id);
    ASSERT_NE(mutable_first, nullptr);

    const engine::utils::Rect rect{{3.0f, 4.0f}, {5.0f, 6.0f}};
    mutable_first->setSourceRect(rect);

    const auto* updated_first = manager.getImagePreset(first_id);
    ASSERT_NE(updated_first, nullptr);
    EXPECT_FLOAT_EQ(updated_first->getSourceRect().pos.x, 3.0f);
    EXPECT_FLOAT_EQ(updated_first->getSourceRect().pos.y, 4.0f);
    EXPECT_FLOAT_EQ(updated_first->getSourceRect().size.x, 5.0f);
    EXPECT_FLOAT_EQ(updated_first->getSourceRect().size.y, 6.0f);
}

TEST(UIPresetManagerDebugApiTest, LoadButtonPresetsStoresKeyMappingForDebug) {
    const auto config_path = makeTempJsonPath("ui_button_presets_keys_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "alpha": {
            "images": {
                "normal": {
                    "path": "assets/ui/alpha.png",
                    "source": [0, 0, 16, 16]
                }
            }
        },
        "beta": {
            "images": {
                "normal": {
                    "path": "assets/ui/beta.png",
                    "source": [0, 0, 16, 16]
                }
            }
        }
    })";
    config_file.close();

    UIPresetManager manager;
    ASSERT_TRUE(manager.loadButtonPresets(config_path.string()));

    const entt::id_type alpha_id = entt::hashed_string{"alpha"}.value();
    const entt::id_type beta_id = entt::hashed_string{"beta"}.value();

    EXPECT_EQ(manager.getButtonPresetKey(alpha_id), "alpha");
    EXPECT_EQ(manager.getButtonPresetKey(beta_id), "beta");

    const entt::id_type unknown_id = entt::hashed_string{"unknown"}.value();
    EXPECT_TRUE(manager.getButtonPresetKey(unknown_id).empty());

    manager.clearButtonPresets();
    EXPECT_TRUE(manager.getButtonPresetKey(alpha_id).empty());
    EXPECT_TRUE(manager.getButtonPresetKey(beta_id).empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

TEST(UIPresetManagerDebugApiTest, LoadImagePresetsStoresKeyMappingForDebug) {
    const auto config_path = makeTempJsonPath("ui_image_presets_keys_");
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({
        "icon_a": {
            "path": "assets/ui/icon_a.png",
            "source": [0, 0, 16, 16]
        },
        "icon_b": {
            "path": "assets/ui/icon_b.png",
            "source": [0, 0, 16, 16]
        }
    })";
    config_file.close();

    UIPresetManager manager;
    ASSERT_TRUE(manager.loadImagePresets(config_path.string()));

    const entt::id_type icon_a_id = entt::hashed_string{"icon_a"}.value();
    const entt::id_type icon_b_id = entt::hashed_string{"icon_b"}.value();

    EXPECT_EQ(manager.getImagePresetKey(icon_a_id), "icon_a");
    EXPECT_EQ(manager.getImagePresetKey(icon_b_id), "icon_b");

    const entt::id_type unknown_id = entt::hashed_string{"unknown_icon"}.value();
    EXPECT_TRUE(manager.getImagePresetKey(unknown_id).empty());

    manager.clearImagePresets();
    EXPECT_TRUE(manager.getImagePresetKey(icon_a_id).empty());
    EXPECT_TRUE(manager.getImagePresetKey(icon_b_id).empty());

    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);
}

} // namespace engine::ui
// NOLINTEND
