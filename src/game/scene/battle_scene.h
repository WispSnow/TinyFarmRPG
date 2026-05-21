#pragma once

#include "engine/scene/scene.h"
#include "engine/system/render_system.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/utils/events.h"
#include "engine/utils/math.h"
#include "engine/vfx/vfx_types.h"
#include "game/battle/battle_log_formatter.h"
#include "game/battle/battle_reward_resolver.h"
#include "game/battle/battle_session.h"
#include "game/scene/battle_action_presentation_plan.h"
#include "game/scene/battle_animation_director.h"
#include "game/scene/battle_background.h"
#include "game/scene/battle_damage_popup_controller.h"
#include "game/scene/battle_scene_types.h"
#include "game/scene/battle_victory_flow_controller.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
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

namespace game::defs {
struct BattleAnimationSpeedChangedEvent;
struct DamagePopupVisibilityChangedEvent;
struct EnemyHpBarVisibilityChangedEvent;
struct CursorMemoryChangedEvent;
} // namespace game::defs

namespace game::scene {

/// @brief 回合制战斗表现层场景。
///
/// BattleScene 负责 UI、输入与状态机编排；领域规则通过 BattleSession 访问，
/// 不直接修改 TurnCore。该场景以 push/pop 方式叠加在探索场景之上，战斗结束后
/// 发出 BattleEndedEvent 并请求弹出自身。
class BattleScene final : public engine::scene::Scene {
    /// @brief 单帧同步推进的战斗流程状态。
    enum class FlowState {
        WaitingForInput,    ///< 等待玩家输入行动。
        ExecutingAction,    ///< 将 pending_action_ 提交给 BattleSession。
        AnimatingResult,    ///< 展示动作结果；当前以短计时占位。
        CheckVictory,       ///< 根据 BattleActionResult::outcome_after 判断是否结束战斗。
        VictoryFlow,        ///< 展示 Victory 结算流程，等待玩家确认后退出。
        NextTurn,           ///< 刷新到下一个行动者并路由到玩家输入或敌方自动行动。
        BattleEnd           ///< 发送结算事件并请求弹出场景。
    };

    /// @brief 当前菜单上下文，用于区分主菜单、技能列表、道具列表和目标选择等不同输入语义。
    enum class MenuState {
        None,
        PartyCommand,
        ActorCommand,
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

    /// @brief 进入战斗前的相机状态，BattleScene 退出时恢复探索态相机。
    struct CameraStateSnapshot {
        glm::vec2 position{0.0F};
        float zoom{1.0F};
        float rotation{0.0F};
        float min_zoom{0.1F};
        float max_zoom{10.0F};
        std::optional<engine::utils::Rect> limit_bounds{};
    };

    struct ScheduledPresentationEvent {
        std::variant<engine::vfx::PlayVfxCommand, engine::utils::PlaySoundEvent, game::battle::BattleActionResult> payload{};
        float remaining_seconds{0.0F};
    };

    /// @brief 战斗命令单项的表现层视图模型。
    ///
    /// 该结构体只服务于 RmlUi 数据绑定，用于描述 PartyCommand / ActorCommand 条目
    /// 的文本、顺序与可选状态。
    struct CommandViewModel {
        int command_id{0};
        int entry_index{0};
        Rml::String label{};
        bool enabled{false};

        friend bool operator==(const CommandViewModel& lhs, const CommandViewModel& rhs) = default;
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
        Rml::String sublabel{};
        bool enabled{false};
        bool is_ally{false};
        bool is_dead{false};
    };

    /// @brief 下方 HUD 中单个队友状态条目的视图模型。
    struct PartyStatusViewModel {
        int unit_id{0};
        Rml::String name{};
        Rml::String hp_text{};
        Rml::String mp_text{};
        Rml::String hp_ratio_percent{"0%"};
        Rml::String mp_ratio_percent{"0%"};
        Rml::String portrait_decorator{"none"};
        bool active{false};
        bool ko{false};
    };

    /// @brief 下方 HUD 中单个状态图标的扁平视图模型。
    struct StateIconViewModel {
        int unit_id{0};
        int entry_index{0};
        Rml::String state_id{};
        Rml::String display_name{};
        Rml::String description{};
        Rml::String turns_text{};
        Rml::String short_label{};
        Rml::String icon_decorator{"none"};
        bool known{false};

        friend bool operator==(const StateIconViewModel& lhs, const StateIconViewModel& rhs) = default;
    };

