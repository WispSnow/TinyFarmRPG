#include "battle_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_input_blocker.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_panel.h"

#include <entt/entity/entity.hpp>
#include <entt/signal/dispatcher.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float RESULT_HOLD_SECONDS = 0.20f;
constexpr engine::ui::Thickness PANEL_PADDING{20.0f, 20.0f, 20.0f, 20.0f};
constexpr glm::vec2 PANEL_SIZE{480.0f, 260.0f};
constexpr glm::vec2 ACTION_BUTTON_SIZE{160.0f, 36.0f};

[[nodiscard]] std::string formatUnitsLine(const std::vector<game::battle::BattleUnit>& units) {
    std::ostringstream stream;
    stream << "Units: ";

    for (std::size_t i = 0; i < units.size(); ++i) {
        const auto& unit = units[i];
        if (i > 0) {
            stream << " | ";
        }

        stream << unit.name << " " << unit.hp << "/" << unit.max_hp;
        if (!unit.isAlive()) {
            stream << " (KO)";
        }
    }

    return stream.str();
}

} // namespace

namespace game::scene {

BattleScene::BattleScene(std::string_view name,
                         engine::core::Context& context,
                         std::vector<game::battle::BattleUnit> units)
    : engine::scene::Scene(name, context),
      session_(std::move(units)) {
}

bool BattleScene::init() {
    if (!initUI()) {
        return false;
    }

    if (!Scene::init()) {
        return false;
    }

    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        state_ = FlowState::BattleEnd;
    }
    refreshView();
    return true;
}

void BattleScene::update(float delta_time) {
    Scene::update(delta_time);
    runStateMachine(delta_time);
    refreshView();
}

bool BattleScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);

    buildLayout();
    return true;
}

void BattleScene::buildLayout() {
    auto dim = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f},
        glm::vec2{0.0f, 0.0f},
        engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.60f});
    dim->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    dim->setPivot({0.0f, 0.0f});
    dim->setOrderIndex(0);
    dim_ = dim.get();
    ui_manager_->addElement(std::move(dim));

    auto blocker = std::make_unique<engine::ui::UIInputBlocker>(
        context_, glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
    blocker->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    blocker->setPivot({0.0f, 0.0f});
    blocker->setOrderIndex(1);
    input_blocker_ = blocker.get();
    ui_manager_->addElement(std::move(blocker));

    auto panel = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f},
        PANEL_SIZE,
        engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.78f});
    panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    panel->setPivot({0.5f, 0.5f});
    panel->setPadding(PANEL_PADDING);
    panel->setOrderIndex(2);
    panel_ = panel.get();

    auto& text_renderer = context_.getTextRenderer();

    auto title_label = std::make_unique<engine::ui::UILabel>(text_renderer, "Battle Prototype");
    title_label->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    title_label->setPivot({0.5f, 0.0f});
    panel_->addChild(std::move(title_label));

    auto turn_label = std::make_unique<engine::ui::UILabel>(
        text_renderer,
        "Turn: -",
        entt::null,
        engine::ui::DEFAULT_UI_FONT_SIZE_PX,
        glm::vec2{0.0f, 42.0f});
    turn_label->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    turn_label->setPivot({0.0f, 0.0f});
    turn_label_ = turn_label.get();
    panel_->addChild(std::move(turn_label));

    auto units_label = std::make_unique<engine::ui::UILabel>(
        text_renderer,
        "Units: -",
        entt::null,
        engine::ui::DEFAULT_UI_FONT_SIZE_PX,
        glm::vec2{0.0f, 78.0f});
    units_label->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    units_label->setPivot({0.0f, 0.0f});
    units_label_ = units_label.get();
    panel_->addChild(std::move(units_label));

    auto result_label = std::make_unique<engine::ui::UILabel>(
        text_renderer,
        "Result: -",
        entt::null,
        engine::ui::DEFAULT_UI_FONT_SIZE_PX,
        glm::vec2{0.0f, 118.0f});
    result_label->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    result_label->setPivot({0.0f, 0.0f});
    result_label_ = result_label.get();
    panel_->addChild(std::move(result_label));

    auto attack_button = engine::ui::UIButton::create(
        context_,
        "primary",
        glm::vec2{0.0f, 180.0f},
        ACTION_BUTTON_SIZE,
        [this]() { queueAttackAction(); });
    if (attack_button) {
        attack_button->setLabelText("Attack");
        attack_button_ = attack_button.get();
        panel_->addChild(std::move(attack_button));
    }

    auto end_turn_button = engine::ui::UIButton::create(
        context_,
        "secondary",
        glm::vec2{ACTION_BUTTON_SIZE.x + 12.0f, 180.0f},
        ACTION_BUTTON_SIZE,
        [this]() { queueEndTurnAction(); });
    if (end_turn_button) {
        end_turn_button->setLabelText("End Turn");
        end_turn_button_ = end_turn_button.get();
        panel_->addChild(std::move(end_turn_button));
    }

    ui_manager_->addElement(std::move(panel));
}

