#include "battle_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Variant.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float RESULT_HOLD_SECONDS = 0.20f;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/battle.rml";
constexpr std::string_view MODEL_NAME = "battle_scene";
constexpr int MAIN_ACTION_COLUMNS = 3;

enum class MainActionId : int {
    Attack = 1,
    Skill = 2,
    Item = 3,
    Guard = 4,
    Escape = 5,
    EndTurn = 6
};

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

[[nodiscard]] std::string formatRecoveryText(const game::battle::BattleActionResult& result) {
    std::string text;
    if (result.hp_recovered > 0) {
        text = "recovered " + std::to_string(result.hp_recovered) + " HP";
    }
    if (result.mp_recovered > 0) {
        if (!text.empty()) {
            text += ", ";
            text += std::to_string(result.mp_recovered) + " MP";
        } else {
            text = "recovered " + std::to_string(result.mp_recovered) + " MP";
        }
    }
    return text;
}

[[nodiscard]] std::string formatActionResultText(const game::battle::BattleActionResult& result) {
    if (result.status == game::battle::BattleActionStatus::Rejected) {
        return result.failure_reason.empty() ? "Result: Action rejected" : "Result: " + result.failure_reason;
    }

    const std::string recovery_text = formatRecoveryText(result);
    switch (result.action_type) {
        case game::battle::BattleActionType::Attack: {
            std::string result_text = "Result: Attack dealt " + std::to_string(result.damage) + " dmg";
            if (result.target_defeated) {
                result_text += " (KO)";
            }
            return result_text;
        }
        case game::battle::BattleActionType::Skill: {
            std::string result_text = "Result: Skill";
            if (result.missed) {
                result_text += " missed";
                return result_text;
            }

            bool has_effect = false;
            if (result.damage > 0) {
                result_text += " dealt " + std::to_string(result.damage) + " dmg";
                has_effect = true;
            }
            if (!recovery_text.empty()) {
                result_text += has_effect ? ", " : " ";
                result_text += recovery_text;
                has_effect = true;
            }
            if (!result.states_added.empty()) {
                result_text += has_effect ? " " : " applied ";
                result_text += "+" + result.states_added.front();
                has_effect = true;
            }
            if (!has_effect) {
                result_text += " applied";
            }
            return result_text;
        }
        case game::battle::BattleActionType::Item:
            return recovery_text.empty() ? "Result: Item used" : "Result: Item " + recovery_text;
        case game::battle::BattleActionType::Guard:
            return "Result: Guarding";
        case game::battle::BattleActionType::Escape:
            return result.escape_succeeded ? "Result: Escaped" : "Result: Escape failed";
        case game::battle::BattleActionType::EndTurn:
            return "Result: Turn ended";
    }

    return "Result: Action applied";
}

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;
using namespace entt::literals;

[[nodiscard]] int getSingleIntArgument(const Rml::VariantList& arguments) {
    if (arguments.size() != 1) {
        return -1;
    }

    return arguments[0].Get<int>(-1);
}

[[nodiscard]] Rml::String makeElementId(std::string_view prefix, int index) {
    Rml::String element_id{prefix.data(), prefix.size()};
    element_id += std::to_string(index);
    return element_id;
}

[[nodiscard]] Rml::String makeRmlString(std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

} // namespace

namespace game::scene {

BattleScene::BattleScene(std::string_view name,
                         engine::core::Context& context,
                         std::vector<game::battle::BattleUnit> units,
                         game::battle::BattleSessionOptions session_options)
    : engine::scene::Scene(name, context),
      rpg_catalog_(session_options.rpg_catalog),
      item_catalog_(session_options.item_catalog),
      session_(std::move(units), std::move(session_options)) {
}

BattleScene::~BattleScene() {
    disconnectInputListeners();
    shutdownUI();
}

bool BattleScene::init() {
    context_.getInputManager().pushContext(engine::input::InputContextId::Battle);
    context_pushed_ = true;

    if (!initUI()) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
        return false;
    }

    if (!Scene::init()) {
        shutdownUI();
        context_.getInputManager().popContext();
        context_pushed_ = false;
        return false;
    }