    /// @brief 状态图标 hover tooltip 的轻量视图模型。
    struct StateTooltipViewModel {
        int active_unit_id{0};
        Rml::String title{};
        Rml::String turns{};
        Rml::String description{};
        bool visible{false};

        friend bool operator==(const StateTooltipViewModel& lhs, const StateTooltipViewModel& rhs) = default;
    };

    /// @brief 滚动战斗日志中单行的 RmlUi 表现层视图模型。
    struct BattleLogEntryViewModel {
        Rml::String text{};
        Rml::String tone_class{};

        friend bool operator==(const BattleLogEntryViewModel& lhs, const BattleLogEntryViewModel& rhs) = default;
    };

    /// @brief Victory overlay 中单个掉落条目的视图模型。
    struct VictoryRewardItemViewModel {
        int entry_index{0};
        Rml::String label{};
        Rml::String count_text{};
        Rml::String icon_decorator{"none"};

        friend bool operator==(const VictoryRewardItemViewModel& lhs, const VictoryRewardItemViewModel& rhs) = default;
    };

    /// @brief Victory overlay 中单个升级条目的视图模型。
    struct VictoryLevelUpViewModel {
        int entry_index{0};
        Rml::String label{};
        Rml::String stat_text{};

        friend bool operator==(const VictoryLevelUpViewModel& lhs, const VictoryLevelUpViewModel& rhs) = default;
    };

    /// @brief 顶部行动顺序条中单个单位的只读表现层条目。
    struct TurnOrderEntryViewModel {
        int unit_id{0};
        int entry_index{0};
        Rml::String name{};
        Rml::String short_label{};
        Rml::String badge_label{};
        Rml::String portrait_decorator{"none"};
        bool current{false};
        bool acted{false};
        bool ko{false};
        bool enemy{false};

        friend bool operator==(const TurnOrderEntryViewModel& lhs, const TurnOrderEntryViewModel& rhs) = default;
    };

    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
    const game::factory::BlueprintManager* blueprint_manager_{nullptr};
    const game::data::AppearanceCatalog* appearance_catalog_{nullptr};
    engine::vfx::VfxService* vfx_service_{nullptr};
    game::battle::BattleSession session_;
    BattleScenePresentationOptions presentation_options_{};
    BattleEnemyHpBarController battle_enemy_hp_bar_controller_{};
    BattleBackgroundRenderer battle_background_{};
    entt::registry battle_registry_{};
    engine::system::RenderSystem battle_render_system_{};
    FlowState state_{FlowState::WaitingForInput};
    MenuState menu_state_{MenuState::None};
    ActionDraft action_draft_{};
    std::optional<game::battle::BattleAction> pending_action_{};
    std::optional<game::battle::BattleActionResult> last_action_result_{};
    BattleAnimationDirector battle_animation_director_{};
    BattleDamagePopupController battle_damage_popup_controller_{};
    BattleVictoryFlowController victory_flow_controller_{};
    std::optional<game::battle::BattleRewardSummary> victory_reward_summary_{};
    std::optional<game::battle::BattleUnitId> command_focus_actor_id_{};
    float command_focus_elapsed_seconds_{0.0f};
    std::optional<std::uint32_t> party_command_accepted_round_{}; ///< 当前轮已通过 PartyCommand 时记录轮次，用于同轮后续玩家行动者跳过 Fight/Escape 询问。
    bool actor_command_entered_via_fight_this_step_{false};       ///< 仅表示当前 ActorCommand 由 Fight 直入且尚未选择角色命令，控制取消是否回退到 PartyCommand。
    bool end_requested_{false};
    bool context_pushed_{false};
    bool input_listeners_connected_{false};
    bool menu_focus_dirty_{true};
    std::optional<CameraStateSnapshot> saved_camera_state_{};
    std::vector<ScheduledPresentationEvent> scheduled_presentation_events_{};

    /// @brief 玩家偏好的战斗动画速度（来自 UserSettingsService）；用于 animationConfigForPlan 中缩放 *_seconds。
    float battle_animation_speed_{1.0f};

