#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/battle/battle_session.h"

#include <RmlUi/Core/Types.h>

#include <optional>
#include <string_view>
#include <vector>

namespace game::scene {

class BattleScene final : public engine::scene::Scene {
    enum class FlowState {
        WaitingForInput,
        ExecutingAction,
        AnimatingResult,
        CheckVictory,
        NextTurn,
        BattleEnd
    };

    game::battle::BattleSession session_;
    FlowState state_{FlowState::WaitingForInput};
    std::optional<game::battle::BattleAction> pending_action_{};
    std::optional<game::battle::BattleActionResult> last_action_result_{};
    float animation_timer_{0.0f};
    bool end_requested_{false};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String turn_text_{"Turn: -"};
    Rml::String units_text_{"Units: -"};
    Rml::String result_text_{"Result: Choose action"};
    bool actions_enabled_{false};

public:
    BattleScene(std::string_view name,
                engine::core::Context& context,
                std::vector<game::battle::BattleUnit> units,
                game::battle::BattleSessionOptions session_options = {});
    ~BattleScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void runStateMachine(float delta_time);
    void refreshView();

    void queueAttackAction();
    void queueSkillAction();
    void queueItemAction();
    void queueGuardAction();
    void queueEscapeAction();
    void queueEndTurnAction();
    [[nodiscard]] const game::battle::BattleUnit* prepareActionActor(game::battle::BattleUnitId& out_actor_id) const;
    [[nodiscard]] std::optional<game::battle::BattleUnitId> selectDefaultTarget(game::battle::BattleSide actor_side) const;

    void requestBattleEnd();
};

} // namespace game::scene