    connectInputListeners();

    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        state_ = FlowState::BattleEnd;
        leaveInputMenu();
    } else {
        enterInputMenu();
    }
    refreshView();
    return true;
}

void BattleScene::update(float delta_time) {
    Scene::update(delta_time);
    runStateMachine(delta_time);
    refreshView();
}

void BattleScene::prepareUi(float interpolation_alpha) {
    Scene::prepareUi(interpolation_alpha);
    syncMenuFocus();
}

void BattleScene::clean() {
    disconnectInputListeners();
    shutdownUI();
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

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("BattleScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("BattleScene: 注册菜单 data types 失败。");
        document_controller_.unload();
        return false;
    }

    populateMainActions();

    if (!constructor.Bind("turn_text", &turn_text_) ||
        !constructor.Bind("units_text", &units_text_) ||
        !constructor.Bind("result_text", &result_text_) ||
        !constructor.Bind("actions_enabled", &actions_enabled_) ||
        !constructor.Bind("menu_title", &menu_title_) ||
        !constructor.Bind("menu_hint", &menu_hint_) ||
        !constructor.Bind("back_hint", &back_hint_) ||
        !constructor.Bind("list_empty_text", &list_empty_text_) ||
        !constructor.Bind("target_empty_text", &target_empty_text_) ||
        !constructor.Bind("main_menu_visible", &main_menu_visible_) ||
        !constructor.Bind("list_menu_visible", &list_menu_visible_) ||
        !constructor.Bind("target_menu_visible", &target_menu_visible_) ||
        !constructor.Bind("list_empty", &list_empty_) ||
        !constructor.Bind("target_empty", &target_empty_) ||
        !constructor.Bind("main_actions", &main_actions_) ||
        !constructor.Bind("list_entries", &list_entries_) ||
        !constructor.Bind("target_entries", &target_entries_)) {
        spdlog::error("BattleScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(
            constructor,
            "main_action_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleMainAction(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "list_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleListEntry(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "target_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleTargetEntry(getSingleIntArgument(arguments));
            })) {
        spdlog::error("BattleScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("BattleScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    document_controller_.markAllDirty();
    menu_focus_dirty_ = true;
    return true;
}

void BattleScene::shutdownUI() {
    document_controller_.unload();
}

bool BattleScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto action_handle = constructor.RegisterStruct<MainActionViewModel>()) {
        action_handle.RegisterMember("action_id", &MainActionViewModel::action_id);
        action_handle.RegisterMember("entry_index", &MainActionViewModel::entry_index);
        action_handle.RegisterMember("label", &MainActionViewModel::label);
        action_handle.RegisterMember("enabled", &MainActionViewModel::enabled);
    } else {
        return false;
    }

    if (auto entry_handle = constructor.RegisterStruct<ListEntryViewModel>()) {
        entry_handle.RegisterMember("entry_index", &ListEntryViewModel::entry_index);
        entry_handle.RegisterMember("entry_id", &ListEntryViewModel::entry_id);
        entry_handle.RegisterMember("label", &ListEntryViewModel::label);
        entry_handle.RegisterMember("sublabel", &ListEntryViewModel::sublabel);
        entry_handle.RegisterMember("enabled", &ListEntryViewModel::enabled);
    } else {
        return false;
    }

    if (auto target_handle = constructor.RegisterStruct<TargetEntryViewModel>()) {
        target_handle.RegisterMember("entry_index", &TargetEntryViewModel::entry_index);
        target_handle.RegisterMember("unit_id", &TargetEntryViewModel::unit_id);
        target_handle.RegisterMember("label", &TargetEntryViewModel::label);
        target_handle.RegisterMember("enabled", &TargetEntryViewModel::enabled);
        target_handle.RegisterMember("is_ally", &TargetEntryViewModel::is_ally);
        target_handle.RegisterMember("is_dead", &TargetEntryViewModel::is_dead);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(main_actions_)>() ||
        !constructor.RegisterArray<decltype(list_entries_)>() ||
        !constructor.RegisterArray<decltype(target_entries_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void BattleScene::connectInputListeners() {
    if (input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).connect<&BattleScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).connect<&BattleScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).connect<&BattleScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).connect<&BattleScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).connect<&BattleScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).connect<&BattleScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = true;
}