void BattleScene::runStateMachine(float delta_time) {
    bool keep_running = true;
    while (keep_running) {
        keep_running = false;

        switch (state_) {
            case FlowState::WaitingForInput:
                return;
            case FlowState::ExecutingAction: {
                if (!pending_action_) {
                    state_ = FlowState::WaitingForInput;
                    return;
                }

                last_action_result_ = session_.submitAction(*pending_action_);
                pending_action_.reset();
                animation_timer_ = RESULT_HOLD_SECONDS;
                state_ = FlowState::AnimatingResult;
                keep_running = true;
                break;
            }
            case FlowState::AnimatingResult: {
                animation_timer_ -= delta_time;
                if (animation_timer_ <= 0.0f) {
                    state_ = FlowState::CheckVictory;
                    keep_running = true;
                }
                break;
            }
            case FlowState::CheckVictory:
                state_ = (session_.outcome() == game::battle::BattleOutcome::Ongoing)
                    ? FlowState::NextTurn
                    : FlowState::BattleEnd;
                keep_running = true;
                break;
            case FlowState::NextTurn:
                state_ = FlowState::WaitingForInput;
                break;
            case FlowState::BattleEnd:
                requestBattleEnd();
                return;
        }
    }
}

void BattleScene::refreshView() {
    const auto current_actor_id = session_.currentActorId();
    const auto& units = session_.units();

    if (turn_label_) {
        if (current_actor_id) {
            const auto* actor = session_.findUnit(*current_actor_id);
            if (actor) {
                turn_label_->setText("Turn: " + actor->name + " (" + std::string(game::battle::toString(actor->side)) + ")");
            }
        } else {
            turn_label_->setText("Turn: -");
        }
    }

    if (units_label_) {
        units_label_->setText(formatUnitsLine(units));
    }

    if (result_label_) {
        if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
            result_label_->setText("Result: " + std::string(game::battle::toString(session_.outcome())));
        } else if (last_action_result_) {
            const auto& result = *last_action_result_;
            if (result.status == game::battle::BattleActionStatus::Rejected) {
                if (!result.failure_reason.empty()) {
                    result_label_->setText("Result: " + result.failure_reason);
                } else {
                    result_label_->setText("Result: Action rejected");
                }
            } else if (result.action_type == game::battle::BattleActionType::Attack) {
                std::string text = "Result: Attack dealt " + std::to_string(result.damage) + " dmg";
                if (result.target_defeated) {
                    text += " (KO)";
                }
                result_label_->setText(std::move(text));
            } else if (result.action_type == game::battle::BattleActionType::Escape) {
                result_label_->setText(result.escape_succeeded ? "Result: Escaped" : "Result: Escape failed");
            } else {
                result_label_->setText("Result: Turn ended");
            }
        } else {
            result_label_->setText("Result: Choose action");
        }
    }

    const bool can_submit_action =
        !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        current_actor_id.has_value();

    if (attack_button_) {
        attack_button_->setEnabled(can_submit_action);
    }
    if (end_turn_button_) {
        end_turn_button_->setEnabled(can_submit_action);
    }
}

void BattleScene::queueAttackAction() {
    if (state_ != FlowState::WaitingForInput || session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id) {
        return;
    }

    const auto* actor = session_.findUnit(*actor_id);
    if (!actor) {
        return;
    }

    const auto target_id = selectDefaultTarget(actor->side);
    if (!target_id) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Attack,
        .actor_id = *actor_id,
        .target_id = target_id
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueEndTurnAction() {
    if (state_ != FlowState::WaitingForInput || session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::EndTurn,
        .actor_id = *actor_id,
        .target_id = std::nullopt
    };
    state_ = FlowState::ExecutingAction;
}

std::optional<game::battle::BattleUnitId> BattleScene::selectDefaultTarget(const game::battle::BattleSide actor_side) const {
    for (const auto& unit : session_.units()) {
        if (!unit.isAlive()) {
            continue;
        }
        if (unit.side == actor_side) {
            continue;
        }
        return unit.id;
    }

    return std::nullopt;
}

void BattleScene::requestBattleEnd() {
    if (end_requested_) {
        return;
    }

    end_requested_ = true;

    game::defs::BattleEndedEvent event{};
    event.outcome = session_.outcome();
    event.final_units = session_.units();
    context_.getDispatcher().trigger(event);

    requestPopScene();
}

} // namespace game::scene
