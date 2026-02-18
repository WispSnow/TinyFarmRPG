#include <gtest/gtest.h>

#include <memory>

#include "engine/ui/ui_element.h"

namespace engine::ui {
namespace {

class CountingElement final : public UIElement {
public:
    using UIElement::UIElement;
    int layout_calls{0};

protected:
    void onLayout() override { ++layout_calls; }
};

TEST(UILayoutInvalidationTest, SamePositionDoesNotTriggerRelayout) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<CountingElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{30.0F, 40.0F});
    CountingElement* child_ptr = child.get();
    root.addChild(std::move(child));

    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 1);

    child_ptr->setPosition({10.0F, 20.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 1);

    child_ptr->setPosition({11.0F, 20.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 2);
}

TEST(UILayoutInvalidationTest, SameSizeAndAnchorDoNotTriggerRelayout) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<CountingElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{30.0F, 40.0F});
    CountingElement* child_ptr = child.get();
    root.addChild(std::move(child));

    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 1);

    child_ptr->setSize({30.0F, 40.0F});
    child_ptr->setAnchor({0.0F, 0.0F}, {0.0F, 0.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 1);

    child_ptr->setSize({31.0F, 40.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(child_ptr->layout_calls, 2);
}

TEST(UILayoutInvalidationTest, LayoutRecomputeCounterTracksDirtyRecomputes) {
    UIElement::resetLayoutRecomputeCounter();

    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<CountingElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{30.0F, 40.0F});
    CountingElement* child_ptr = child.get();
    root.addChild(std::move(child));

    (void)child_ptr->getLayoutSize();
    const std::uint64_t first_pass = UIElement::consumeLayoutRecomputeCounter();
    EXPECT_GE(first_pass, 2U);

    child_ptr->setPosition({10.0F, 20.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_EQ(UIElement::consumeLayoutRecomputeCounter(), 0U);

    child_ptr->setPosition({12.0F, 20.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_GE(UIElement::consumeLayoutRecomputeCounter(), 1U);
}

TEST(UILayoutInvalidationTest, RootElementCanUseLayoutOverrideSize) {
    UIElement root({5.0F, 6.0F}, {100.0F, 80.0F});
    EXPECT_EQ(root.getLayoutSize(), glm::vec2(100.0F, 80.0F));

    root.setLayoutOverrideSize(glm::vec2{20.0F, 10.0F});
    EXPECT_EQ(root.getLayoutSize(), glm::vec2(20.0F, 10.0F));

    root.clearLayoutOverrideSize();
    EXPECT_EQ(root.getLayoutSize(), glm::vec2(100.0F, 80.0F));
}

} // namespace
} // namespace engine::ui