void BattleScene::disconnectInputListeners() {
    if (!input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).disconnect<&BattleScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).disconnect<&BattleScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).disconnect<&BattleScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).disconnect<&BattleScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).disconnect<&BattleScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).disconnect<&BattleScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = false;
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
                    enterInputMenu();
                    return;
                }

                last_action_result_ = session_.submitAction(*pending_action_);
                pending_action_.reset();
                animation_timer_ = RESULT_HOLD_SECONDS;
                leaveInputMenu();
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
                enterInputMenu();
                break;
            case FlowState::BattleEnd:
                leaveInputMenu();
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
        document_controller_.markDirty("turn_text");
    }

    const std::string units_text = formatUnitsLine(units);
    if (updateBoundString(units_text_, units_text)) {
        document_controller_.markDirty("units_text");
    }

    std::string result_text = "Result: Choose action";
    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        result_text = "Result: " + std::string(game::battle::toString(session_.outcome()));
    } else if (last_action_result_) {
        result_text = formatActionResultText(*last_action_result_);
    }
    if (updateBoundString(result_text_, result_text)) {
        document_controller_.markDirty("result_text");
    }

    const bool can_submit_action =
        !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        current_actor_id.has_value();

    if (updateBoundBool(actions_enabled_, can_submit_action)) {
        document_controller_.markDirty("actions_enabled");
    }

    refreshMenuEnabledState(can_submit_action);
    if (!can_submit_action && menu_state_ != MenuState::None) {
        leaveInputMenu();
    } else if (can_submit_action && menu_state_ == MenuState::None) {
        enterInputMenu();
    }
}

void BattleScene::refreshMenuEnabledState(bool enabled) {
    bool changed = false;
    for (auto& action : main_actions_) {
        if (action.enabled == enabled) {
            continue;
        }

        action.enabled = enabled;
        changed = true;
    }

    if (changed) {
        document_controller_.markDirty("main_actions");
        menu_focus_dirty_ = true;
    }
}

void BattleScene::markMenuDirty() {
    document_controller_.markDirty("menu_title");
    document_controller_.markDirty("menu_hint");
    document_controller_.markDirty("back_hint");
    document_controller_.markDirty("list_empty_text");
    document_controller_.markDirty("target_empty_text");
    document_controller_.markDirty("main_menu_visible");
    document_controller_.markDirty("list_menu_visible");
    document_controller_.markDirty("target_menu_visible");
    document_controller_.markDirty("list_empty");
    document_controller_.markDirty("target_empty");
    document_controller_.markDirty("main_actions");
    document_controller_.markDirty("list_entries");
    document_controller_.markDirty("target_entries");
}

void BattleScene::enterInputMenu() {
    action_draft_ = {};
    setMenuState(MenuState::MainMenu);
}

void BattleScene::leaveInputMenu() {
    action_draft_ = {};
    setMenuState(MenuState::None);
}

