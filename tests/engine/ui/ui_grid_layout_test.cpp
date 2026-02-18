#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "engine/ui/layout/ui_grid_layout.h"

namespace engine::ui {
namespace {

constexpr float kEpsilon = 0.001F;

void expectVec2Near(const glm::vec2& actual, float expected_x, float expected_y) {
    EXPECT_NEAR(actual.x, expected_x, kEpsilon);
    EXPECT_NEAR(actual.y, expected_y, kEpsilon);
}

} // namespace

TEST(UIGridLayoutTest, FixedCellSizeAndSpacingDriveChildPositions) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{80.0F, 60.0F});
    layout->setColumnCount(2);
    layout->setSpacing({2.0F, 3.0F});
    layout->setCellSize({20.0F, 10.0F});

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{5.0F, 4.0F});
    auto second = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{7.0F, 8.0F});
    auto third = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{11.0F, 12.0F});

    UIElement* first_ptr = first.get();
    UIElement* second_ptr = second.get();
    UIElement* third_ptr = third.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(second));
    layout->addChild(std::move(third));

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), 0.0F, 0.0F);
    expectVec2Near(second_ptr->getPosition(), 22.0F, 0.0F);
    expectVec2Near(third_ptr->getPosition(), 0.0F, 13.0F);

    // UIL-010 contract: fixed cell uses layout override, requested size must stay unchanged.
    expectVec2Near(first_ptr->getRequestedSize(), 5.0F, 4.0F);
    expectVec2Near(second_ptr->getRequestedSize(), 7.0F, 8.0F);
    expectVec2Near(third_ptr->getRequestedSize(), 11.0F, 12.0F);
    expectVec2Near(first_ptr->getLayoutSize(), 20.0F, 10.0F);
    expectVec2Near(second_ptr->getLayoutSize(), 20.0F, 10.0F);
    expectVec2Near(third_ptr->getLayoutSize(), 20.0F, 10.0F);
}

TEST(UIGridLayoutTest, InvisibleChildrenDoNotConsumeGridSlots) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{80.0F, 60.0F});
    layout->setColumnCount(2);
    layout->setSpacing({1.0F, 1.0F});
    layout->setCellSize({10.0F, 10.0F});

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F});
    auto hidden = std::make_unique<UIElement>(glm::vec2{99.0F, 99.0F}, glm::vec2{1.0F, 1.0F});
    hidden->setVisible(false);
    auto third = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F});

    UIElement* first_ptr = first.get();
    UIElement* hidden_ptr = hidden.get();
    UIElement* third_ptr = third.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(hidden));
    layout->addChild(std::move(third));

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), 0.0F, 0.0F);
    expectVec2Near(hidden_ptr->getPosition(), 99.0F, 99.0F);
    expectVec2Near(third_ptr->getPosition(), 11.0F, 0.0F);
}

TEST(UIGridLayoutTest, RejectsNonPositiveColumnCountAndKeepsPreviousValue) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{300.0F, 120.0F});
    layout->setSpacing({0.0F, 0.0F});
    layout->setCellSize({10.0F, 10.0F});

    layout->setColumnCount(0);

    std::vector<UIElement*> children{};
    for (int i = 0; i < 6; ++i) {
        auto child = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F});
        children.push_back(child.get());
        layout->addChild(std::move(child));
    }

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    // Default column_count_ is 5. If setColumnCount(0) is ignored, the 6th child starts a new row.
    expectVec2Near(children[4]->getPosition(), 40.0F, 0.0F);
    expectVec2Near(children[5]->getPosition(), 0.0F, 10.0F);
}

TEST(UIGridLayoutTest, ClearingFixedCellFallsBackToIntrinsicRequestedSize) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{80.0F, 60.0F});
    layout->setColumnCount(1);
    layout->setSpacing({0.0F, 0.0F});
    layout->setCellSize({20.0F, 10.0F});

    auto child = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{7.0F, 9.0F});
    UIElement* child_ptr = child.get();
    layout->addChild(std::move(child));

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));

    layout_ptr->forceLayout();
    expectVec2Near(child_ptr->getLayoutSize(), 20.0F, 10.0F);
    expectVec2Near(child_ptr->getRequestedSize(), 7.0F, 9.0F);

    layout_ptr->setCellSize({0.0F, 0.0F});
    layout_ptr->forceLayout();
    expectVec2Near(child_ptr->getLayoutSize(), 7.0F, 9.0F);
    expectVec2Near(child_ptr->getRequestedSize(), 7.0F, 9.0F);
}

TEST(UIGridLayoutTest, ReparentAfterRemovalClearsFixedCellLayoutOverride) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{80.0F, 60.0F});
    layout->setColumnCount(1);
    layout->setCellSize({20.0F, 10.0F});

    auto child = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{7.0F, 9.0F});
    UIElement* child_ptr = child.get();
    layout->addChild(std::move(child));

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(child_ptr->getLayoutSize(), 20.0F, 10.0F);
    expectVec2Near(child_ptr->getRequestedSize(), 7.0F, 9.0F);

    std::unique_ptr<UIElement> detached = layout_ptr->removeChild(child_ptr);
    ASSERT_NE(detached, nullptr);

    UIElement plain_parent({0.0F, 0.0F}, {100.0F, 100.0F});
    plain_parent.addChild(std::move(detached));

    expectVec2Near(child_ptr->getLayoutSize(), 7.0F, 9.0F);
    expectVec2Near(child_ptr->getRequestedSize(), 7.0F, 9.0F);
}

TEST(UIGridLayoutTest, IntrinsicVariableSizeUsesFlowLayoutPerRowWithoutOverlap) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{120.0F, 120.0F});
    layout->setColumnCount(2);
    layout->setSpacing({2.0F, 3.0F});
    layout->setCellSize({0.0F, 0.0F}); // intrinsic mode

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{10.0F, 5.0F});
    auto second = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{20.0F, 8.0F});
    auto third = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{30.0F, 6.0F});

    UIElement* first_ptr = first.get();
    UIElement* second_ptr = second.get();
    UIElement* third_ptr = third.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(second));
    layout->addChild(std::move(third));

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), 0.0F, 0.0F);
    expectVec2Near(second_ptr->getPosition(), 12.0F, 0.0F);
    // Row 2 starts at previous row max height (8) + spacing (3).
    expectVec2Near(third_ptr->getPosition(), 0.0F, 11.0F);
}

TEST(UIGridLayoutTest, OverflowingRowsArePositionedWithoutContainerClamping) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIGridLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{25.0F, 15.0F});
    layout->setColumnCount(2);
    layout->setSpacing({2.0F, 2.0F});
    layout->setCellSize({10.0F, 10.0F});

    std::vector<UIElement*> children{};
    for (int i = 0; i < 5; ++i) {
        auto child = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F});
        children.push_back(child.get());
        layout->addChild(std::move(child));
    }

    UIGridLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(children[0]->getPosition(), 0.0F, 0.0F);
    expectVec2Near(children[1]->getPosition(), 12.0F, 0.0F);
    expectVec2Near(children[2]->getPosition(), 0.0F, 12.0F);
    expectVec2Near(children[3]->getPosition(), 12.0F, 12.0F);
    // Overflow case: grid does not clamp child coordinates to container bounds.
    expectVec2Near(children[4]->getPosition(), 0.0F, 24.0F);
}

} // namespace engine::ui
