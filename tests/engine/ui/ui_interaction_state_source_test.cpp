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
// This file uses source-contract checks (string matching) as a safeguard.
// It does NOT replace runtime behavior tests.

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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

TEST(UIInteractionStateSourceTest, UIInteractiveBusinessVirtualCallbacksRemovedContractIsPresent) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    const std::string header_source = readTextFile(header_path);
    ASSERT_FALSE(header_source.empty());

    EXPECT_EQ(header_source.find("virtual void clicked()"), std::string::npos)
        << "UIInteractive should remove business clicked() virtual callback after UIR-042.";
    EXPECT_EQ(header_source.find("virtual void hover_enter()"), std::string::npos)
        << "UIInteractive should remove business hover_enter() virtual callback after UIR-042.";
    EXPECT_EQ(header_source.find("virtual void hover_leave()"), std::string::npos)
        << "UIInteractive should remove business hover_leave() virtual callback after UIR-042.";

    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("clicked();"), std::string::npos)
        << "UIInteractive should not call legacy clicked() virtual callback after UIR-042.";
    EXPECT_EQ(source.find("hover_enter();"), std::string::npos)
        << "UIInteractive should not call legacy hover_enter() virtual callback after UIR-042.";
    EXPECT_EQ(source.find("hover_leave();"), std::string::npos)
        << "UIInteractive should not call legacy hover_leave() virtual callback after UIR-042.";
}

TEST(UIInteractionStateSourceTest, UIElementFindInteractiveAtAvoidsRttiContractIsPresent) {
    const std::filesystem::path element_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_element.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(element_header_path)) << element_header_path;
    const std::string element_header = readTextFile(element_header_path);
    ASSERT_FALSE(element_header.empty());

    EXPECT_NE(element_header.find("virtual UIInteractive* asInteractive() { return nullptr; }"), std::string::npos)
        << "UIElement should provide a non-RTTI interactive downcast hook.";
    EXPECT_NE(element_header.find("virtual const UIInteractive* asInteractive() const { return nullptr; }"), std::string::npos)
        << "UIElement should provide const interactive downcast hook.";

    const std::filesystem::path interactive_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(interactive_header_path)) << interactive_header_path;
    const std::string interactive_header = readTextFile(interactive_header_path);
    ASSERT_FALSE(interactive_header.empty());

    EXPECT_NE(interactive_header.find("UIInteractive* asInteractive() override { return this; }"), std::string::npos)
        << "UIInteractive should override asInteractive() for direct cast-free dispatch.";
    EXPECT_NE(interactive_header.find("const UIInteractive* asInteractive() const override { return this; }"), std::string::npos)
        << "UIInteractive should override const asInteractive() for direct cast-free dispatch.";

    const std::filesystem::path element_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_element.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(element_source_path)) << element_source_path;
    const std::string element_source = readTextFile(element_source_path);
    ASSERT_FALSE(element_source.empty());

    EXPECT_NE(element_source.find("const UIInteractive* interactive = asInteractive();"), std::string::npos)
        << "findInteractiveAt should query interactive pointer via asInteractive().";
    EXPECT_EQ(element_source.find("dynamic_cast<const UIInteractive*>(this)"), std::string::npos)
        << "findInteractiveAt should avoid RTTI dynamic_cast after UIR-051.";
}

TEST(UIInteractionStateSourceTest, InteractionBehaviorStateChangedHookContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/behavior/interaction_behavior.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("enum class InteractionPhase : std::uint8_t;"), std::string::npos)
        << "InteractionBehavior should forward declare InteractionPhase.";
    EXPECT_NE(source.find("virtual void onStateChanged(UIInteractive&"), std::string::npos)
        << "InteractionBehavior should expose optional onStateChanged hook.";
}

TEST(UIInteractionStateSourceTest, ClickBehaviorContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/behavior/click_behavior.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("class ClickBehavior final : public InteractionBehavior"), std::string::npos)
        << "ClickBehavior should inherit from InteractionBehavior.";
    EXPECT_NE(source.find("using ClickCallback = std::function<void(UIInteractive&)>;"), std::string::npos)
        << "ClickBehavior should expose click callback alias.";
    EXPECT_NE(source.find("void setOnClick(ClickCallback cb)"), std::string::npos)
        << "ClickBehavior should allow setting click callback.";
    EXPECT_NE(source.find("void onClick(UIInteractive& owner) override"), std::string::npos)
        << "ClickBehavior should dispatch click callback.";
}