void BattleScene::setMenuState(MenuState next_state) {
    menu_state_ = next_state;
    main_menu_visible_ = next_state == MenuState::MainMenu;
    list_menu_visible_ = next_state == MenuState::SkillList || next_state == MenuState::ItemList;
    target_menu_visible_ = next_state == MenuState::TargetSelect;
    list_empty_ = list_entries_.empty();
    target_empty_ = target_entries_.empty();

    switch (next_state) {
        case MenuState::None:
            menu_title_ = "";
            menu_hint_ = "";
            back_hint_ = "";
            break;
        case MenuState::MainMenu:
            menu_title_ = "Actions";
            menu_hint_ = "Choose an action.";
            back_hint_ = "";
            main_action_cursor_ = main_actions_.empty()
                ? -1
                : std::clamp(main_action_cursor_, 0, static_cast<int>(main_actions_.size()) - 1);
            break;
        case MenuState::SkillList:
            menu_title_ = "Skills";
            menu_hint_ = "Choose a skill.";
            back_hint_ = "Cancel: Back";
            list_empty_text_ = "No skills available";
            list_entry_cursor_ = list_entries_.empty() ? -1 : std::clamp(list_entry_cursor_, 0, static_cast<int>(list_entries_.size()) - 1);
            break;
        case MenuState::ItemList:
            menu_title_ = "Items";
            menu_hint_ = "Choose an item.";
            back_hint_ = "Cancel: Back";
            list_empty_text_ = "No battle items available";
            list_entry_cursor_ = list_entries_.empty() ? -1 : std::clamp(list_entry_cursor_, 0, static_cast<int>(list_entries_.size()) - 1);
            break;
        case MenuState::TargetSelect:
            menu_title_ = "Targets";
            menu_hint_ = "Choose a target.";
            back_hint_ = "Cancel: Back";
            target_empty_text_ = "No targets available";
            target_entry_cursor_ = target_entries_.empty() ? -1 : std::clamp(target_entry_cursor_, 0, static_cast<int>(target_entries_.size()) - 1);
            break;
    }

    markMenuDirty();
    menu_focus_dirty_ = true;
}

void BattleScene::syncMenuFocus() {
    if (!menu_focus_dirty_) {
        return;
    }

    int cursor = -1;
    std::string_view prefix;

    switch (menu_state_) {
        case MenuState::None:
            menu_focus_dirty_ = false;
            return;
        case MenuState::MainMenu:
            cursor = main_action_cursor_;
            prefix = "battle-main-action-";
            break;
        case MenuState::SkillList:
        case MenuState::ItemList:
            cursor = list_entry_cursor_;
            prefix = "battle-list-entry-";
            break;
        case MenuState::TargetSelect:
            cursor = target_entry_cursor_;
            prefix = "battle-target-entry-";
            break;
    }

    // cursor < 0: 无可聚焦条目，直接清除脏标记。
    // cursor >= 0 且 focus 成功: 清除。
    // cursor >= 0 但元素尚未生成（data-if 子树未展开）: 保持脏标记，下帧重试。
    if (cursor < 0 || focusElementById(makeElementId(prefix, cursor))) {
        menu_focus_dirty_ = false;
    }
}

bool BattleScene::focusElementById(std::string_view element_id) {
    auto* document = document_controller_.document();
    if (!document) {
        return false;
    }

    auto* element = document->GetElementById(Rml::String{element_id.data(), element_id.size()});
    if (!element) {
        return false;
    }

    element->Focus(true);
    return true;
}

void BattleScene::populateMainActions() {
    const bool enabled = actions_enabled_;
    main_actions_ = {
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Attack), .entry_index = 0, .label = "Attack", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Skill), .entry_index = 1, .label = "Skill", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Item), .entry_index = 2, .label = "Item", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Guard), .entry_index = 3, .label = "Guard", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Escape), .entry_index = 4, .label = "Escape", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::EndTurn), .entry_index = 5, .label = "End Turn", .enabled = enabled},
    };
}

void BattleScene::populateSkillEntries(const game::battle::BattleUnit& actor) {
    list_entries_.clear();
    list_entry_cursor_ = -1;
    list_empty_text_ = "No skills available";

    if (!rpg_catalog_) {
        spdlog::warn("BattleScene: RPG catalog 不可用，无法生成技能列表。");
        return;
    }

    int entry_index = 0;
    for (const auto& skill_id : actor.skill_ids) {
        const auto* skill = rpg_catalog_->findSkill(skill_id);
        if (!skill) {
            spdlog::warn("BattleScene: skill '{}' 不存在于 RPG catalog，已跳过。", skill_id);
            continue;
        }

        const std::string_view label = skill->display_name_.empty()
            ? std::string_view{skill->id_}
            : std::string_view{skill->display_name_};
        list_entries_.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = skill->id_,
            .label = makeRmlString(label),
            .sublabel = skillSubtitle(actor, *skill),
            .enabled = isSkillEntryEnabled(actor, *skill)
        });
    }

    list_entry_cursor_ = firstEnabledListEntryIndex();
}

