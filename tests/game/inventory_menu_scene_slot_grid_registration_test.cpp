// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

TEST(InventoryMenuSceneSlotGridRegistrationTest, SharedSlotVectorTypeIsRegisteredOnlyOnce) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.cpp").lexically_normal();
    const std::filesystem::path tab_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/inventory_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(tab_source_path)) << tab_source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string tab_source = test_source_utils::readTextFile(tab_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;
    ASSERT_FALSE(tab_source.empty()) << "无法读取: " << tab_source_path;

    const std::string init_ui_block =
        test_source_utils::extractFunctionBlock(source, "bool InventoryMenuScene::initUI()");
    const std::string bind_model_block =
        test_source_utils::extractFunctionBlock(tab_source, "bool InventoryTabContent::bindModel(Rml::DataModelConstructor& constructor)");
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(bind_model_block.empty());

    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "RegisterArray<SlotGridViewModels>()"), 1U)
        << "InventoryMenuScene should register the shared SlotGrid vector array type exactly once.";
    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "RegisterArray<decltype(hotbar_slots_)>()"), 0U)
        << "Hotbar slots reuse the same vector<SlotGridViewModel> array type and should not register it again.";
    EXPECT_NE(bind_model_block.find("Bind(\"backpack_slots\", &backpack_slots_)"), std::string::npos);
    EXPECT_NE(bind_model_block.find("Bind(\"hotbar_slots\", &hotbar_slots_)"), std::string::npos);
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, SceneUsesDocumentControllerForUiLifecycle) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("RmlDocumentController document_controller_"), std::string::npos);
    EXPECT_EQ(header.find("RmlDataBridge"), std::string::npos);
    EXPECT_NE(header.find("std::unordered_map<game::ui::MenuTabId"), std::string::npos);

    const std::string init_ui_block =
        test_source_utils::extractFunctionBlock(source, "bool InventoryMenuScene::initUI()");
    const std::string shutdown_ui_block =
        test_source_utils::extractFunctionBlock(source, "void InventoryMenuScene::shutdownUI()");
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(shutdown_ui_block.empty());

    EXPECT_NE(init_ui_block.find("document_controller_.attach(runtime, instanceId())"), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.createModel(MODEL_NAME, &type_register_)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("InventoryTabContent"), std::string::npos);
    EXPECT_NE(init_ui_block.find("Bind(\"active_tab_id\", &active_tab_id_bind_)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.markAllDirty()"), std::string::npos);
    EXPECT_EQ(init_ui_block.find("document_controller_.queueFocusFirstEnabledElementByClass(\"hb-slot\")"),
              std::string::npos);
    EXPECT_NE(shutdown_ui_block.find("document_controller_.unload();"), std::string::npos);
    EXPECT_EQ(source.find("loadRmlDocument("), std::string::npos);
    EXPECT_EQ(source.find("unloadAllRmlDocuments("), std::string::npos);
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, InventoryMenuRmlUsesNavigationRootAndDeclarativeTabState) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string source = test_source_utils::readTextFile(rml_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << rml_path;

    EXPECT_NE(source.find("tf-screen-root tf-nav-root"), std::string::npos);
    EXPECT_NE(source.find("data-class-tab-active=\"active_tab_id == 0\""), std::string::npos);
    EXPECT_NE(source.find("data-event-click=\"switch_tab(0)\""), std::string::npos);
    EXPECT_NE(source.find("data-if=\"active_tab_id == 0\""), std::string::npos);
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, ClosingActionMenuDoesNotClearEntriesInSameUiTick) {
    const std::filesystem::path tab_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/inventory_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(tab_source_path)) << tab_source_path;

    const std::string source = test_source_utils::readTextFile(tab_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << tab_source_path;

    const std::string close_action_menu_block =
        test_source_utils::extractFunctionBlock(source, "void InventoryTabContent::closeActionMenu(bool /*restore_focus*/)");
    ASSERT_FALSE(close_action_menu_block.empty());

    EXPECT_NE(close_action_menu_block.find("action_menu_visible_ = false;"), std::string::npos);
    EXPECT_EQ(close_action_menu_block.find("action_menu_entries_.clear();"), std::string::npos)
        << "Closing the menu should hide the data-if subtree first; clearing the backing array in the same tick "
           "can make stale data-for bindings read action_menu_entries[n] and trigger RmlUi out-of-bounds warnings.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