TEST(UIInteractionStateSourceTest, UIButtonCreateBindsClickAndHoverCallbacksViaBehaviorContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_button.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("bindCallbacksToBehaviors();"), std::string::npos)
        << "UIButton constructor should bind callback behaviors.";
    EXPECT_NE(source.find("std::make_unique<ClickBehavior>()"), std::string::npos)
        << "UIButton should install ClickBehavior for click callback dispatch.";
    EXPECT_NE(source.find("class UIButtonHoverStateBehavior final : public InteractionBehavior"), std::string::npos)
        << "UIButton should bind hover callbacks through behavior layer.";
    EXPECT_NE(source.find("void onStateChanged(UIInteractive& owner, InteractionPhase old_phase, InteractionPhase new_phase) override"), std::string::npos)
        << "UIButton hover behavior should use onStateChanged hook.";
    EXPECT_NE(source.find("new_phase == InteractionPhase::Hovered && old_phase != InteractionPhase::Hovered"), std::string::npos)
        << "UIButton hover enter callback should be mapped from any transition into hovered phase.";
    EXPECT_NE(source.find("old_phase == InteractionPhase::Hovered && new_phase == InteractionPhase::Normal"), std::string::npos)
        << "UIButton hover leave callback should be mapped from Hovered->Normal transition.";
}

TEST(UIInteractionStateSourceTest, UIButtonNoLongerReliesOnLegacyVirtualCallbacksContractIsPresent) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_button.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string header_source = readTextFile(header_path);
    ASSERT_FALSE(header_source.empty());

    EXPECT_EQ(header_source.find("void clicked() override;"), std::string::npos)
        << "UIButton should remove clicked() override after UIR-042.";
    EXPECT_EQ(header_source.find("void hover_enter() override;"), std::string::npos)
        << "UIButton should remove hover_enter() override after UIR-042.";
    EXPECT_EQ(header_source.find("void hover_leave() override;"), std::string::npos)
        << "UIButton should remove hover_leave() override after UIR-042.";
    EXPECT_EQ(header_source.find("callbacks_bound_to_behaviors_"), std::string::npos)
        << "UIButton should drop compatibility guard flag once legacy virtual callbacks are removed.";

    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_button.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("void UIButton::clicked()"), std::string::npos)
        << "UIButton should remove clicked() implementation after UIR-042.";
    EXPECT_EQ(source.find("void UIButton::hover_enter()"), std::string::npos)
        << "UIButton should remove hover_enter() implementation after UIR-042.";
    EXPECT_EQ(source.find("void UIButton::hover_leave()"), std::string::npos)
        << "UIButton should remove hover_leave() implementation after UIR-042.";
}

TEST(UIInteractionStateSourceTest, UIButtonVisualStateUsesInteractionPhaseAsSingleSourceContractIsPresent) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_button.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    const std::string header_source = readTextFile(header_path);
    ASSERT_FALSE(header_source.empty());

    EXPECT_EQ(header_source.find("UIButtonVisualState current_visual_state_"), std::string::npos)
        << "UIButton should not store duplicated current visual state after UIR-050.";
    EXPECT_EQ(header_source.find("fromStateId("), std::string::npos)
        << "UIButton should remove legacy state-id to visual-state mapping API after UIR-050.";
    EXPECT_EQ(header_source.find("void applyStateVisual(entt::id_type state_id) override;"), std::string::npos)
        << "UIButton should no longer override applyStateVisual for local visual state sync after UIR-050.";

    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_button.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("[[nodiscard]] UIButtonVisualState mapVisualState(InteractionPhase phase)"), std::string::npos)
        << "UIButton should map InteractionPhase to visual state at render time.";
    EXPECT_NE(source.find("const InteractionPhase phase = getInteractionPhase();"), std::string::npos)
        << "UIButton render path should read interaction phase as single source of truth.";
    EXPECT_NE(source.find("resolvePresetImageForState(*skin, phase)"), std::string::npos)
        << "UIButton image resolve should be phase-driven.";
    EXPECT_NE(source.find("resolvePresetLabelVisual(skin, getInteractionPhase())"), std::string::npos)
        << "UIButton label visual resolve should be phase-driven.";

    EXPECT_EQ(source.find("void UIButton::applyStateVisual(entt::id_type state_id)"), std::string::npos)
        << "UIButton should remove legacy applyStateVisual override implementation after UIR-050.";
    EXPECT_EQ(source.find("std::optional<UIButtonVisualState> UIButton::fromStateId"), std::string::npos)
        << "UIButton should remove legacy fromStateId implementation after UIR-050.";
}