void BattleScene::populateItemEntries() {
    list_entries_.clear();
    list_entry_cursor_ = -1;
    list_empty_text_ = "No battle items available";

    if (!item_catalog_) {
        spdlog::warn("BattleScene: Item catalog 不可用，无法生成物品列表。");
        return;
    }

    const auto& item_stocks = session_.itemStocks();
    if (item_stocks.empty()) {
        return;
    }

    auto items = item_catalog_->listItems();
    std::sort(items.begin(), items.end(), [](const game::data::ItemData* lhs, const game::data::ItemData* rhs) {
        const std::string_view lhs_label = lhs && !lhs->display_name_.empty()
            ? std::string_view{lhs->display_name_}
            : (lhs ? std::string_view{lhs->id_str_} : std::string_view{});
        const std::string_view rhs_label = rhs && !rhs->display_name_.empty()
            ? std::string_view{rhs->display_name_}
            : (rhs ? std::string_view{rhs->id_str_} : std::string_view{});
        if (lhs_label == rhs_label) {
            return (lhs ? lhs->id_str_ : std::string{}) < (rhs ? rhs->id_str_ : std::string{});
        }
        return lhs_label < rhs_label;
    });

    int entry_index = 0;
    for (const auto* item : items) {
        if (!item || item->id_str_.empty() || !item->battle_use_) {
            continue;
        }

        const auto stock_it = item_stocks.find(item->id_);
        if (stock_it == item_stocks.end() || stock_it->second <= 0) {
            continue;
        }

        const std::string_view label = item->display_name_.empty()
            ? std::string_view{item->id_str_}
            : std::string_view{item->display_name_};
        list_entries_.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = item->id_str_,
            .label = makeRmlString(label),
            .sublabel = itemSubtitle(stock_it->second, *item->battle_use_),
            .enabled = isItemEntryEnabled(stock_it->second, *item->battle_use_)
        });
    }

    list_entry_cursor_ = firstEnabledListEntryIndex();
}

