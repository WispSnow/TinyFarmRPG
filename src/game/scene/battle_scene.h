#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/battle/battle_session.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace game::data {
class RpgCatalog;
struct SkillData;
enum class Scope : std::uint8_t;
} // namespace game::data

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

    enum class MenuState {
        None,
        MainMenu,
        SkillList,
        ItemList,
        TargetSelect
    };

    struct ActionDraft {
        game::battle::BattleActionType pending_type{game::battle::BattleActionType::EndTurn};
        std::optional<std::string> selected_skill_id{};
        std::optional<std::string> selected_item_id{};
        std::optional<game::battle::BattleUnitId> selected_target_id{};
        bool requires_target_selection{false};
    };

    struct MainActionViewModel {
        int action_id{0};
        int entry_index{0};
        Rml::String label{};
        bool enabled{false};
    };

    struct ListEntryViewModel {
        int entry_index{0};
        Rml::String entry_id{};
        Rml::String label{};
        Rml::String sublabel{};
        bool enabled{false};
    };

    struct TargetEntryViewModel {
        int entry_index{0};
        int unit_id{0};
        Rml::String label{};
        bool enabled{false};
        bool is_ally{false};
        bool is_dead{false};
    };

    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    game::battle::BattleSession session_;
    FlowState state_{FlowState::WaitingForInput};
    MenuState menu_state_{MenuState::MainMenu};
    ActionDraft action_draft_{};
    std::optional<game::battle::BattleAction> pending_action_{};
    std::optional<game::battle::BattleActionResult> last_action_result_{};
    float animation_timer_{0.0f};
    bool end_requested_{false};
    bool context_pushed_{false};
    bool input_listeners_connected_{false};
    bool menu_focus_dirty_{true};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    Rml::String turn_text_{"Turn: -"};
    Rml::String units_text_{"Units: -"};
    Rml::String result_text_{"Result: Choose action"};
    bool actions_enabled_{false};
    Rml::String menu_title_{"Actions"};
    Rml::String menu_hint_{"Choose an action."};
    Rml::String back_hint_{};
    Rml::String list_empty_text_{"No entries available"};
    Rml::String target_empty_text_{"No targets available"};
    bool main_menu_visible_{true};
    bool list_menu_visible_{false};
    bool target_menu_visible_{false};
    bool list_empty_{true};
    bool target_empty_{true};
    std::vector<MainActionViewModel> main_actions_{};
    std::vector<ListEntryViewModel> list_entries_{};
    std::vector<TargetEntryViewModel> target_entries_{};
    int main_action_cursor_{0};
    int list_entry_cursor_{-1};
    int target_entry_cursor_{-1};

public:
    BattleScene(std::string_view name,
                engine::core::Context& context,
                std::vector<game::battle::BattleUnit> units,
                game::battle::BattleSessionOptions session_options = {});
    ~BattleScene() override;

    bool init() override;
    void update(float delta_time) override;
    void prepareUi(float interpolation_alpha) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void connectInputListeners();
    void disconnectInputListeners();
    void runStateMachine(float delta_time);
    void refreshView();
    void refreshMenuEnabledState(bool enabled);
    void markMenuDirty();
    void enterInputMenu();
    void leaveInputMenu();
    void setMenuState(MenuState next_state);
    void syncMenuFocus();
    [[nodiscard]] bool focusElementById(std::string_view element_id);
    void populateMainActions();
    void enterListMenu(MenuState list_state);
    void populateSkillEntries(const game::battle::BattleUnit& actor);
    [[nodiscard]] const ListEntryViewModel* findListEntry(int entry_index) const;
    [[nodiscard]] bool isSkillEntryEnabled(const game::battle::BattleUnit& actor,
                                           const game::data::SkillData& skill) const;
    [[nodiscard]] Rml::String skillSubtitle(const game::battle::BattleUnit& actor,
                                            const game::data::SkillData& skill) const;
    [[nodiscard]] bool requiresTargetSelection(game::data::Scope scope) const;
    [[nodiscard]] int firstEnabledListEntryIndex() const;
    [[nodiscard]] MenuState menuStateForActionDraftSource() const;
    void enterTargetPlaceholder(std::string_view text);
    void handleMainAction(int entry_index);
    void handleListEntry(int entry_index);
    void handleSkillEntry(const ListEntryViewModel& entry);
    void handleTargetEntry(int entry_index);
    void submitAction(game::battle::BattleAction action);
    [[nodiscard]] bool isWaitingForActionInput() const;
    [[nodiscard]] bool moveMenuCursor(int delta);
    [[nodiscard]] bool moveCursorInEntries(int& cursor, int count, int step, const std::vector<bool>& enabled_entries);

    bool onMenuUpPressed();
    bool onMenuDownPressed();
    bool onMenuLeftPressed();
    bool onMenuRightPressed();
    bool onMenuConfirmPressed();
    bool onMenuCancelPressed();

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