    /// @brief 是否启用光标记忆。
    bool cursor_memory_enabled_{true};
    /// @brief 每个行动者上次选择的 ActorCommand 下标（按 BattleUnitId 键存储）。战斗结束清空。
    std::unordered_map<game::battle::BattleUnitId, int> last_actor_command_index_per_actor_{};
    /// @brief 每个行动者上次选中的 skill id（list_entries_.entry_id）。
    std::unordered_map<game::battle::BattleUnitId, std::string> last_skill_id_per_actor_{};
    /// @brief 每个行动者上次选中的 item id（list_entries_.entry_id）。
    std::unordered_map<game::battle::BattleUnitId, std::string> last_item_id_per_actor_{};
    /// @brief 每个行动者上次选中的 target unit id。
    std::unordered_map<game::battle::BattleUnitId, game::battle::BattleUnitId> last_target_unit_id_per_actor_{};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    Rml::String turn_text_{"Turn: -"};
    Rml::String result_text_{"Result: Choose action"};
    bool actions_enabled_{false};
    std::string menu_status_text_{"Choose action"};
    Rml::String list_empty_text_{"No entries available"};
    Rml::String target_empty_text_{"No targets available"};
    bool party_command_visible_{false};
    bool actor_command_visible_{false};
    bool list_menu_visible_{false};
    bool target_menu_visible_{false};
    bool list_empty_{true};
    bool target_empty_{true};
    std::vector<TurnOrderEntryViewModel> turn_order_entries_{};
    std::vector<PartyStatusViewModel> party_status_{};
    std::vector<StateIconViewModel> party_state_icons_{};
    StateTooltipViewModel state_tooltip_{};
    int state_tooltip_entry_index_{-1};
    std::vector<game::battle::BattleLogLine> battle_log_history_{};
    std::vector<BattleLogEntryViewModel> battle_log_entries_{};
    std::vector<VictoryRewardItemViewModel> victory_reward_items_{};
    std::vector<VictoryLevelUpViewModel> victory_level_ups_{};
    std::vector<CommandViewModel> party_commands_{};
    std::vector<CommandViewModel> actor_commands_{};
    std::vector<ListEntryViewModel> list_entries_{};
    std::vector<TargetEntryViewModel> target_entries_{};
    int party_command_cursor_{0};
    int actor_command_cursor_{0};
    int list_entry_cursor_{-1};
    int target_entry_cursor_{-1};
    bool victory_overlay_visible_{false};
    bool victory_continue_enabled_{false};
    bool victory_continue_focus_dirty_{false};
    bool victory_items_empty_{true};
    bool victory_level_ups_empty_{true};
    Rml::String victory_title_{"Victory!"};
    Rml::String victory_gold_text_{"0"};
    Rml::String victory_exp_text_{"0"};
    Rml::String victory_item_empty_text_{"No drops"};
    Rml::String victory_prompt_text_{"Confirm"};

public:
    /// @brief 构造战斗场景。
    /// @param name 场景名称。
    /// @param context 引擎上下文。
    /// @param units 初始战斗单位。
    /// @param session_options BattleSession 使用的技能/道具目录与库存依赖。
    BattleScene(std::string_view name,
                engine::core::Context& context,
                std::vector<game::battle::BattleUnit> units,
                game::battle::BattleSessionOptions session_options = {},
                BattleScenePresentationOptions presentation_options = {});
    ~BattleScene() override;

    bool init() override;
    void update(float delta_time) override;
    void render(float interpolation_alpha) override;
    void prepareUi(float interpolation_alpha) override;
    void clean() override;
    [[nodiscard]] engine::scene::SceneUiCoverage uiCoverage() const override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);

    void connectInputListeners();
    void disconnectInputListeners();
    void enterBattleCamera();
    void restoreBattleCamera();

    /// @brief 运行同步战斗流程状态机，直到进入需要等待的状态。
    void runStateMachine(float delta_time);
    void beginCurrentTurnFlow();
    [[nodiscard]] const game::battle::BattleUnit* currentActor() const;
    [[nodiscard]] game::battle::BattleAction buildEnemyAction(const game::battle::BattleUnit& actor) const;

    /// @brief 根据 BattleSession 快照刷新 UI 文本和按钮状态。
    void refreshView();
    void rebuildTurnOrderView();
    void rebuildPartyStatusView();
    void rebuildVictoryView();
    void hideStateTooltip();
    void appendBattleLogLines(const std::vector<game::battle::BattleLogLine>& lines);
    void rebuildBattleLogView();
    [[nodiscard]] Rml::String battleLogToneClass(game::battle::BattleLogTone tone) const;
    void refreshMenuEnabledState(bool enabled);
    void markMenuDirty();
    void enterInputMenu();
    void leaveInputMenu();
    void setMenuState(MenuState next_state);
    void syncMenuFocus();
    void syncVictoryContinueFocus();
    [[nodiscard]] bool focusElementById(std::string_view element_id);

    /// @brief 当前玩家行动机会是否应先显示队伍命令层。
    [[nodiscard]] bool shouldOpenPartyCommand() const;