const BattleScene::ListEntryViewModel* BattleScene::findListEntry(int entry_index) const {
    const auto it = std::find_if(
        list_entries_.begin(),
        list_entries_.end(),
        [entry_index](const ListEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == list_entries_.end() ? nullptr : &*it;
}

bool BattleScene::isSkillEntryEnabled(const game::battle::BattleUnit& actor,
                                      const game::data::SkillData& skill) const {
    return actor.mp >= skill.mp_cost_ && skill.scope_ != game::data::Scope::None;
}

Rml::String BattleScene::skillSubtitle(const game::battle::BattleUnit& actor,
                                       const game::data::SkillData& skill) const {
    std::string subtitle = "MP " + std::to_string(skill.mp_cost_);
    if (actor.mp < skill.mp_cost_) {
        subtitle += " / Low MP";
    }
    return subtitle;
}

const game::data::ItemData* BattleScene::findBattleItemByEntryId(std::string_view entry_id,
                                                                 int* out_stock_count) const {
    if (out_stock_count) {
        *out_stock_count = 0;
    }
    if (!item_catalog_ || entry_id.empty()) {
        return nullptr;
    }

    const entt::id_type item_id = game::data::RpgCatalog::hashId(entry_id);
    if (out_stock_count) {
        if (const auto stock_it = session_.itemStocks().find(item_id); stock_it != session_.itemStocks().end()) {
            *out_stock_count = stock_it->second;
        }
    }
    return item_catalog_->findItem(item_id);
}

bool BattleScene::isItemEntryEnabled(int stock_count, const game::data::BattleItemUseConfig& use) const {
    return stock_count >= std::max(1, use.consume) && use.scope != game::data::Scope::None;
}

Rml::String BattleScene::itemSubtitle(int stock_count, const game::data::BattleItemUseConfig& use) const {
    std::string subtitle = "x" + std::to_string(std::max(0, stock_count));
    if (use.consume > 1) {
        subtitle += " / Use " + std::to_string(use.consume);
    }
    if (stock_count < std::max(1, use.consume)) {
        subtitle += " / Low Stock";
    }
    return subtitle;
}

bool BattleScene::requiresTargetSelection(game::data::Scope scope) const {
    return scope == game::data::Scope::OneEnemy || scope == game::data::Scope::OneAlly;
}

int BattleScene::firstEnabledListEntryIndex() const {
    for (const auto& entry : list_entries_) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return list_entries_.empty() ? -1 : list_entries_.front().entry_index;
}

void BattleScene::populateTargetEntries(game::data::Scope scope, const game::battle::BattleUnit& actor) {
    target_entries_.clear();
    target_entry_cursor_ = -1;
    target_empty_text_ = "No valid targets";

    int entry_index = 0;
    for (const auto& unit : session_.units()) {
        const bool matches_scope = (scope == game::data::Scope::OneEnemy && unit.side != actor.side) ||
            (scope == game::data::Scope::OneAlly && unit.side == actor.side);
        if (!matches_scope) {
            continue;
        }

        target_entries_.push_back(TargetEntryViewModel{
            .entry_index = entry_index++,
            .unit_id = static_cast<int>(unit.id),
            .label = targetLabel(unit),
            .enabled = unit.isAlive(),
            .is_ally = unit.side == actor.side,
            .is_dead = !unit.isAlive()
        });
    }

    target_entry_cursor_ = firstEnabledTargetEntryIndex();
}

const BattleScene::TargetEntryViewModel* BattleScene::findTargetEntry(int entry_index) const {
    const auto it = std::find_if(
        target_entries_.begin(),
        target_entries_.end(),
        [entry_index](const TargetEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == target_entries_.end() ? nullptr : &*it;
}

int BattleScene::firstEnabledTargetEntryIndex() const {
    for (const auto& entry : target_entries_) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return target_entries_.empty() ? -1 : target_entries_.front().entry_index;
}

Rml::String BattleScene::targetLabel(const game::battle::BattleUnit& unit) const {
    std::string label = unit.name + " HP " + std::to_string(unit.hp) + "/" + std::to_string(unit.max_hp);
    if (!unit.isAlive()) {
        label += " (KO)";
    }
    return label;
}

BattleScene::MenuState BattleScene::menuStateForActionDraftSource() const {
    switch (action_draft_.pending_type) {
        case game::battle::BattleActionType::Skill:
            return MenuState::SkillList;
        case game::battle::BattleActionType::Item:
            return MenuState::ItemList;
        case game::battle::BattleActionType::Attack:
        case game::battle::BattleActionType::Guard:
        case game::battle::BattleActionType::Escape:
        case game::battle::BattleActionType::EndTurn:
            return MenuState::MainMenu;
    }
    return MenuState::MainMenu;
}

void BattleScene::setMenuHint(std::string_view text) {
    menu_hint_ = makeRmlString(text);
    document_controller_.markDirty("menu_hint");
}

void BattleScene::continueDraftAfterScopeSelected(game::data::Scope scope, const game::battle::BattleUnit& actor) {
    action_draft_.requires_target_selection = requiresTargetSelection(scope);
    action_draft_.selected_target_id.reset();

    switch (scope) {
        case game::data::Scope::OneEnemy:
        case game::data::Scope::OneAlly:
            populateTargetEntries(scope, actor);
            setMenuState(MenuState::TargetSelect);
            return;
        case game::data::Scope::Self:
        case game::data::Scope::AllEnemies:
        case game::data::Scope::AllAllies:
            (void)submitDraftAction();
            return;
        case game::data::Scope::None:
            setMenuHint("Action cannot be used.");
            return;
    }
}

void BattleScene::handleMainAction(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(main_actions_.size())) {
        return;
    }

    main_action_cursor_ = entry_index;
    menu_focus_dirty_ = true;
    const auto& action = main_actions_[entry_index];
    if (!action.enabled) {
        return;
    }

    switch (static_cast<MainActionId>(action.action_id)) {
        case MainActionId::Attack:
            queueAttackAction();
            break;
        case MainActionId::Skill:
            queueSkillAction();
            break;
        case MainActionId::Item:
            queueItemAction();
            break;
        case MainActionId::Guard:
            queueGuardAction();
            break;
        case MainActionId::Escape:
            queueEscapeAction();
            break;
        case MainActionId::EndTurn:
            queueEndTurnAction();
            break;
    }
}

void BattleScene::handleListEntry(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(list_entries_.size())) {
        return;
    }

    const auto* entry = findListEntry(entry_index);
    if (!entry) {
        return;
    }

    list_entry_cursor_ = entry->entry_index;
    menu_focus_dirty_ = true;
    if (!entry->enabled) {
        return;
    }

    if (menu_state_ == MenuState::SkillList) {
        handleSkillEntry(*entry);
    } else if (menu_state_ == MenuState::ItemList) {
        handleItemEntry(*entry);
    }
}

void BattleScene::handleSkillEntry(const ListEntryViewModel& entry) {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor || !rpg_catalog_) {
        return;
    }

    const auto* skill = rpg_catalog_->findSkill(entry.entry_id);
    if (!skill || !isSkillEntryEnabled(*actor, *skill)) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Skill,
        .selected_skill_id = skill->id_,
        .selected_item_id = std::nullopt,
        .selected_target_id = std::nullopt
    };
    continueDraftAfterScopeSelected(skill->scope_, *actor);
}

