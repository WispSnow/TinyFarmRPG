#pragma once

#include "engine/scene/scene.h"
#include "game/battle/battle_session.h"

#include <optional>
#include <string_view>
#include <vector>

namespace engine::ui {
class UIButton;
class UILabel;
class UIPanel;
class UIInputBlocker;
} // namespace engine::ui

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

    engine::ui::UIPanel* dim_{nullptr};
    engine::ui::UIInputBlocker* input_blocker_{nullptr};
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* turn_label_{nullptr};
    engine::ui::UILabel* units_label_{nullptr};
    engine::ui::UILabel* result_label_{nullptr};
    engine::ui::UIButton* attack_button_{nullptr};
    engine::ui::UIButton* skill_button_{nullptr};
    engine::ui::UIButton* item_button_{nullptr};
    engine::ui::UIButton* guard_button_{nullptr};
    engine::ui::UIButton* escape_button_{nullptr};
    engine::ui::UIButton* end_turn_button_{nullptr};

public:
    BattleScene(std::string_view name,
                engine::core::Context& context,
                std::vector<game::battle::BattleUnit> units,
                game::battle::BattleSessionOptions session_options = {});
    ~BattleScene() override = default;

    bool init() override;
    void update(float delta_time) override;

private:
    [[nodiscard]] bool initUI();
    void buildLayout();
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