TEST(UIInteractionStateSourceTest, InteractionPhaseRefreshTraceContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("InteractionPhase UIInteractive::computeInteractionPhase() const"), std::string::npos)
        << "UIInteractive should keep a dedicated phase normalization method.";
    EXPECT_NE(source.find("void UIInteractive::refreshInteractionPhase(std::string_view reason)"), std::string::npos)
        << "UIInteractive should centralize phase refresh and debug tracing.";
    EXPECT_NE(source.find("UIInteractive phase changed"), std::string::npos)
        << "Phase refresh should emit trace logs on phase transition.";
    EXPECT_NE(source.find("refreshInteractionPhase(\"setInteractive\")"), std::string::npos)
        << "setInteractive should refresh interaction phase.";
}

TEST(UIInteractionStateSourceTest, UIInteractiveNotifiesBehaviorOnPhaseChangedContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void UIInteractive::notifyPhaseChanged"), std::string::npos)
        << "UIInteractive should provide a dedicated phase change notifier.";
    EXPECT_NE(source.find("behavior->onStateChanged(*this, old_phase, new_phase);"), std::string::npos)
        << "UIInteractive should dispatch onStateChanged to mounted behaviors.";
    EXPECT_NE(source.find("notifyPhaseChanged(source_phase, interaction_phase_, \"transitionTo\");"), std::string::npos)
        << "transitionTo should notify behavior layer on phase transition.";
    EXPECT_NE(source.find("notifyPhaseChanged(old_phase, interaction_phase_, \"setEnabled(false)\");"), std::string::npos)
        << "setEnabled(false) should notify behavior layer on phase transition.";
    EXPECT_NE(source.find("notifyPhaseChanged(old_phase, interaction_phase_, \"setEnabled(true)\");"), std::string::npos)
        << "setEnabled(true) should notify behavior layer on phase transition.";
}

TEST(UIInteractionStateSourceTest, TransitionToImmediateAndHoverSoundContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void UIInteractive::transitionTo(InteractionPhase target_phase)"), std::string::npos)
        << "UIInteractive should provide transitionTo() for phase transitions.";
    EXPECT_NE(source.find("playSoundEvent(UI_SOUND_EVENT_HOVER_ID);"), std::string::npos)
        << "transitionTo should keep hover sound on Normal->Hovered transition.";
    EXPECT_EQ(source.find("hover_leave();"), std::string::npos)
        << "transitionTo should not dispatch legacy hover_leave virtual callbacks after UIR-042.";
    EXPECT_NE(source.find("interaction_phase_ = target_phase;"), std::string::npos)
        << "transitionTo should mutate phase directly at UIR-031.";
    EXPECT_NE(source.find("applyPhaseEnterEffects(interaction_phase_);"), std::string::npos)
        << "transitionTo should apply phase enter effects immediately.";
    EXPECT_EQ(source.find("setNextState(std::move(next));"), std::string::npos)
        << "transitionTo should not queue deferred transitions.";
    EXPECT_EQ(source.find("setState(std::move(next));"), std::string::npos)
        << "transitionTo should not depend on legacy UIState objects.";
    EXPECT_EQ(source.find("makeLegacyStateForPhase"), std::string::npos)
        << "transitionTo should not build legacy UIState objects.";
}

