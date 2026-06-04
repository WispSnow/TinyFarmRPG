// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

namespace game::scene {
namespace {

TEST(SaveSlotSelectSceneEnableStateTest, RefreshSlotButtonsPublishesEnabledBinding) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/save_slot_select.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string source = readTextFile(source_path);
    const std::string rml_source = readTextFile(rml_path);
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml_source.empty());

    EXPECT_NE(source.find("slot.enabled = enabled;"), std::string::npos)
        << "SaveSlotSelectScene should publish per-slot enabled state into the RmlUi data model.";
    EXPECT_NE(source.find("enabled = (mode_ == Mode::Save);"), std::string::npos)
        << "Invalid slots should remain overwritable in save mode but disabled in load mode.";
    EXPECT_NE(source.find("document_controller_.markDirty(\"slots\")"), std::string::npos)
        << "SaveSlotSelectScene should mark slots dirty after rebuilding slot view models.";
    EXPECT_NE(rml_source.find("data-class-disabled=\"!slot.enabled\""), std::string::npos)
        << "Save slot select RML should bind disabled styling to slot.enabled.";
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!slot.enabled\""), std::string::npos)
        << "Save slot select RML should bind disabled attr to slot.enabled.";
    EXPECT_NE(rml_source.find("data-event-click=\"slot_select(slot.slot_index)\""), std::string::npos)
        << "Save slot select RML should route slot clicks through the data model event.";
}

TEST(SaveSlotSelectSceneEnableStateTest, DynamicLabelsUseLocalizationKeys) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("sink<game::defs::LanguageChangedEvent>()"), std::string::npos)
        << "SaveSlotSelectScene should subscribe language changes for slot labels and confirm text.";
    EXPECT_NE(source.find("\"save_slot.status.empty\""), std::string::npos)
        << "Empty slot label should be generated from an i18n key.";
    EXPECT_NE(source.find("\"save_slot.status.day_with_timestamp\""), std::string::npos)
        << "Saved slot label with timestamp should be generated from an i18n key.";
    EXPECT_NE(source.find("\"save_slot.confirm.overwrite\""), std::string::npos)
        << "Overwrite confirmation text should be generated from an i18n key.";
}

TEST(SaveSlotSelectSceneEnableStateTest, DoesNotExposeDeleteMode) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = readTextFile(header_path);
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(header.find("Delete,"), std::string::npos);
    EXPECT_EQ(source.find("Mode::Delete"), std::string::npos);
    EXPECT_EQ(source.find("\"save_slot.confirm.delete\""), std::string::npos);
    EXPECT_EQ(source.find("Delete slot "), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
