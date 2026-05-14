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
    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "registerQuestTabDataTypes(constructor)"), 1U)
        << "Quest tab data types should also be registered once in the scene-level guarded block.";
    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "registerMapTabDataTypes(constructor)"), 1U)
        << "Map tab data types should also be registered once in the scene-level guarded block.";
    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "RegisterArray<decltype(hotbar_slots_)>()"), 0U)
        << "Hotbar slots reuse the same vector<SlotGridViewModel> array type and should not register it again.";
    EXPECT_NE(bind_model_block.find("Bind(\"backpack_slots\", &backpack_slots_)"), std::string::npos);
    EXPECT_NE(bind_model_block.find("Bind(\"hotbar_slots\", &hotbar_slots_)"), std::string::npos);

    const std::filesystem::path quest_tab_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/quest_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(quest_tab_source_path)) << quest_tab_source_path;
    const std::string quest_tab_source = test_source_utils::readTextFile(quest_tab_source_path);
    ASSERT_FALSE(quest_tab_source.empty()) << "无法读取: " << quest_tab_source_path;
    const std::string quest_bind_model_block =
        test_source_utils::extractFunctionBlock(quest_tab_source, "bool QuestTabContent::bindModel(Rml::DataModelConstructor& constructor)");
    ASSERT_FALSE(quest_bind_model_block.empty());
    EXPECT_EQ(quest_bind_model_block.find("RegisterStruct<QuestEntryViewModel>()"), std::string::npos);
    EXPECT_EQ(quest_bind_model_block.find("RegisterArray<QuestEntryViewModels>()"), std::string::npos);
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
    EXPECT_NE(init_ui_block.find("QuestTabContent"), std::string::npos);
    EXPECT_EQ(init_ui_block.find("Bind(\"active_tab_id\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("PlaceholderTabContent"), std::string::npos);
    EXPECT_EQ(init_ui_block.find("tabs_.emplace(game::ui::MenuTabId::Quests, std::make_unique<PlaceholderTabContent>())"),
              std::string::npos);
    EXPECT_NE(init_ui_block.find("switchTabFromTabsetIndex"), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.markAllDirty()"), std::string::npos);
    EXPECT_EQ(init_ui_block.find("document_controller_.queueFocusFirstEnabledElementByClass(\"hb-slot\")"),
              std::string::npos);
    EXPECT_NE(shutdown_ui_block.find("document_controller_.unload();"), std::string::npos);
    EXPECT_EQ(source.find("loadRmlDocument("), std::string::npos);
    EXPECT_EQ(source.find("unloadAllRmlDocuments("), std::string::npos);
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, InventoryMenuRmlUsesNavigationRootAndNativeTabset) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rml").lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rcss").lexically_normal();
    const std::filesystem::path theme_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/spritesheet.rcss").lexically_normal();
    const std::filesystem::path equipment_tab_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/equipment_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;
    ASSERT_TRUE(std::filesystem::exists(theme_path)) << theme_path;
    ASSERT_TRUE(std::filesystem::exists(equipment_tab_source_path)) << equipment_tab_source_path;

    const std::string source = test_source_utils::readTextFile(rml_path);
    const std::string style = test_source_utils::readTextFile(rcss_path);
    const std::string theme = test_source_utils::readTextFile(theme_path);
    const std::string tab_source = test_source_utils::readTextFile(equipment_tab_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << rml_path;
    ASSERT_FALSE(style.empty()) << "无法读取: " << rcss_path;
    ASSERT_FALSE(theme.empty()) << "无法读取: " << theme_path;
    ASSERT_FALSE(tab_source.empty()) << "无法读取: " << equipment_tab_source_path;

    EXPECT_NE(source.find("tf-screen-root tf-nav-root"), std::string::npos);
    EXPECT_NE(source.find("<tabset id=\"menu-tabset\" data-event-tabchange=\"switch_tab(ev.tab_index)\""),
              std::string::npos);
    EXPECT_NE(source.find("<panel id=\"panel-inventory\""), std::string::npos);
    EXPECT_NE(source.find("<panel id=\"panel-equipment\""), std::string::npos);
    EXPECT_NE(source.find("<panel id=\"panel-quests\""), std::string::npos);
    EXPECT_NE(source.find("data-for=\"quest : active_quest_entries\""), std::string::npos);
    EXPECT_NE(source.find("data-for=\"quest : completed_quest_entries\""), std::string::npos);
    EXPECT_NE(source.find("data-if=\"!has_active_quests\""), std::string::npos);
    EXPECT_NE(source.find("data-if=\"!has_completed_quests\""), std::string::npos);
    EXPECT_NE(source.find("<panel id=\"panel-map\""), std::string::npos);
    EXPECT_NE(source.find("id=\"map-content\""), std::string::npos);
    EXPECT_NE(source.find("data-attr-src=\"map_preview_src\""), std::string::npos);
    EXPECT_NE(source.find("data-style-left=\"map_preview_left\""), std::string::npos);
    EXPECT_NE(source.find("data-style-left=\"player_marker_left\""), std::string::npos);
    EXPECT_NE(source.find(">No map data</div>"), std::string::npos);
    EXPECT_NE(source.find("<panel id=\"panel-options\""), std::string::npos);
    EXPECT_EQ(source.find("data-class-tab-active"), std::string::npos);
    EXPECT_EQ(source.find("active_tab_id =="), std::string::npos);
    EXPECT_EQ(source.find("class=\"menu-separator\""), std::string::npos);
    EXPECT_EQ(source.find(">Sort</button>"), std::string::npos);
    EXPECT_EQ(source.find(">Unequip</button>"), std::string::npos);
    EXPECT_NE(source.find("equipment-slot-placeholder"), std::string::npos);
    EXPECT_NE(source.find("data-if=\"!eqslot.has_item\""), std::string::npos);
    EXPECT_NE(source.find("data-style-decorator=\"eqslot.placeholder_decorator\""), std::string::npos);
    EXPECT_NE(style.find("sort-icon:     368px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(style.find("sort-icon-pressed: 368px 48px 16px 16px;"), std::string::npos);
    EXPECT_NE(style.find("menu-equipment-slot-bg:        71px 41px 18px 18px;"), std::string::npos);
    EXPECT_NE(style.find("menu-equipment-slot-bg-inner:  74px 44px 12px 12px;"), std::string::npos);
    EXPECT_NE(style.find("equipment-slot-weapon-hint:    106px 41px 14px 15px;"), std::string::npos);
    EXPECT_NE(style.find("equipment-slot-head-hint:       11px 41px 12px 12px;"), std::string::npos);
    EXPECT_NE(style.find("equipment-slot-body-hint:       41px 42px 16px 11px;"), std::string::npos);
    EXPECT_NE(style.find("equipment-slot-boot-hint:      137px 41px 14px 14px;"), std::string::npos);
    EXPECT_NE(style.find("equipment-slot-accessory-hint: 170px 42px 12px 11px;"), std::string::npos);
    EXPECT_NE(style.find("opacity: 0.3;"), std::string::npos);
    EXPECT_NE(style.find("left: 2dp;\n    top: 2dp;\n    width: 24dp;\n    height: 24dp;"), std::string::npos);
    EXPECT_NE(style.find("unequip-icon:  272px 64px 16px 16px;"), std::string::npos);
    EXPECT_NE(style.find("unequip-icon-pressed: 272px 80px 16px 16px;"), std::string::npos);
    EXPECT_NE(source.find("data-event-click=\"equipment_candidate_click(candidate.inventory_slot_index)\""),
              std::string::npos);
    EXPECT_NE(source.find("{{ candidate.delta_text }}"), std::string::npos);
    EXPECT_NE(style.find("#equipment-candidate-panel scrollbarvertical"), std::string::npos);
    EXPECT_NE(style.find("overflow-x: hidden;"), std::string::npos);
    EXPECT_NE(style.find("width: 4dp;\n    scrollbar-margin: 0dp;"), std::string::npos);
    EXPECT_NE(style.find("height: 108dp;\n    margin-top: 6dp;\n    overflow-y: auto;"), std::string::npos);
    EXPECT_NE(style.find("width: 206dp;\n    min-height: 22dp;"), std::string::npos);
    EXPECT_NE(style.find("display: flex;\n    flex-direction: row;\n    align-items: center;\n    justify-content: space-between;\n    margin-left: 6dp;\n    width: 182dp;\n    gap: 4dp;"),
              std::string::npos);
    EXPECT_NE(style.find("width: 82dp;\n    font-size: 10dp;"), std::string::npos);
    EXPECT_NE(style.find("width: 96dp;\n    font-size: 9dp;"), std::string::npos);
    EXPECT_NE(style.find("font-size: 10dp;\n    color: #ffffff;\n    text-align: left;"), std::string::npos);
    EXPECT_NE(style.find("font-size: 9dp;\n    color: #9ece6a;\n    text-align: right;\n    white-space: normal;\n    word-break: normal;"),
              std::string::npos);
    EXPECT_NE(style.find("#map-preview-frame"), std::string::npos);
    EXPECT_NE(style.find("width: 218dp;\n    height: 126dp;"), std::string::npos);
    EXPECT_NE(style.find("#map-player-marker"), std::string::npos);
    EXPECT_NE(style.find("background-color: #f7768e;"), std::string::npos);
    EXPECT_NE(tab_source.find("fmt::format(\"{}{}{}\""), std::string::npos);
    EXPECT_EQ(tab_source.find("RegisterArray<EquipmentCandidateDeltaViewModels>()"), std::string::npos);
    EXPECT_EQ(tab_source.find("handle.RegisterMember(\"param_deltas\""), std::string::npos);
    EXPECT_EQ(tab_source.find("\\u00A0"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-sword:     192px  0px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-staff:     176px 16px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-helmet:    160px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-armor:     176px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-boots:     208px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-wooden-accessory: 288px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-sword:     192px  0px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-staff:     176px 16px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-helmet:    160px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-armor:     176px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-boots:     208px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(theme.find("item-equipment-iron-accessory: 288px 32px 16px 16px;"), std::string::npos);
    EXPECT_NE(style.find("menu-party-card-bg:       67px  3px 42px 42px;"), std::string::npos);
    EXPECT_NE(style.find("menu-party-card-bg-inner: 76px 12px 24px 24px;"), std::string::npos);

    const auto footer_block = style.find("#menu-footer");
    ASSERT_NE(footer_block, std::string::npos);
    const auto footer_block_end = style.find('}', footer_block);
    ASSERT_NE(footer_block_end, std::string::npos);
    const auto footer_width = style.find("width: 218dp;", footer_block);
    ASSERT_NE(footer_width, std::string::npos);
    EXPECT_LT(footer_width, footer_block_end);

    const auto inventory_panel = source.find("<panel id=\"panel-inventory\"");
    const auto equipment_tab = source.find("<tab class=\"tab-icon tf-nav-auto tf-focus-ring-gold\" id=\"tab-equipment\"");
    const auto footer = source.find("id=\"menu-footer\"");
    ASSERT_NE(inventory_panel, std::string::npos);
    ASSERT_NE(equipment_tab, std::string::npos);
    ASSERT_NE(footer, std::string::npos);
    EXPECT_LT(inventory_panel, footer);
    EXPECT_LT(footer, equipment_tab) << "Gold footer should belong only to the inventory tab.";

    const auto equipment_content = source.find("id=\"equipment-content\"");
    const auto equipment_detail = source.find("id=\"equipment-detail\"");
    ASSERT_NE(equipment_content, std::string::npos);
    ASSERT_NE(equipment_detail, std::string::npos);
    EXPECT_LT(equipment_content, equipment_detail)
        << "Equipment detail should live in the bottom footer area after the expanded candidate region.";

    const auto tabset_end = source.find("</tabset>");
    const auto party_col = source.find("id=\"party-col\"");
    ASSERT_NE(tabset_end, std::string::npos);
    ASSERT_NE(party_col, std::string::npos);
    EXPECT_LT(tabset_end, party_col) << "party-col must stay outside tabset so it remains visible for every tab.";
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, InventoryMenuSceneAndGameScenePassCatalogsAndWorldStateIntoTabs) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.cpp").lexically_normal();
    const std::filesystem::path game_scene_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(game_scene_path)) << game_scene_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string game_scene_source = test_source_utils::readTextFile(game_scene_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;
    ASSERT_FALSE(game_scene_source.empty()) << "无法读取: " << game_scene_path;

    EXPECT_NE(header.find("const game::data::QuestCatalog* quest_catalog_{nullptr};"), std::string::npos);
    EXPECT_NE(header.find("const game::world::WorldState* world_state_{nullptr};"), std::string::npos);
    EXPECT_NE(header.find("const game::data::QuestCatalog* quest_catalog"), std::string::npos);
    EXPECT_NE(header.find("const game::world::WorldState* world_state"), std::string::npos);
    EXPECT_NE(source.find("quest_catalog_(quest_catalog)"), std::string::npos);
    EXPECT_NE(source.find("world_state_(world_state)"), std::string::npos);

    const std::string inventory_toggle_block =
        test_source_utils::extractFunctionBlock(game_scene_source, "bool GameScene::onInventoryToggle()");
    ASSERT_FALSE(inventory_toggle_block.empty());
    EXPECT_NE(inventory_toggle_block.find("services_->quest_catalog.get()"), std::string::npos);
    EXPECT_NE(inventory_toggle_block.find("services_->world_state.get()"), std::string::npos);
}

TEST(InventoryMenuSceneSlotGridRegistrationTest, ClosingActionMenuDoesNotClearEntriesInSameUiTick) {
    const std::filesystem::path tab_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/inventory_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(tab_source_path)) << tab_source_path;

    const std::string source = test_source_utils::readTextFile(tab_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << tab_source_path;

    const std::string close_action_menu_block =
        test_source_utils::extractFunctionBlock(source, "void InventoryTabContent::closeActionMenu()");
    ASSERT_FALSE(close_action_menu_block.empty());

    EXPECT_NE(close_action_menu_block.find("action_menu_visible_ = false;"), std::string::npos);
    EXPECT_EQ(close_action_menu_block.find("action_menu_entries_.clear();"), std::string::npos)
        << "Closing the menu should hide the data-if subtree first; clearing the backing array in the same tick "
           "can make stale data-for bindings read action_menu_entries[n] and trigger RmlUi out-of-bounds warnings.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
