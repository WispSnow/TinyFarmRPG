#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "engine/ui/layout/ui_stack_layout.h"

namespace engine::ui {
namespace {

constexpr float kEpsilon = 0.001F;

void expectVec2Near(const glm::vec2& actual, float expected_x, float expected_y) {
    EXPECT_NEAR(actual.x, expected_x, kEpsilon);
    EXPECT_NEAR(actual.y, expected_y, kEpsilon);
}

} // namespace

TEST(UIStackLayoutTest, HorizontalLayoutSkipsInvisibleChildrenWhenPacking) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIStackLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{100.0F, 20.0F});
    layout->setOrientation(Orientation::Horizontal);
    layout->setSpacing(5.0F);

    auto first = std::make_unique<UIElement>(glm::vec2{99.0F, 99.0F}, glm::vec2{10.0F, 10.0F});
    auto hidden = std::make_unique<UIElement>(glm::vec2{77.0F, 66.0F}, glm::vec2{20.0F, 10.0F});
    hidden->setVisible(false);
    auto third = std::make_unique<UIElement>(glm::vec2{88.0F, 88.0F}, glm::vec2{15.0F, 10.0F});

    UIElement* first_ptr = first.get();
    UIElement* hidden_ptr = hidden.get();
    UIElement* third_ptr = third.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(hidden));
    layout->addChild(std::move(third));

    UIStackLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), 0.0F, 0.0F);
    expectVec2Near(hidden_ptr->getPosition(), 77.0F, 66.0F);
    expectVec2Near(third_ptr->getPosition(), 15.0F, 0.0F);
}

TEST(UIStackLayoutTest, CenterAlignmentOffsetsChildrenWhenContainerIsLarger) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIStackLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{100.0F, 20.0F});
    layout->setOrientation(Orientation::Horizontal);
    layout->setSpacing(5.0F);
    layout->setContentAlignment(Alignment::Center);

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{10.0F, 10.0F});
    auto second = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{20.0F, 10.0F});
    UIElement* first_ptr = first.get();
    UIElement* second_ptr = second.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(second));

    UIStackLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), 32.5F, 0.0F);
    expectVec2Near(second_ptr->getPosition(), 47.5F, 0.0F);
}

TEST(UIStackLayoutTest, EndAlignmentAllowsNegativeOffsetWhenContentOverflows) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIStackLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{40.0F, 20.0F});
    layout->setOrientation(Orientation::Horizontal);
    layout->setSpacing(5.0F);
    layout->setContentAlignment(Alignment::End);

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{20.0F, 10.0F});
    auto second = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{30.0F, 10.0F});
    UIElement* first_ptr = first.get();
    UIElement* second_ptr = second.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(second));

    UIStackLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();

    expectVec2Near(first_ptr->getPosition(), -15.0F, 0.0F);
    expectVec2Near(second_ptr->getPosition(), 10.0F, 0.0F);
}

TEST(UIStackLayoutTest, AutoResizeUpdatesMainAxisSizeUsingVisibleChildrenOnly) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto layout = std::make_unique<UIStackLayout>(glm::vec2{0.0F, 0.0F}, glm::vec2{30.0F, 1.0F});
    layout->setOrientation(Orientation::Vertical);
    layout->setSpacing(2.0F);
    layout->setAutoResize(true);

    auto first = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{8.0F, 10.0F});
    auto second = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{8.0F, 5.0F});
    auto hidden = std::make_unique<UIElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{8.0F, 100.0F});
    hidden->setVisible(false);

    UIElement* first_ptr = first.get();
    UIElement* second_ptr = second.get();

    layout->addChild(std::move(first));
    layout->addChild(std::move(second));
    layout->addChild(std::move(hidden));

    UIStackLayout* layout_ptr = layout.get();
    root.addChild(std::move(layout));
    layout_ptr->forceLayout();
    layout_ptr->forceLayout();

    EXPECT_NEAR(layout_ptr->getRequestedSize().y, 17.0F, kEpsilon);
    EXPECT_NEAR(layout_ptr->getLayoutSize().y, 17.0F, kEpsilon);
    expectVec2Near(first_ptr->getPosition(), 0.0F, 0.0F);
    expectVec2Near(second_ptr->getPosition(), 0.0F, 12.0F);
}

} // namespace engine::ui
