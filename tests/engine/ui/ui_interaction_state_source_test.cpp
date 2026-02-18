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

// NOTE:
// This file uses source-contract checks (string matching) as a temporary safeguard.
// It does NOT prove runtime behavior correctness. Runtime interaction-state tests
// should be added in UIR-022.

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(UIInteractionStateSourceTest, PressedStateReleaseInsideOutsideContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/state/ui_pressed_state.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    // inside release -> hover + clicked
    EXPECT_NE(source.find("if (is_inside)"), std::string::npos)
        << "Pressed release should branch by inside/outside.";
    EXPECT_NE(source.find("owner_->setNextState(std::make_unique<UIHoverState>(owner_));"), std::string::npos)
        << "Pressed release inside should transition to hover.";
    EXPECT_NE(source.find("owner_->clicked();"), std::string::npos)
        << "Pressed release inside should trigger clicked callback.";

    // outside release -> normal
    EXPECT_NE(source.find("owner_->setNextState(std::make_unique<UINormalState>(owner_));"), std::string::npos)
        << "Pressed release outside should transition back to normal.";
}

TEST(UIInteractionStateSourceTest, NormalStatePressTransitionsToPressedContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/state/ui_normal_state.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void UINormalState::onMousePressed()"), std::string::npos)
        << "UINormalState should explicitly handle mouse press.";
    EXPECT_NE(source.find("owner_->setNextState(std::make_unique<UIPressedState>(owner_));"), std::string::npos)
        << "Normal state press should transition to pressed state.";
}

// NOTE:
// For UIR-010, Normal->Pressed path is expected to skip HoverState::enter(),
// so hover_enter() is not triggered on same-frame press. This avoids hover flash.

TEST(UIInteractionStateSourceTest, MouseReleasedDispatchOrderContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const std::string drag_end_call = "behavior->onDragEnd(*this, current, is_inside);";
    const std::string state_release_call = "if (state_) state_->onMouseReleased(is_inside);";
    const std::string behavior_release_call = "behavior->onReleased(*this, is_inside);";
    const std::string click_guard = "if (is_inside)";
    const std::string click_call = "behavior->onClick(*this);";

    const auto drag_pos = source.find(drag_end_call);
    const auto state_pos = source.find(state_release_call);
    const auto release_pos = source.find(behavior_release_call);
    const auto guard_pos = source.find(click_guard);
    const auto click_pos = source.find(click_call);

    ASSERT_NE(drag_pos, std::string::npos) << "mouseReleased should dispatch drag end to behaviors.";
    ASSERT_NE(state_pos, std::string::npos) << "mouseReleased should dispatch state onMouseReleased.";
    ASSERT_NE(release_pos, std::string::npos) << "mouseReleased should dispatch behavior onReleased.";
    ASSERT_NE(guard_pos, std::string::npos) << "mouseReleased should guard click by inside check.";
    ASSERT_NE(click_pos, std::string::npos) << "mouseReleased should dispatch click to behaviors.";

    EXPECT_LT(drag_pos, state_pos)
        << "Drag end should be dispatched before state release handling.";
    EXPECT_LT(state_pos, release_pos)
        << "State release handling should occur before behavior onReleased dispatch.";
    EXPECT_LT(release_pos, guard_pos)
        << "Behavior onReleased should occur before inside click dispatch.";
    EXPECT_LT(guard_pos, click_pos)
        << "Inside guard should appear before behavior click dispatch.";
}

TEST(UIInteractionStateSourceTest, ClearMouseStateCancelsPressedCaptureContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_manager.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const std::string pressed_guard = "if (pressed_element_)";
    const std::string pressed_alias = "auto* pressed = pressed_element_;";
    const std::string clear_capture = "pressed_element_ = nullptr;";
    const std::string cancel_release = "pressed->mouseReleased(false);";

    const auto guard_pos = source.find(pressed_guard);
    const auto alias_pos = source.find(pressed_alias);
    const auto clear_pos = source.find(clear_capture, alias_pos);
    const auto cancel_pos = source.find(cancel_release);

    ASSERT_NE(guard_pos, std::string::npos) << "clearMouseState should guard pressed capture cleanup.";
    ASSERT_NE(alias_pos, std::string::npos) << "clearMouseState should keep a local pressed pointer before clearing.";
    ASSERT_NE(clear_pos, std::string::npos) << "clearMouseState should clear pressed_element_ capture pointer.";
    ASSERT_NE(cancel_pos, std::string::npos) << "clearMouseState should issue cancel release (inside=false).";

    EXPECT_LT(alias_pos, clear_pos)
        << "clearMouseState should clear pressed_element_ before invoking callbacks.";
    EXPECT_LT(clear_pos, cancel_pos)
        << "clearMouseState should avoid re-entrancy by clearing capture before cancel release.";
}

TEST(UIInteractionStateSourceTest, SetEnabledUnifiedSemanticsContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void UIInteractive::setEnabled(bool enabled)"), std::string::npos)
        << "UIInteractive should expose setEnabled(bool) as unified enable/disable entry.";
    EXPECT_NE(source.find("mouseReleased(false);"), std::string::npos)
        << "setEnabled(false) should cancel an active press via release(false).";
    EXPECT_NE(source.find("applyStateVisual(UI_IMAGE_DISABLED_ID);"), std::string::npos)
        << "setEnabled(false) should drive disabled visual state.";
    EXPECT_NE(source.find("applyStateVisual(UI_IMAGE_NORMAL_ID);"), std::string::npos)
        << "setEnabled(true) should restore normal visual state.";
    EXPECT_NE(source.find("std::make_unique<engine::ui::state::UINormalState>(this)"), std::string::npos)
        << "setEnabled should normalize interaction state when toggling enabled flag.";
}

TEST(UIInteractionStateSourceTest, InteractionPhaseEnumAndGetterContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("enum class InteractionPhase : std::uint8_t"), std::string::npos)
        << "UIInteractive should define a typed InteractionPhase enum.";
    EXPECT_NE(source.find("Normal"), std::string::npos)
        << "InteractionPhase should include Normal.";
    EXPECT_NE(source.find("Hovered"), std::string::npos)
        << "InteractionPhase should include Hovered.";
    EXPECT_NE(source.find("Pressed"), std::string::npos)
        << "InteractionPhase should include Pressed.";
    EXPECT_NE(source.find("Disabled"), std::string::npos)
        << "InteractionPhase should include Disabled.";
    EXPECT_NE(source.find("InteractionPhase getInteractionPhase() const"), std::string::npos)
        << "UIInteractive should expose read-only interaction phase query.";
}

TEST(UIInteractionStateSourceTest, InteractionPhaseRefreshTraceContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("InteractionPhase UIInteractive::computeInteractionPhase() const"), std::string::npos)
        << "UIInteractive should compute phase from current legacy state.";
    EXPECT_NE(source.find("void UIInteractive::refreshInteractionPhase(std::string_view reason)"), std::string::npos)
        << "UIInteractive should centralize phase refresh and debug tracing.";
    EXPECT_NE(source.find("UIInteractive phase changed"), std::string::npos)
        << "Phase refresh should emit trace logs on phase transition.";
    EXPECT_NE(source.find("refreshInteractionPhase(\"setState\")"), std::string::npos)
        << "setState should refresh interaction phase.";
    EXPECT_NE(source.find("refreshInteractionPhase(\"setInteractive\")"), std::string::npos)
        << "setInteractive should refresh interaction phase.";
}

} // namespace
} // namespace engine::ui
// NOLINTEND
