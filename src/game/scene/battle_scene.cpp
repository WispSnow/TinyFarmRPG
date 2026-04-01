#include "battle_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/entity/entity.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float RESULT_HOLD_SECONDS = 0.20f;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/battle.rml";
constexpr std::string_view MODEL_NAME = "battle_scene";
// TODO(FND-010): replace hardcoded action IDs with player-selectable skill/item UI.
constexpr std::string_view kDefaultSkillId = "skill.attack";
constexpr std::string_view kDefaultItemId = "strawberry_item";

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

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

BattleScene::BattleScene(std::string_view name,
                         engine::core::Context& context,
                         std::vector<game::battle::BattleUnit> units,
                         game::battle::BattleSessionOptions session_options)
    : engine::scene::Scene(name, context),
      session_(std::move(units), std::move(session_options)) {
}

BattleScene::~BattleScene() {
    unloadOwnedRmlDocumentsNow();
}

bool BattleScene::init() {
    context_.getInputManager().pushContext(engine::input::InputContextId::Battle);
    context_pushed_ = true;

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
    if (auto* runtime = context_.getRmlUi(); runtime && document_) {
        runtime->queueFocusElementById(document_, "battle-action-attack");
    }
    return true;
}

void BattleScene::update(float delta_time) {
    Scene::update(delta_time);
    runStateMachine(delta_time);
    refreshView();
}

void BattleScene::clean() {
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool BattleScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("BattleScene: RmlUiRuntime 不可用。");
        return false;
    }

    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        spdlog::error("BattleScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("BattleScene: 创建 data model 失败。");
        return false;
    }

    if (!constructor.Bind("turn_text", &turn_text_) ||
        !constructor.Bind("units_text", &units_text_) ||
        !constructor.Bind("result_text", &result_text_) ||
        !constructor.Bind("actions_enabled", &actions_enabled_)) {
        spdlog::error("BattleScene: 绑定 data model 变量失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        spdlog::error("BattleScene: 加载 RML 文档失败。");
        data_bridge_.destroy();
        return false;
    }

    event_bridge_.on("attack", [this](Rml::Event&) { queueAttackAction(); });
    event_bridge_.on("skill", [this](Rml::Event&) { queueSkillAction(); });
    event_bridge_.on("item", [this](Rml::Event&) { queueItemAction(); });
    event_bridge_.on("guard", [this](Rml::Event&) { queueGuardAction(); });
    event_bridge_.on("escape", [this](Rml::Event&) { queueEscapeAction(); });
    event_bridge_.on("end_turn", [this](Rml::Event&) { queueEndTurnAction(); });
    event_bridge_.registerTo(document_, "click");

    data_bridge_.markAllDirty();
    return true;
}

void BattleScene::beforeUnloadOwnedRmlDocuments() {
    event_bridge_.unregisterAll();
}

void BattleScene::afterUnloadOwnedRmlDocuments() {
    document_ = nullptr;
    data_bridge_.destroy();
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

    std::string turn_text = "Turn: -";
    if (current_actor_id) {
        if (const auto* actor = session_.findUnit(*current_actor_id)) {
            turn_text = "Turn: " + actor->name + " (" + std::string(game::battle::toString(actor->side)) + ")";
        }
    }
    if (updateBoundString(turn_text_, turn_text)) {
        data_bridge_.markDirty("turn_text");
    }

    const std::string units_text = formatUnitsLine(units);
    if (updateBoundString(units_text_, units_text)) {
        data_bridge_.markDirty("units_text");
    }

    std::string result_text = "Result: Choose action";
    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        result_text = "Result: " + std::string(game::battle::toString(session_.outcome()));
    } else if (last_action_result_) {
        const auto& result = *last_action_result_;
        if (result.status == game::battle::BattleActionStatus::Rejected) {
            if (!result.failure_reason.empty()) {
                result_text = "Result: " + result.failure_reason;
            } else {
                result_text = "Result: Action rejected";
            }
        } else {
            switch (result.action_type) {
                case game::battle::BattleActionType::Attack: {
                    result_text = "Result: Attack dealt " + std::to_string(result.damage) + " dmg";
                    if (result.target_defeated) {
                        result_text += " (KO)";
                    }
                    break;
                }
                case game::battle::BattleActionType::Skill: {
                    result_text = "Result: Skill";
                    if (result.missed) {
                        result_text += " missed";
                    } else {
                        result_text += " dealt " + std::to_string(result.damage) + " dmg";
                        if (!result.states_added.empty()) {
                            result_text += " +" + result.states_added.front();
                        }
                    }
                    break;
                }
                case game::battle::BattleActionType::Item:
                    result_text = "Result: Item used";
                    break;
                case game::battle::BattleActionType::Guard:
                    result_text = "Result: Guarding";
                    break;
                case game::battle::BattleActionType::Escape:
                    result_text = result.escape_succeeded ? "Result: Escaped" : "Result: Escape failed";
                    break;
                case game::battle::BattleActionType::EndTurn:
                    result_text = "Result: Turn ended";
                    break;
            }
        }
    }
    if (updateBoundString(result_text_, result_text)) {
        data_bridge_.markDirty("result_text");
    }

    const bool can_submit_action =
        !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        current_actor_id.has_value();

    if (updateBoundBool(actions_enabled_, can_submit_action)) {
        data_bridge_.markDirty("actions_enabled");
    }
}

void BattleScene::queueAttackAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    const auto target_id = selectDefaultTarget(actor->side);
    if (!target_id) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Attack,
        .actor_id = actor_id,
        .target_id = target_id
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueSkillAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    const auto target_id = selectDefaultTarget(actor->side);
    if (!target_id) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Skill,
        .actor_id = actor_id,
        .target_id = target_id,
        .skill_id = std::string(kDefaultSkillId)
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueItemAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Item,
        .actor_id = actor_id,
        .target_id = std::nullopt,
        .item_id = std::string(kDefaultItemId)
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueGuardAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Guard,
        .actor_id = actor_id,
        .target_id = std::nullopt
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueEscapeAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::Escape,
        .actor_id = actor_id,
        .target_id = std::nullopt
    };
    state_ = FlowState::ExecutingAction;
}

void BattleScene::queueEndTurnAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    pending_action_ = game::battle::BattleAction{
        .type = game::battle::BattleActionType::EndTurn,
        .actor_id = actor_id,
        .target_id = std::nullopt
    };
    state_ = FlowState::ExecutingAction;
}

const game::battle::BattleUnit* BattleScene::prepareActionActor(game::battle::BattleUnitId& out_actor_id) const {
    if (state_ != FlowState::WaitingForInput || session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return nullptr;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id) {
        return nullptr;
    }

    const auto* actor = session_.findUnit(*actor_id);
    if (!actor) {
        return nullptr;
    }

    out_actor_id = *actor_id;
    return actor;
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