TEST(UIInteractionStateSourceTest, MouseEventsDrivePhaseDirectlyWithoutStateDispatchContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void UIInteractive::mouseEnter()"), std::string::npos)
        << "mouseEnter handler should exist.";
    EXPECT_NE(source.find("void UIInteractive::mouseExit()"), std::string::npos)
        << "mouseExit handler should exist.";
    EXPECT_NE(source.find("void UIInteractive::mousePressed()"), std::string::npos)
        << "mousePressed handler should exist.";
    EXPECT_NE(source.find("void UIInteractive::mouseReleased(bool is_inside)"), std::string::npos)
        << "mouseReleased handler should exist.";

    EXPECT_NE(source.find("transitionTo(InteractionPhase::Hovered);"), std::string::npos)
        << "mouseEnter/mouseReleased should drive hovered transition directly.";
    EXPECT_NE(source.find("transitionTo(InteractionPhase::Pressed);"), std::string::npos)
        << "mousePressed should drive pressed transition directly.";
    EXPECT_NE(source.find("transitionTo(InteractionPhase::Normal);"), std::string::npos)
        << "mouseExit/mouseReleased should drive normal transition directly.";

    EXPECT_EQ(source.find("state_->onMouseEnter()"), std::string::npos)
        << "mouseEnter should not dispatch via legacy state handlers after UIR-031.";
    EXPECT_EQ(source.find("state_->onMouseExit()"), std::string::npos)
        << "mouseExit should not dispatch via legacy state handlers after UIR-031.";
    EXPECT_EQ(source.find("state_->onMousePressed()"), std::string::npos)
        << "mousePressed should not dispatch via legacy state handlers after UIR-031.";
    EXPECT_EQ(source.find("state_->onMouseReleased(is_inside)"), std::string::npos)
        << "mouseReleased should not dispatch via legacy state handlers after UIR-031.";
}

