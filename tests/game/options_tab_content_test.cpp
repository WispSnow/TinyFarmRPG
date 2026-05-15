// Source-level regression：在 options_tab_content.cpp 与 inventory_menu.rml 中
// 断言 5 项配置的绑定字段、event 名与 RML 元素都按预期存在。
// 不引入 RmlUi runtime fixture（与 shop_menu_*_flow_test 风格一致）。
//
// 当模块文件被重命名或重大重构时，本测试会失败提示更新；目的是防止"标签页内
// 部静默回归到 placeholder"。

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] std::string slurp(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << "Failed to open " << path;
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

TEST(OptionsTabContentSourceTest, CppBindsAllFiveModelFields) {
    const std::string src = slurp(projectRoot() / "src" / "game" / "ui" / "options_tab_content.cpp");
    for (const std::string_view field : {
        "options_battle_speed_text",
        "options_damage_popup_text",
        "options_enemy_hp_bar_text",
        "options_cursor_memory_text",
        "options_font_scale_text",
        "options_show_damage_popup",
        "options_show_enemy_hp_bar",
        "options_cursor_memory",
    }) {
        EXPECT_NE(src.find(field), std::string::npos)
            << "options_tab_content.cpp 应包含 binding 字段 " << field;
    }
}

TEST(OptionsTabContentSourceTest, CppBindsAllEventCallbacks) {
    const std::string src = slurp(projectRoot() / "src" / "game" / "ui" / "options_tab_content.cpp");
    for (const std::string_view event : {
        "options_battle_speed_prev",
        "options_battle_speed_next",
        "options_toggle_damage_popup",
        "options_toggle_enemy_hp_bar",
        "options_toggle_cursor_memory",
        "options_font_scale_prev",
        "options_font_scale_next",
    }) {
        EXPECT_NE(src.find(event), std::string::npos)
            << "options_tab_content.cpp 应包含 event 名 " << event;
    }
}

TEST(OptionsTabContentSourceTest, RmlExposesPanelOptionsWithFiveRows) {
    const std::string rml = slurp(projectRoot() / "ui" / "rmlui" / "scenes" / "inventory_menu.rml");
    EXPECT_NE(rml.find("panel-options"), std::string::npos);
    EXPECT_NE(rml.find("options-content"), std::string::npos);

    // 五个 binding 字段都应出现在 RML 上（来自 {{ ... }} 表达式）。
    for (const std::string_view field : {
        "options_battle_speed_text",
        "options_damage_popup_text",
        "options_enemy_hp_bar_text",
        "options_cursor_memory_text",
        "options_font_scale_text",
    }) {
        EXPECT_NE(rml.find(field), std::string::npos)
            << "inventory_menu.rml 应绑定 " << field;
    }

    // 5 项 event 的 click 绑定都应出现。
    for (const std::string_view event : {
        "options_battle_speed_prev",
        "options_battle_speed_next",
        "options_toggle_damage_popup",
        "options_toggle_enemy_hp_bar",
        "options_toggle_cursor_memory",
        "options_font_scale_prev",
        "options_font_scale_next",
    }) {
        EXPECT_NE(rml.find(event), std::string::npos)
            << "inventory_menu.rml 应触发 " << event;
    }
}

TEST(OptionsTabContentSourceTest, InventorySceneInjectsOptionsTabContent) {
    const std::string src = slurp(projectRoot() / "src" / "game" / "scene" / "inventory_menu_scene.cpp");
    EXPECT_NE(src.find("OptionsTabContent"), std::string::npos);
    EXPECT_NE(src.find("user_settings_service_"), std::string::npos);
    EXPECT_EQ(src.find("PlaceholderTabContent"), std::string::npos)
        << "Options tab 不应再使用 PlaceholderTabContent。";
}

} // namespace
