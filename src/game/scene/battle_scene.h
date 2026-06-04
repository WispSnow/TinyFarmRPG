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
#include "game/scene/battle_flow_controller.h"
#include "game/scene/battle_input_router.h"
#include "game/scene/battle_menu_model.h"
#include "game/scene/battle_scene_state.h"
#include "game/scene/battle_scene_types.h"
#include "game/scene/battle_scene_view_models.h"
#include "game/scene/battle_view_model_builder.h"
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
struct LanguageChangedEvent;
} // namespace game::defs

namespace game::runtime {
class LocalizationService;
} // namespace game::runtime

namespace game::scene {

/// @brief 回合制战斗表现层场景。
///
/// BattleScene 负责 UI、输入与状态机编排；领域规则通过 BattleSession 访问，
/// 不直接修改 TurnCore。该场景以 push/pop 方式叠加在探索场景之上，战斗结束后
/// 发出 BattleEndedEvent 并请求弹出自身。
class BattleScene final : public engine::scene::Scene,
                          private BattleFlowController::Delegate,
                          private BattleInputRouter::Delegate {
    using MenuState = BattleMenuState;
    using ActionDraft = BattleActionDraft;
    using CameraStateSnapshot = BattleCameraStateSnapshot;
    using ScheduledPresentationEvent = BattleScheduledPresentationEvent;
    using CommandViewModel = BattleCommandViewModel;
    using ListEntryViewModel = BattleListEntryViewModel;
    using TargetEntryViewModel = BattleTargetEntryViewModel;
    using PartyStatusViewModel = BattlePartyStatusViewModel;
    using StateIconViewModel = BattleStateIconViewModel;
    using StateTooltipViewModel = BattleStateTooltipViewModel;
    using BattleLogEntryViewModel = game::scene::BattleLogEntryViewModel;
    using VictoryRewardItemViewModel = BattleVictoryRewardItemViewModel;
    using VictoryLevelUpViewModel = BattleVictoryLevelUpViewModel;
    using TurnOrderEntryViewModel = BattleTurnOrderEntryViewModel;

    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
    const game::factory::BlueprintManager* blueprint_manager_{nullptr};
    const game::data::AppearanceCatalog* appearance_catalog_{nullptr};
    engine::vfx::VfxService* vfx_service_{nullptr};
    BattleViewModelBuilder view_model_builder_;
    game::battle::BattleSession session_;
    BattleScenePresentationOptions presentation_options_{};
    BattleEnemyHpBarController battle_enemy_hp_bar_controller_{};
    BattleBackgroundRenderer battle_background_{};
    entt::registry battle_registry_{};
    engine::system::RenderSystem battle_render_system_{};
    BattleFlowController flow_controller_{};
    BattleInputRouter input_router_{};
    BattleMenuModel menu_model_{};
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
    std::optional<CameraStateSnapshot> saved_camera_state_{};
    std::vector<ScheduledPresentationEvent> scheduled_presentation_events_{};

    /// @brief 玩家偏好的战斗动画速度（来自 UserSettingsService）；用于 animationConfigForPlan 中缩放 *_seconds。
    float battle_animation_speed_{1.0f};

    /// @brief 是否启用光标记忆。
    bool cursor_memory_enabled_{true};
    /// @brief 每个行动者上次选择的 ActorCommand 下标（按 BattleUnitId 键存储）。战斗结束清空。
    std::unordered_map<game::battle::BattleUnitId, int> last_actor_command_index_per_actor_{};
    /// @brief 每个行动者上次选中的 skill id（BattleMenuModel::list_entries.entry_id）。
    std::unordered_map<game::battle::BattleUnitId, std::string> last_skill_id_per_actor_{};
    /// @brief 每个行动者上次选中的 item id（BattleMenuModel::list_entries.entry_id）。
    std::unordered_map<game::battle::BattleUnitId, std::string> last_item_id_per_actor_{};
    /// @brief 每个行动者上次选中的 target unit id。
    std::unordered_map<game::battle::BattleUnitId, game::battle::BattleUnitId> last_target_unit_id_per_actor_{};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    std::vector<TurnOrderEntryViewModel> turn_order_entries_{};
    std::vector<PartyStatusViewModel> party_status_{};
    std::vector<StateIconViewModel> party_state_icons_{};
    StateTooltipViewModel state_tooltip_{};
    int state_tooltip_entry_index_{-1};
    std::vector<game::battle::BattleLogLine> battle_log_history_{};
    std::vector<BattleLogEntryViewModel> battle_log_entries_{};
    std::vector<VictoryRewardItemViewModel> victory_reward_items_{};
    std::vector<VictoryLevelUpViewModel> victory_level_ups_{};
    bool victory_overlay_visible_{false};
    bool victory_continue_enabled_{false};
    bool victory_continue_focus_dirty_{false};
    bool victory_items_empty_{true};
    bool victory_level_ups_empty_{true};
    Rml::String victory_title_{"!battle.victory.title!"};
    Rml::String victory_gold_text_{"0"};
    Rml::String victory_exp_text_{"0"};
    Rml::String victory_item_empty_text_{"!battle.victory.no_drops!"};
    Rml::String victory_prompt_text_{"!common.confirm!"};
    bool defeat_overlay_visible_{false};
    bool defeat_continue_enabled_{false};
    bool defeat_continue_focus_dirty_{false};
    bool defeat_flow_finished_{false};
    Rml::String defeat_title_{"!battle.defeat.title!"};
    Rml::String defeat_body_text_{"!battle.defeat.body!"};
    Rml::String defeat_losses_text_{"!battle.defeat.losses!"};
    Rml::String defeat_recovery_text_{"!battle.defeat.recovery!"};
    Rml::String defeat_prompt_text_{"!battle.defeat.continue!"};

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
    [[nodiscard]] const game::runtime::LocalizationService* localization() const noexcept;

    void connectInputListeners();
    void disconnectInputListeners();
    void enterBattleCamera();
    void restoreBattleCamera();

    /// @brief 运行同步战斗流程状态机，直到进入需要等待的状态。
    void runStateMachine(float delta_time);
    [[nodiscard]] bool hasPendingAction() const override;
    void executePendingAction() override;
    void beginCurrentTurnFlow() override;
    [[nodiscard]] const game::battle::BattleUnit* currentActor() const;
    [[nodiscard]] game::battle::BattleAction buildEnemyAction(const game::battle::BattleUnit& actor) const;
    void emitBattleTurnStarted(const game::battle::BattleUnit& unit);
    void emitBattleActionScriptEvents(const game::battle::BattleActionResult& result,
                                      const std::vector<game::battle::BattleUnit>& before_units,
                                      std::uint32_t round_index);
    void updateResultAnimation(float delta_time) override;
    [[nodiscard]] bool resultAnimationFinished() const override;
    [[nodiscard]] game::battle::BattleOutcome battleOutcome() const override;

    /// @brief 根据 BattleSession 快照刷新 UI 文本和按钮状态。
    void refreshView();
    void rebuildTurnOrderView();
    void rebuildPartyStatusView();
    void rebuildVictoryView();
    void rebuildDefeatView();
    void hideStateTooltip();
    void appendBattleLogLines(const std::vector<game::battle::BattleLogLine>& lines);
    void rebuildBattleLogView();
    void refreshMenuEnabledState(bool enabled);
    void markMenuDirty();
    void enterInputMenu();
    void leaveInputMenu() override;
    void setMenuState(MenuState next_state);
    void syncMenuStateText();
    void syncMenuFocus();
    void syncVictoryContinueFocus();
    void syncDefeatContinueFocus();
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
    [[nodiscard]] std::string localizedBattleSide(game::battle::BattleSide side) const;
    [[nodiscard]] std::string localizedBattleOutcome(game::battle::BattleOutcome outcome) const;
    [[nodiscard]] Rml::String targetLabel(const game::battle::BattleUnit& unit) const;
    [[nodiscard]] Rml::String targetSublabel(const game::battle::BattleUnit& unit) const;
    [[nodiscard]] MenuState menuStateForActionDraftSource() const;
    void setMenuHint(std::string_view text);
    void setMenuHintKey(std::string_view key, std::string_view fallback);
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
    [[nodiscard]] BattleMenuState battleMenuState() const override;
    [[nodiscard]] bool moveMenuCursor(int delta);
    [[nodiscard]] bool moveBattleMenuCursor(int delta) override;
    [[nodiscard]] bool moveCursorInEntries(int& cursor, int count, int step, const std::vector<bool>& enabled_entries);
    bool confirmBattleMenu() override;
    bool cancelBattleMenu() override;

    /// @brief 构造并排队普通攻击行动。
    void queueAttackAction();
    void queueSkillAction();
    void queueItemAction();
    void queueGuardAction();
    void queueEscapeAction();

    /// @brief 获取当前行动者并写出 actor id。
    /// @return 当前行动者存在时返回单位指针，否则返回 nullptr。
    [[nodiscard]] const game::battle::BattleUnit* prepareActionActor(game::battle::BattleUnitId& out_actor_id) const;
    void beginVictoryFlow() override;
    [[nodiscard]] game::battle::BattleRewardSummary resolveVictoryRewards();
    void updateVictoryFlow(float delta_time) override;
    [[nodiscard]] bool victoryFlowFinished() const override;
    void finishVictoryFlow() override;
    void playVictoryAudioCue();
    void beginDefeatFlow() override;
    void updateDefeatFlow(float delta_time) override;
    [[nodiscard]] bool defeatFlowFinished() const override;
    void finishDefeatFlow() override;
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

    void requestBattleEnd() override;

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
    void publishWebBattleDiagnostics() const;
    void connectUserSettingsListeners();
    void disconnectUserSettingsListeners();
    void onBattleAnimationSpeedChanged(const game::defs::BattleAnimationSpeedChangedEvent& evt);
    void onDamagePopupVisibilityChanged(const game::defs::DamagePopupVisibilityChangedEvent& evt);
    void onEnemyHpBarVisibilityChanged(const game::defs::EnemyHpBarVisibilityChangedEvent& evt);
    void onCursorMemoryChanged(const game::defs::CursorMemoryChangedEvent& evt);
    void onLanguageChanged(const game::defs::LanguageChangedEvent& evt);
};

} // namespace game::scene