void BattleScene::handleItemEntry(const ListEntryViewModel& entry) {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    int stock_count = 0;
    const auto* item = findBattleItemByEntryId(entry.entry_id, &stock_count);
    if (!item || !item->battle_use_ || !isItemEntryEnabled(stock_count, *item->battle_use_)) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Item,
        .selected_skill_id = std::nullopt,
        .selected_item_id = item->id_str_,
        .selected_target_id = std::nullopt
    };
    continueDraftAfterScopeSelected(item->battle_use_->scope, *actor);
}

void BattleScene::handleTargetEntry(int entry_index) {
    if (!isWaitingForActionInput()) {
        return;
    }

    const auto* entry = findTargetEntry(entry_index);
    if (!entry) {
        return;
    }

    target_entry_cursor_ = entry->entry_index;
    menu_focus_dirty_ = true;
    if (!entry->enabled) {
        return;
    }

    action_draft_.selected_target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
    (void)submitDraftAction();
}

bool BattleScene::submitDraftAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        setMenuHint("Action is no longer available.");
        return false;
    }

    if (action_draft_.requires_target_selection && !action_draft_.selected_target_id) {
        setMenuHint("Choose a target.");
        return false;
    }

    switch (action_draft_.pending_type) {
        case game::battle::BattleActionType::Attack:
            if (!action_draft_.selected_target_id) {
                setMenuHint("Choose a target.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Attack,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id
            });
            return true;
        case game::battle::BattleActionType::Skill:
            if (!action_draft_.selected_skill_id) {
                setMenuHint("Action is no longer available.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Skill,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id,
                .skill_id = *action_draft_.selected_skill_id
            });
            return true;
        case game::battle::BattleActionType::Item:
            if (!action_draft_.selected_item_id) {
                setMenuHint("Action is no longer available.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Item,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id,
                .item_id = *action_draft_.selected_item_id
            });
            return true;
        case game::battle::BattleActionType::Guard:
        case game::battle::BattleActionType::Escape:
        case game::battle::BattleActionType::EndTurn:
            setMenuHint("Action is no longer available.");
            return false;
    }

    return false;
}

void BattleScene::submitAction(game::battle::BattleAction action) {
    pending_action_ = std::move(action);
    leaveInputMenu();
    state_ = FlowState::ExecutingAction;
}

bool BattleScene::isWaitingForActionInput() const {
    return !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        session_.currentActorId().has_value();
}

