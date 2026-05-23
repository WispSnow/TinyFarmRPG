#include "rest_dialog_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/rest_dialog.rml";
constexpr std::string_view MODEL_NAME = "rest_dialog";

using engine::ui::rmlui::updateBoundString;
using engine::ui::rmlui::updateBoundBool;

[[nodiscard]] Rml::String makeRmlString(const std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] std::string formatRestStatLine(const std::string_view label,
                                             const int current,
                                             const int after,
                                             const int max_value) {
    if (current == after) {
        return fmt::format("{} {}/{}", label, after, max_value);
    }
    return fmt::format("{} {}>{}/{}", label, current, after, max_value);
}

} // namespace

namespace game::scene {

using namespace entt::literals;

RestDialogScene::RestDialogScene(std::string_view name,
                                 engine::core::Context& context,
                                 const entt::entity player,
                                 std::vector<game::domain::RestRecoveryPreview> recovery_previews)
    : engine::scene::Scene(name, context),
      previous_state_(context.getGameState().getCurrentState()),
      player_(player),
      recovery_previews_(std::move(recovery_previews)) {
}

RestDialogScene::~RestDialogScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool RestDialogScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Dialogue);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&RestDialogScene::onMenuCancelPressed>(this);
    if (!Scene::init()) {
        return false;
    }

    return true;
}

void RestDialogScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool RestDialogScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("RestDialogScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("RestDialogScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("RestDialogScene: 注册恢复预览 data types 失败。");
        document_controller_.unload();
        return false;
    }

    if (!constructor.Bind("hours_text", &hours_text_) ||
        !constructor.Bind("recovery_summary_text", &recovery_summary_text_) ||
        !constructor.Bind("has_recovery_members", &has_recovery_members_) ||
        !constructor.Bind("recovery_empty_text", &recovery_empty_text_) ||
        !constructor.Bind("recovery_members", &recovery_members_)) {
        spdlog::error("RestDialogScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindSimpleEvent(constructor, "hours_down", [this] { adjustHours(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "hours_up", [this] { adjustHours(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "confirm", [this] { onConfirm(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "cancel", [this] { onCancel(); })) {
        spdlog::error("RestDialogScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("RestDialogScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    updateHoursLabel();
    refreshRecoveryPreview();
    return true;
}

bool RestDialogScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto member_handle = constructor.RegisterStruct<RestRecoveryMemberViewModel>()) {
        member_handle.RegisterMember("display_name", &RestRecoveryMemberViewModel::display_name);
        member_handle.RegisterMember("hp_text", &RestRecoveryMemberViewModel::hp_text);
        member_handle.RegisterMember("mp_text", &RestRecoveryMemberViewModel::mp_text);
        member_handle.RegisterMember("hp_delta_text", &RestRecoveryMemberViewModel::hp_delta_text);
        member_handle.RegisterMember("mp_delta_text", &RestRecoveryMemberViewModel::mp_delta_text);
        member_handle.RegisterMember("has_hp_gain", &RestRecoveryMemberViewModel::has_hp_gain);
        member_handle.RegisterMember("has_mp_gain", &RestRecoveryMemberViewModel::has_mp_gain);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(recovery_members_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void RestDialogScene::shutdownUI() {
    document_controller_.unload();
}

void RestDialogScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&RestDialogScene::onMenuCancelPressed>(this);
}

void RestDialogScene::updateHoursLabel() {
    const auto text = std::to_string(selected_hours_) + "h";
    if (updateBoundString(hours_text_, text)) {
        document_controller_.markDirty("hours_text");
    }
}

const game::domain::RestRecoveryPreview* RestDialogScene::currentRecoveryPreview() const {
    const auto it = std::find_if(
        recovery_previews_.begin(),
        recovery_previews_.end(),
        [this](const game::domain::RestRecoveryPreview& preview) {
            return preview.hours == selected_hours_;
        });
    return it == recovery_previews_.end() ? nullptr : &*it;
}

void RestDialogScene::refreshRecoveryPreview() {
    recovery_members_.clear();

    const auto* preview = currentRecoveryPreview();
    std::string summary = "Recovery unavailable";
    std::string empty_text = "Party status unavailable.";

    if (preview) {
        if (preview->members.empty()) {
            summary = "No active party";
            empty_text = "No active party members.";
        } else if (!preview->anyRecovered()) {
            summary = "Already rested";
        } else if (preview->full_recovery) {
            summary = "Full recovery";
        } else {
            summary = fmt::format("HP/MP +{}%", preview->recovery_percent);
        }

        recovery_members_.reserve(preview->members.size());
        for (const auto& member : preview->members) {
            RestRecoveryMemberViewModel view_model{};
            view_model.display_name = makeRmlString(member.display_name);
            view_model.hp_text = makeRmlString(
                formatRestStatLine("HP", member.current_hp, member.after_hp, member.max_hp));
            view_model.mp_text = makeRmlString(
                formatRestStatLine("MP", member.current_mp, member.after_mp, member.max_mp));
            view_model.has_hp_gain = member.hpGain() > 0;
            view_model.has_mp_gain = member.mpGain() > 0;
            view_model.hp_delta_text = view_model.has_hp_gain
                ? makeRmlString(fmt::format("+{}", member.hpGain()))
                : Rml::String{};
            view_model.mp_delta_text = view_model.has_mp_gain
                ? makeRmlString(fmt::format("+{}", member.mpGain()))
                : Rml::String{};
            recovery_members_.push_back(std::move(view_model));
        }
    }

    if (updateBoundString(recovery_summary_text_, summary)) {
        document_controller_.markDirty("recovery_summary_text");
    }
    if (updateBoundString(recovery_empty_text_, empty_text)) {
        document_controller_.markDirty("recovery_empty_text");
    }
    if (updateBoundBool(has_recovery_members_, !recovery_members_.empty())) {
        document_controller_.markDirty("has_recovery_members");
    }
    document_controller_.markDirty("recovery_members");
}

void RestDialogScene::adjustHours(int delta) {
    const int next = std::clamp(selected_hours_ + delta, MIN_REST_HOURS, MAX_REST_HOURS);
    if (next == selected_hours_) {
        return;
    }

    selected_hours_ = next;
    updateHoursLabel();
    refreshRecoveryPreview();
}

bool RestDialogScene::onMenuCancelPressed() {
    onCancel();
    return true;
}

void RestDialogScene::onConfirm() {
    context_.getDispatcher().trigger(game::defs::RestConfirmRequest{
        .player = player_,
        .hours = selected_hours_,
    });
    requestPopScene();
}

void RestDialogScene::onCancel() {
    requestPopScene();
}

} // namespace game::scene