    /// @brief 重建 Fight / Escape 队伍命令条目。
    void populatePartyCommands();

    /// @brief 重建 Attack / Skill / Guard / Item 角色命令条目。
    void populateActorCommands();
    void populateSkillEntries(const game::battle::BattleUnit& actor);
    void populateItemEntries();
    [[nodiscard]] const CommandViewModel* findPartyCommand(int entry_index) const;
    [[nodiscard]] const CommandViewModel* findActorCommand(int entry_index) const;
    [[nodiscard]] const ListEntryViewModel* findListEntry(int entry_index) const;
    [[nodiscard]] int firstEnabledPartyCommandIndex() const;
    [[nodiscard]] int firstEnabledActorCommandIndex() const;
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
    [[nodiscard]] Rml::String targetSublabel(const game::battle::BattleUnit& unit) const;
    [[nodiscard]] MenuState menuStateForActionDraftSource() const;
    void setMenuHint(std::string_view text);
    void continueDraftAfterScopeSelected(game::data::Scope scope, const game::battle::BattleUnit& actor);
    void handlePartyCommand(int entry_index);
    void handleActorCommand(int entry_index);
    void handleListEntry(int entry_index);
    void handleSkillEntry(const ListEntryViewModel& entry);
    void handleItemEntry(const ListEntryViewModel& entry);
    void handleTargetEntry(int entry_index);
    void handleStateIconHoverEnter(int unit_id, int entry_index);
    void handleStateIconHoverExit(int unit_id, int entry_index);
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

    /// @brief 获取当前行动者并写出 actor id。
    /// @return 当前行动者存在时返回单位指针，否则返回 nullptr。
    [[nodiscard]] const game::battle::BattleUnit* prepareActionActor(game::battle::BattleUnitId& out_actor_id) const;
    void beginVictoryFlow();
    [[nodiscard]] game::battle::BattleRewardSummary resolveVictoryRewards();
    void finishVictoryFlow();
    void playVictoryAudioCue();
    [[nodiscard]] std::vector<BattlePresentationUnitAnchor> collectBattlePresentationUnitAnchors() const;
    [[nodiscard]] BattleAnimationTimelineConfig animationConfigForPlan(
        const BattleActionPresentationPlan& plan) const;
    [[nodiscard]] BattleActionPresentationPlan presentationPlanForResult(
        const game::battle::BattleActionResult& result,
        const std::vector<BattlePresentationUnitAnchor>& unit_anchors) const;
    [[nodiscard]] glm::vec2 actionStartOffsetFor(game::battle::BattleUnitId actor_id) const;

    void schedulePresentationPlanEvents(const BattleActionPresentationPlan& plan,
                                        const game::battle::BattleActionResult& result);
    void schedulePresentationEvent(engine::vfx::PlayVfxCommand command, float fire_time_seconds);
    void schedulePresentationEvent(engine::utils::PlaySoundEvent event, float fire_time_seconds);
    void scheduleEnemyHpRevealEvent(game::battle::BattleActionResult result, float fire_time_seconds);
    void updateScheduledPresentationEvents(float delta_time);
    void updateCommandFocus(float delta_time);
    [[nodiscard]] std::optional<BattleAnimationPose> commandFocusPoseFor(game::battle::BattleUnitId unit_id,
                                                                         game::battle::BattleSide side) const;
    [[nodiscard]] std::optional<BattleAnimationPose> presentationPoseFor(game::battle::BattleUnitId unit_id,
                                                                         game::battle::BattleSide side) const;

    void requestBattleEnd();

    [[nodiscard]] bool initPresentation();
    void updatePresentation(float delta_time);
    void refreshPresentation();
    void syncPresentationTransforms();
    void syncPresentationShadows();
    void syncEnemyHpBarHighlight();
    void renderEnemyHpBars();
    void renderDamagePopups();
    void renderBattlefieldBackground();

    /// @brief 从 UserSettingsService 同步当前偏好到本地缓存与子控制器。
    void syncUserSettingsState();
    void connectUserSettingsListeners();
    void disconnectUserSettingsListeners();
    void onBattleAnimationSpeedChanged(const game::defs::BattleAnimationSpeedChangedEvent& evt);
    void onDamagePopupVisibilityChanged(const game::defs::DamagePopupVisibilityChangedEvent& evt);
    void onEnemyHpBarVisibilityChanged(const game::defs::EnemyHpBarVisibilityChangedEvent& evt);
    void onCursorMemoryChanged(const game::defs::CursorMemoryChangedEvent& evt);
};

} // namespace game::scene