bool BattleScene::moveMenuCursor(int delta) {
    if (!isWaitingForActionInput() || delta == 0) {
        return false;
    }

    switch (menu_state_) {
        case MenuState::MainMenu: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(main_actions_.size());
            for (const auto& action : main_actions_) {
                enabled_entries.push_back(action.enabled);
            }

            if (!moveCursorInEntries(main_action_cursor_, static_cast<int>(main_actions_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::SkillList:
        case MenuState::ItemList: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(list_entries_.size());
            for (const auto& entry : list_entries_) {
                enabled_entries.push_back(entry.enabled);
            }

            if (!moveCursorInEntries(list_entry_cursor_, static_cast<int>(list_entries_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::TargetSelect: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(target_entries_.size());
            for (const auto& target : target_entries_) {
                enabled_entries.push_back(target.enabled);
            }

            if (!moveCursorInEntries(target_entry_cursor_, static_cast<int>(target_entries_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::None:
            return false;
    }
}

bool BattleScene::moveCursorInEntries(int& cursor, int count, int step, const std::vector<bool>& enabled_entries) {
    if (count <= 0 || enabled_entries.empty()) {
        return false;
    }

    const int start = cursor >= 0 && cursor < count ? cursor : 0;
    for (int offset = 1; offset <= count; ++offset) {
        const int raw_candidate = start + step * offset;
        const int candidate = (raw_candidate % count + count) % count;
        if (candidate >= static_cast<int>(enabled_entries.size()) || !enabled_entries[candidate]) {
            continue;
        }

        if (candidate == cursor) {
            return false;
        }

        cursor = candidate;
        return true;
    }

    return false;
}

bool BattleScene::onMenuUpPressed() {
    const bool moved = moveMenuCursor(menu_state_ == MenuState::MainMenu ? -MAIN_ACTION_COLUMNS : -1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuDownPressed() {
    const bool moved = moveMenuCursor(menu_state_ == MenuState::MainMenu ? MAIN_ACTION_COLUMNS : 1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuLeftPressed() {
    const bool moved = moveMenuCursor(-1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuRightPressed() {
    const bool moved = moveMenuCursor(1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuConfirmPressed() {
    if (!isWaitingForActionInput()) {
        return menu_state_ != MenuState::None;
    }

    switch (menu_state_) {
        case MenuState::MainMenu:
            handleMainAction(main_action_cursor_);
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            handleListEntry(list_entry_cursor_);
            return true;
        case MenuState::TargetSelect:
            handleTargetEntry(target_entry_cursor_);
            return true;
        case MenuState::None:
            return false;
    }
}

bool BattleScene::onMenuCancelPressed() {
    if (!isWaitingForActionInput()) {
        return menu_state_ != MenuState::None;
    }

    switch (menu_state_) {
        case MenuState::TargetSelect:
            action_draft_.selected_target_id.reset();
            setMenuState(menuStateForActionDraftSource());
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            action_draft_ = {};
            setMenuState(MenuState::MainMenu);
            return true;
        case MenuState::MainMenu:
            return true;
        case MenuState::None:
            return false;
    }
}

void BattleScene::queueAttackAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Attack,
        .requires_target_selection = true
    };
    continueDraftAfterScopeSelected(game::data::Scope::OneEnemy, *actor);
}

void BattleScene::queueSkillAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Skill,
        .requires_target_selection = true
    };
    populateSkillEntries(*actor);
    setMenuState(MenuState::SkillList);
}

void BattleScene::queueItemAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Item,
        .requires_target_selection = false
    };
    populateItemEntries();
    setMenuState(MenuState::ItemList);
}

void BattleScene::queueGuardAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::Guard,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
}

void BattleScene::queueEscapeAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::Escape,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
}

void BattleScene::queueEndTurnAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::EndTurn,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
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

void BattleScene::requestBattleEnd() {
    if (end_requested_) {
        return;
    }

    end_requested_ = true;

    game::defs::BattleEndedEvent event{};
    event.outcome = session_.outcome();
    event.final_units = session_.units();
    event.remaining_item_stocks = session_.itemStocks();
    context_.getDispatcher().trigger(event);

    requestPopScene();
}

} // namespace game::scene
