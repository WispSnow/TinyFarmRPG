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
class ItemCatalog;
class RpgCatalog;
struct BattleItemUseConfig;
struct ItemData;
struct SkillData;
enum class Scope : std::uint8_t;
} // namespace game::data

namespace game::scene {

/// @brief 回合制战斗表现层场景。
///
/// BattleScene 负责 UI、输入与状态机编排；领域规则通过 BattleSession 访问，
/// 不直接修改 TurnCore。该场景以 push/pop 方式叠加在探索场景之上，战斗结束后
/// 发出 BattleEndedEvent 并请求弹出自身。
class BattleScene final : public engine::scene::Scene {
    /// @brief 单帧同步推进的战斗流程状态。
    enum class FlowState {
        WaitingForInput,    ///< 等待玩家或未来 AI 提交行动。
        ExecutingAction,    ///< 将 pending_action_ 提交给 BattleSession。
        AnimatingResult,    ///< 展示动作结果；当前以短计时占位。
        CheckVictory,       ///< 根据 BattleActionResult::outcome_after 判断是否结束战斗。
        NextTurn,           ///< 刷新到下一个行动者并回到输入等待。
        BattleEnd           ///< 发送结算事件并请求弹出场景。
    };

    /// @brief 当前菜单上下文，用于区分主菜单、技能列表、道具列表和目标选择等不同输入语义。
    enum class MenuState {
        None,
        MainMenu,
        SkillList,
        ItemList,
        TargetSelect
    };

    /// @brief 记录玩家在菜单中逐步拼装中的行动草稿。
    ///
    /// BattleScene 会先记录动作类型、技能/道具来源与已选目标，再在信息足够时
    /// 组装为最终 BattleAction 并提交给 BattleSession。
    struct ActionDraft {
        game::battle::BattleActionType pending_type{game::battle::BattleActionType::EndTurn};
        std::optional<std::string> selected_skill_id{};
        std::optional<std::string> selected_item_id{};
        std::optional<game::battle::BattleUnitId> selected_target_id{};
        bool requires_target_selection{false};
    };

    /// @brief 主动作菜单单项的表现层视图模型。
    ///
    /// 该结构体只服务于 RmlUi 数据绑定，用于描述 Attack/Skill/Item 等顶层菜单项
    /// 的文本、顺序与可选状态。
    struct MainActionViewModel {
        int action_id{0};
        int entry_index{0};
        Rml::String label{};
        bool enabled{false};
    };

    /// @brief 技能列表或道具列表共用的条目视图模型。
    ///
    /// BattleScene 会按当前菜单上下文生成这类条目，供 RmlUi 渲染可滚动列表并在
    /// 选择后回查对应技能或道具。
    struct ListEntryViewModel {
        int entry_index{0};
        Rml::String entry_id{};
        Rml::String label{};
        Rml::String sublabel{};
        bool enabled{false};
    };

    /// @brief 目标选择菜单中单个战斗单位的视图模型。
    ///
    /// 该结构体把 BattleUnit 转换为 UI 需要的目标条目数据，供玩家在选择技能或
    /// 道具作用对象时展示阵营、死亡状态与标签文本。
    struct TargetEntryViewModel {
        int entry_index{0};
        int unit_id{0};
        Rml::String label{};
        bool enabled{false};
        bool is_ally{false};
        bool is_dead{false};
    };

    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
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
    /// @brief 构造战斗场景。
    /// @param name 场景名称。
    /// @param context 引擎上下文。
    /// @param units 初始战斗单位。
    /// @param session_options BattleSession 使用的技能/道具目录与库存依赖。
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

    /// @brief 运行同步战斗流程状态机，直到进入需要等待的状态。
    void runStateMachine(float delta_time);

    /// @brief 根据 BattleSession 快照刷新 UI 文本和按钮状态。
    void refreshView();
    void refreshMenuEnabledState(bool enabled);
    void markMenuDirty();
    void enterInputMenu();
    void leaveInputMenu();
    void setMenuState(MenuState next_state);
    void syncMenuFocus();
    [[nodiscard]] bool focusElementById(std::string_view element_id);
    void populateMainActions();
    void populateSkillEntries(const game::battle::BattleUnit& actor);
    void populateItemEntries();
    [[nodiscard]] const ListEntryViewModel* findListEntry(int entry_index) const;
    [[nodiscard]] bool isSkillEntryEnabled(const game::battle::BattleUnit& actor,
                                           const game::data::SkillData& skill) const;
    [[nodiscard]] Rml::String skillSubtitle(const game::battle::BattleUnit& actor,
                                            const game::data::SkillData& skill) const;
    [[nodiscard]] const game::data::ItemData* findBattleItemByEntryId(std::string_view entry_id,
                                                                      int* out_stock_count = nullptr) const;
    [[nodiscard]] bool isItemEntryEnabled(int stock_count, const game::data::BattleItemUseConfig& use) const;
    [[nodiscard]] Rml::String itemSubtitle(int stock_count, const game::data::BattleItemUseConfig& use) const;
    [[nodiscard]] bool requiresTargetSelection(game::data::Scope scope) const;
    [[nodiscard]] int firstEnabledListEntryIndex() const;
    void populateTargetEntries(game::data::Scope scope, const game::battle::BattleUnit& actor);
    [[nodiscard]] const TargetEntryViewModel* findTargetEntry(int entry_index) const;
    [[nodiscard]] int firstEnabledTargetEntryIndex() const;
    [[nodiscard]] Rml::String targetLabel(const game::battle::BattleUnit& unit) const;
    [[nodiscard]] MenuState menuStateForActionDraftSource() const;
    void setMenuHint(std::string_view text);
    void continueDraftAfterScopeSelected(game::data::Scope scope, const game::battle::BattleUnit& actor);
    void handleMainAction(int entry_index);
    void handleListEntry(int entry_index);
    void handleSkillEntry(const ListEntryViewModel& entry);
    void handleItemEntry(const ListEntryViewModel& entry);
    void handleTargetEntry(int entry_index);
    [[nodiscard]] bool submitDraftAction();
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

    /// @brief 构造并排队普通攻击行动。
    void queueAttackAction();
    void queueSkillAction();
    void queueItemAction();
    void queueGuardAction();
    void queueEscapeAction();
    void queueEndTurnAction();

    /// @brief 获取当前行动者并写出 actor id。
    /// @return 当前行动者存在时返回单位指针，否则返回 nullptr。
    [[nodiscard]] const game::battle::BattleUnit* prepareActionActor(game::battle::BattleUnitId& out_actor_id) const;

    void requestBattleEnd();
};

} // namespace game::scene