TEST(UIInteractionStateSourceTest, MouseReleasedDispatchOrderContractIsPresent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const auto fn_pos = source.find("void UIInteractive::mouseReleased(bool is_inside)");
    ASSERT_NE(fn_pos, std::string::npos) << "mouseReleased implementation should exist.";

    const std::string phase_snapshot = "const InteractionPhase phase_before_release = computeInteractionPhase();";
    const std::string drag_end_call = "behavior->onDragEnd(*this, current, is_inside);";
    const std::string inside_transition = "transitionTo(InteractionPhase::Hovered);";
    const std::string outside_transition = "transitionTo(InteractionPhase::Normal);";
    const std::string behavior_release_call = "behavior->onReleased(*this, is_inside);";
    const std::string click_guard = "if (is_inside && phase_before_release == InteractionPhase::Pressed)";
    const std::string click_call = "behavior->onClick(*this);";

    const auto phase_pos = source.find(phase_snapshot, fn_pos);
    const auto drag_pos = source.find(drag_end_call, fn_pos);
    const auto inside_transition_pos = source.find(inside_transition, fn_pos);
    const auto outside_transition_pos = source.find(outside_transition, fn_pos);
    const auto release_pos = source.find(behavior_release_call, fn_pos);
    const auto guard_pos = source.find(click_guard, fn_pos);
    const auto click_pos = source.find(click_call, fn_pos);

    ASSERT_NE(phase_pos, std::string::npos) << "mouseReleased should snapshot phase before state changes.";
    ASSERT_NE(drag_pos, std::string::npos) << "mouseReleased should dispatch drag end to behaviors.";
    ASSERT_NE(inside_transition_pos, std::string::npos)
        << "mouseReleased inside should transition to hovered when releasing from pressed.";
    ASSERT_NE(outside_transition_pos, std::string::npos)
        << "mouseReleased outside should transition to normal when releasing from pressed.";
    ASSERT_NE(release_pos, std::string::npos) << "mouseReleased should dispatch behavior onReleased.";
    ASSERT_NE(guard_pos, std::string::npos) << "mouseReleased should guard click by inside check.";
    ASSERT_NE(click_pos, std::string::npos) << "mouseReleased should dispatch click to behaviors.";
    EXPECT_EQ(source.find("clicked();", fn_pos), std::string::npos)
        << "mouseReleased should not dispatch legacy clicked() callback after UIR-042.";

    EXPECT_LT(phase_pos, drag_pos)
        << "Phase snapshot should happen before drag end/callback pipeline.";
    EXPECT_LT(drag_pos, inside_transition_pos)
        << "Drag end should be dispatched before transition/click handling.";
    EXPECT_LT(inside_transition_pos, release_pos)
        << "Inside transition should happen before behavior onReleased dispatch.";
    EXPECT_LT(release_pos, guard_pos)
        << "Behavior onReleased should occur before inside click dispatch.";
    EXPECT_LT(guard_pos, click_pos)
        << "Inside guard should appear before behavior click dispatch.";
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
    EXPECT_NE(source.find("interaction_phase_ = InteractionPhase::Disabled;"), std::string::npos)
        << "setEnabled(false) should normalize phase to disabled.";
    EXPECT_NE(source.find("interaction_phase_ = InteractionPhase::Normal;"), std::string::npos)
        << "setEnabled(true) should normalize phase to normal.";
    EXPECT_NE(source.find("applyStateVisual(UI_IMAGE_DISABLED_ID);"), std::string::npos)
        << "setEnabled(false) should drive disabled visual state.";
    EXPECT_NE(source.find("applyStateVisual(UI_IMAGE_NORMAL_ID);"), std::string::npos)
        << "setEnabled(true) should restore normal visual state.";
    EXPECT_EQ(source.find("std::make_unique<engine::ui::state::UINormalState>(this)"), std::string::npos)
        << "setEnabled should not rely on legacy UIState normalization at UIR-031.";
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

TEST(UIInteractionStateSourceTest, LegacyStateArtifactsRemovedContractIsPresent) {
    const std::filesystem::path state_dir =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/state").lexically_normal();
    const std::filesystem::path state_h = (state_dir / "ui_state.h").lexically_normal();
    const std::filesystem::path normal_h = (state_dir / "ui_normal_state.h").lexically_normal();
    const std::filesystem::path hover_h = (state_dir / "ui_hover_state.h").lexically_normal();
    const std::filesystem::path pressed_h = (state_dir / "ui_pressed_state.h").lexically_normal();
    const std::filesystem::path normal_cpp = (state_dir / "ui_normal_state.cpp").lexically_normal();
    const std::filesystem::path hover_cpp = (state_dir / "ui_hover_state.cpp").lexically_normal();
    const std::filesystem::path pressed_cpp = (state_dir / "ui_pressed_state.cpp").lexically_normal();

    EXPECT_FALSE(std::filesystem::exists(state_h)) << state_h;
    EXPECT_FALSE(std::filesystem::exists(normal_h)) << normal_h;
    EXPECT_FALSE(std::filesystem::exists(hover_h)) << hover_h;
    EXPECT_FALSE(std::filesystem::exists(pressed_h)) << pressed_h;
    EXPECT_FALSE(std::filesystem::exists(normal_cpp)) << normal_cpp;
    EXPECT_FALSE(std::filesystem::exists(hover_cpp)) << hover_cpp;
    EXPECT_FALSE(std::filesystem::exists(pressed_cpp)) << pressed_cpp;

    const std::filesystem::path interactive_h_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_interactive.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(interactive_h_path)) << interactive_h_path;
    const std::string interactive_h = readTextFile(interactive_h_path);

    EXPECT_EQ(interactive_h.find("#include \"state/ui_state.h\""), std::string::npos)
        << "UIInteractive header should not include legacy UIState.";
    EXPECT_EQ(interactive_h.find("void setState(std::unique_ptr<engine::ui::state::UIState> state)"), std::string::npos)
        << "UIInteractive should not expose setState API after UIR-031.";
    EXPECT_EQ(interactive_h.find("void setNextState(std::unique_ptr<engine::ui::state::UIState> state)"), std::string::npos)
        << "UIInteractive should not expose setNextState API after UIR-031.";
    EXPECT_EQ(interactive_h.find("engine::ui::state::UIState* getState() const"), std::string::npos)
        << "UIInteractive should not expose getState API after UIR-031.";
}

} // namespace
} // namespace engine::ui
// NOLINTEND
