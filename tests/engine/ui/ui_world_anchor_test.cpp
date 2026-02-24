#include <gtest/gtest.h>

#include <memory>

#include "engine/ui/ui_element.h"

namespace engine::ui {
namespace {

class WorldAnchorTestElement final : public UIElement {
public:
    using UIElement::UIElement;

    void applyWorldAnchorForTest(glm::vec2 screen_pos) { applyWorldAnchorPosition(screen_pos); }
    bool isLayoutDirtyForTest() const { return layout_dirty_; }
};

TEST(UIWorldAnchorTest, SetWorldAnchorSetsPositioningModeAndData) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{30.0F, 40.0F});
    auto* child_ptr = child.get();
    root.addChild(std::move(child));

    child_ptr->setWorldAnchor({12.0F, 34.0F}, {1.0F, -2.0F});
    EXPECT_EQ(child_ptr->getPositioningMode(), PositioningMode::WorldAnchor);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchor().x, 12.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchor().y, 34.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchorOffset().x, 1.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchorOffset().y, -2.0F);
}

TEST(UIWorldAnchorTest, ClearWorldAnchorRestoresScreenModeAndClearsData) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{30.0F, 40.0F});
    auto* child_ptr = child.get();
    root.addChild(std::move(child));

    child_ptr->setWorldAnchor({12.0F, 34.0F}, {1.0F, -2.0F});
    child_ptr->clearWorldAnchor();

    EXPECT_EQ(child_ptr->getPositioningMode(), PositioningMode::Screen);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchor().x, 0.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchor().y, 0.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchorOffset().x, 0.0F);
    EXPECT_FLOAT_EQ(child_ptr->getWorldAnchorOffset().y, 0.0F);
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionConsidersPivot) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{20.0F, 10.0F});
    auto* child_ptr = child.get();
    root.addChild(std::move(child));

    child_ptr->setPivot({0.5F, 1.0F});
    child_ptr->setWorldAnchor({100.0F, 200.0F});
    child_ptr->applyWorldAnchorForTest({100.0F, 80.0F});

    const glm::vec2 screen_pos = child_ptr->getScreenPosition();
    EXPECT_NEAR(screen_pos.x, 90.0F, 0.001F);
    EXPECT_NEAR(screen_pos.y, 70.0F, 0.001F);
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionInvalidatesChildren) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto parent = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{20.0F, 10.0F});
    auto* parent_ptr = parent.get();
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{1.0F, 2.0F}, glm::vec2{8.0F, 6.0F});
    auto* child_ptr = child.get();
    parent_ptr->addChild(std::move(child));
    root.addChild(std::move(parent));

    (void)child_ptr->getLayoutSize();
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());

    parent_ptr->setWorldAnchor({100.0F, 100.0F});
    parent_ptr->applyWorldAnchorForTest({50.0F, 60.0F});
    EXPECT_TRUE(child_ptr->isLayoutDirtyForTest());
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionDiffGuardSkipsUnchanged) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto parent = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{20.0F, 10.0F});
    auto* parent_ptr = parent.get();
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{1.0F, 2.0F}, glm::vec2{8.0F, 6.0F});
    auto* child_ptr = child.get();
    parent_ptr->addChild(std::move(child));
    root.addChild(std::move(parent));

    parent_ptr->setWorldAnchor({100.0F, 100.0F});
    parent_ptr->applyWorldAnchorForTest({50.0F, 60.0F});
    (void)child_ptr->getLayoutSize();
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());

    parent_ptr->applyWorldAnchorForTest({50.0F, 60.0F});
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());
}

TEST(UIWorldAnchorTest, NonRootChildWorldAnchorFallsBackToScreenLayout) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto parent = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{100.0F, 80.0F});
    auto* parent_ptr = parent.get();
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{8.0F, 9.0F}, glm::vec2{20.0F, 10.0F});
    auto* child_ptr = child.get();
    parent_ptr->addChild(std::move(child));
    root.addChild(std::move(parent));

    child_ptr->setWorldAnchor({999.0F, 999.0F});
    const glm::vec2 pos = child_ptr->getScreenPosition();
    EXPECT_NEAR(pos.x, 18.0F, 0.001F);
    EXPECT_NEAR(pos.y, 29.0F, 0.001F);
}

TEST(UIWorldAnchorTest, ClearWorldAnchorUsesExistingPosition) {
    UIElement root({0.0F, 0.0F}, {400.0F, 300.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{7.0F, 11.0F}, glm::vec2{20.0F, 10.0F});
    auto* child_ptr = child.get();
    root.addChild(std::move(child));

    child_ptr->setWorldAnchor({100.0F, 200.0F});
    child_ptr->applyWorldAnchorForTest({50.0F, 60.0F});
    EXPECT_NEAR(child_ptr->getScreenPosition().x, 50.0F, 0.001F);
    EXPECT_NEAR(child_ptr->getScreenPosition().y, 60.0F, 0.001F);

    child_ptr->clearWorldAnchor();
    const glm::vec2 restored = child_ptr->getScreenPosition();
    EXPECT_NEAR(restored.x, 7.0F, 0.001F);
    EXPECT_NEAR(restored.y, 11.0F, 0.001F);
}

} // namespace
} // namespace engine::ui
