// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(UILayoutSourceTest, ProgressBarOnLayoutNoLongerMutatesFillAnchorDirectly) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_progress_bar.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const auto on_layout_pos = source.find("void UIProgressBar::onLayout()");
    ASSERT_NE(on_layout_pos, std::string::npos)
        << "UIProgressBar::onLayout should exist.";
    EXPECT_EQ(source.find("updateFillVisual();", on_layout_pos), std::string::npos)
        << "UIL-023 contract: onLayout should not call updateFillVisual to avoid layout-time anchor mutation.";
}

TEST(UILayoutSourceTest, ProgressBarFillVisualUsesAnchorDiffGuardBeforeWrite) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_progress_bar.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("fill_image_->getAnchorMin()"), std::string::npos)
        << "updateFillVisual should read current min anchor before writing.";
    EXPECT_NE(source.find("fill_image_->getAnchorMax()"), std::string::npos)
        << "updateFillVisual should read current max anchor before writing.";
    EXPECT_NE(source.find("fill_image_->setAnchor(anchor_min, anchor_max);"), std::string::npos)
        << "updateFillVisual should write anchor only after diff guard.";
}

TEST(UILayoutSourceTest, ProgressBarShowLabelInvalidatesLayoutOnVisibilityChange) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_progress_bar.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("if (label_->isVisible() != show)"), std::string::npos)
        << "showLabel should skip redundant visibility writes.";
    EXPECT_NE(source.find("invalidateLayout();"), std::string::npos)
        << "showLabel should invalidate layout when visibility changes.";
}

TEST(UILayoutSourceTest, ItemSlotCountLabelFallbackUsesLocalOrigin) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_item_slot.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("glm::vec2 pos{0.0f, 0.0f};"), std::string::npos)
        << "UIItemSlot count label fallback should use local origin, not parent-space position.";
}

TEST(UILayoutSourceTest, DragPreviewOnLayoutUsesResolvedLayoutSize) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_drag_preview.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("count_label_->getLayoutSize()"), std::string::npos)
        << "UIDragPreview should align count label using layout size.";
    EXPECT_NE(source.find("glm::vec2 pos = getLayoutSize();"), std::string::npos)
        << "UIDragPreview should compute anchor position from resolved layout size.";
}

} // namespace
} // namespace engine::ui
// NOLINTEND
