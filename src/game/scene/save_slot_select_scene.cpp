#include "save_slot_select_scene.h"

#include "game/save/save_service.h"
#include "game/save/save_slot_summary.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

using namespace entt::literals;

namespace {

constexpr int SLOT_COUNT = 10;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/save_slot_select.rml";
constexpr std::string_view MODEL_NAME = "save_slot_select";

[[nodiscard]] bool tryToLocalTm(std::time_t value, std::tm& out) {
#if defined(_WIN32)
    return localtime_s(&out, &value) == 0;
#else
    return localtime_r(&value, &out) != nullptr;
#endif
}

[[nodiscard]] std::string formatTimestampForDisplay(std::string_view timestamp) {
    if (timestamp.empty()) {
        return {};
    }

    const char* begin = timestamp.data();
    const char* end = timestamp.data() + timestamp.size();
    std::chrono::seconds::rep epoch_seconds{0};
    const auto [ptr, ec] = std::from_chars(begin, end, epoch_seconds);
    if (ec != std::errc{} || ptr != end || epoch_seconds < 0) {
        return {};
    }

    const auto time_point = std::chrono::sys_seconds{std::chrono::seconds{epoch_seconds}};
    const std::time_t time_value = std::chrono::system_clock::to_time_t(time_point);
    std::tm tm{};
    if (!tryToLocalTm(time_value, tm)) {
        return {};
    }

    return fmt::format("{:%y-%m-%d %H:%M}", tm);
}

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

SaveSlotSelectScene::SaveSlotSelectScene(std::string_view name,
                                         engine::core::Context& context,
                                         SlotSelectCallback on_select,
                                         Mode mode)
    : engine::scene::Scene(name, context),
      on_select_(std::move(on_select)),
      mode_(mode) {
}

SaveSlotSelectScene::~SaveSlotSelectScene() {
    disconnectRuntimeListeners();
    if (document_ || data_bridge_.isValid()) {
        if (document_) {
            unloadAllRmlDocuments();
            document_ = nullptr;
        }
        data_bridge_.destroy();
    }
}

bool SaveSlotSelectScene::init() {
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs).connect<&SaveSlotSelectScene::onMenuCancelPressed>(this);
    if (!Scene::init()) {
        return false;
    }
    return true;
}

void SaveSlotSelectScene::clean() {
    disconnectRuntimeListeners();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
    document_ = nullptr;
    focus_before_confirm_ = nullptr;
    data_bridge_.destroy();
}

void SaveSlotSelectScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&SaveSlotSelectScene::onMenuCancelPressed>(this);
}

bool SaveSlotSelectScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    static std::unordered_set<Rml::DataTypeRegister*> registered_type_registers;

    auto* type_register = constructor.GetDataTypeRegister();
    if (!type_register) {
        return false;
    }
    if (registered_type_registers.contains(type_register)) {
        return true;
    }

    if (auto slot_handle = constructor.RegisterStruct<SlotViewModel>()) {
        slot_handle.RegisterMember("slot_index", &SlotViewModel::slot_index);
        slot_handle.RegisterMember("label", &SlotViewModel::label);
        slot_handle.RegisterMember("enabled", &SlotViewModel::enabled);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(slots_)>()) {
        return false;
    }

    registered_type_registers.insert(type_register);
    return true;
}

bool SaveSlotSelectScene::initUI() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        spdlog::error("SaveSlotSelectScene: RmlUILayer 不可用。");
        return false;
    }

    auto* rml_context = layer->getContext();
    if (!rml_context) {
        spdlog::error("SaveSlotSelectScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("SaveSlotSelectScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("SaveSlotSelectScene: 注册 slot data types 失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.Bind("slots", &slots_) ||
        !constructor.Bind("confirm_visible", &confirm_visible_) ||
        !constructor.Bind("confirm_text", &confirm_text_)) {
        spdlog::error("SaveSlotSelectScene: 绑定 data model 变量失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.BindEventCallback("slot_select", &SaveSlotSelectScene::onSlotSelectEvent, this) ||
        !constructor.BindEventCallback("back", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { onBackClicked(); }) ||
        !constructor.BindEventCallback("confirm_yes", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { onOverwriteConfirmYes(); }) ||
        !constructor.BindEventCallback("confirm_no", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { onOverwriteConfirmNo(); })) {
        spdlog::error("SaveSlotSelectScene: 绑定 data event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("SaveSlotSelectScene: 加载 RML 文档失败。");
        return false;
    }

    refreshSlotButtons();
    data_bridge_.markAllDirty();
    queueDefaultFocus();
    return true;
}

void SaveSlotSelectScene::queueDefaultFocus() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer || !document_) {
        return;
    }

    const bool has_enabled_slot = std::any_of(slots_.begin(), slots_.end(), [](const SlotViewModel& slot) {
        return slot.enabled;
    });
    if (has_enabled_slot) {
        layer->queueFocusFirstEnabledElementByClass(document_, "save-slot-button");
    } else {
        layer->queueFocusElementById(document_, "save-slot-back");
    }
}

void SaveSlotSelectScene::refreshSlotButtons() {
    slots_.clear();
    slots_.reserve(SLOT_COUNT);

    for (int i = 0; i < SLOT_COUNT; ++i) {
        const auto path = game::save::SaveService::slotPath(i);
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);

        std::string label_text;
        bool enabled = true;
        if (ec) {
            label_text = "Error";
            enabled = false;
        } else if (!exists) {
            label_text = "Empty";
            enabled = (mode_ == Mode::Save);
        } else {
            std::string summary_error;
            if (const auto summary = game::save::tryReadSlotSummary(path, summary_error)) {
                label_text = "Day " + std::to_string(summary->day);
                if (!summary->timestamp.empty()) {
                    const auto formatted = formatTimestampForDisplay(summary->timestamp);
                    label_text += " - " + (formatted.empty() ? summary->timestamp : formatted);
                }
            } else {
                label_text = "Invalid";
                spdlog::warn("SaveSlotSelectScene: slot {} summary 读取失败: {}", i, summary_error);
            }
        }

        SlotViewModel slot{};
        slot.slot_index = i;
        slot.label = Rml::String{label_text.data(), label_text.size()};
        slot.enabled = enabled;
        slots_.push_back(std::move(slot));
    }

    data_bridge_.markDirty("slots");
}

void SaveSlotSelectScene::onSlotClicked(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) {
        spdlog::warn("SaveSlotSelectScene: 非法 slot {}", slot);
        return;
    }

    if (!slots_.empty()) {
        const std::size_t index = static_cast<std::size_t>(slot);
        if (index < slots_.size() && !slots_[index].enabled) {
            return;
        }
    }

    if (mode_ == Mode::Save) {
        const auto path = game::save::SaveService::slotPath(slot);
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            spdlog::warn("SaveSlotSelectScene: slot {} 状态读取失败: {}", slot, ec.message());
            return;
        }
        if (exists) {
            showOverwriteConfirm(slot);
            return;
        }
    }

    if (on_select_) {
        on_select_(slot);
        return;
    }
    spdlog::warn("SaveSlotSelectScene: 未设置 on_select 回调，忽略 slot {}", slot);
}

void SaveSlotSelectScene::onBackClicked() {
    if (confirm_visible_) {
        hideOverwriteConfirm();
        return;
    }
    requestPopScene();
}

bool SaveSlotSelectScene::onMenuCancelPressed() {
    if (confirm_visible_) {
        hideOverwriteConfirm();
        return true;
    }
    requestPopScene();
    return true;
}

void SaveSlotSelectScene::showOverwriteConfirm(int slot) {
    pending_overwrite_slot_ = slot;
    if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
        focus_before_confirm_ = layer->getFocusedElement();
        if (focus_before_confirm_ && focus_before_confirm_->GetOwnerDocument() != document_) {
            focus_before_confirm_ = nullptr;
        }
    }

    const auto text = "Overwrite slot " + std::to_string(slot + 1) + "?";
    if (updateBoundString(confirm_text_, text)) {
        data_bridge_.markDirty("confirm_text");
    }
    if (updateBoundBool(confirm_visible_, true)) {
        data_bridge_.markDirty("confirm_visible");
    }
    if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
        layer->queueFocusElementById(document_, "save-slot-confirm-yes");
    }
}

void SaveSlotSelectScene::hideOverwriteConfirm() {
    pending_overwrite_slot_.reset();
    if (updateBoundBool(confirm_visible_, false)) {
        data_bridge_.markDirty("confirm_visible");
    }
    if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
        if (focus_before_confirm_) {
            layer->queueFocusElement(focus_before_confirm_);
        } else {
            queueDefaultFocus();
        }
    }
    focus_before_confirm_ = nullptr;
}

void SaveSlotSelectScene::onOverwriteConfirmYes() {
    if (!pending_overwrite_slot_) {
        hideOverwriteConfirm();
        return;
    }

    const int slot = *pending_overwrite_slot_;
    hideOverwriteConfirm();

    if (on_select_) {
        on_select_(slot);
        return;
    }
    spdlog::warn("SaveSlotSelectScene: 未设置 on_select 回调，忽略 slot {}", slot);
}

void SaveSlotSelectScene::onOverwriteConfirmNo() {
    hideOverwriteConfirm();
}

void SaveSlotSelectScene::onSlotSelectEvent(Rml::DataModelHandle,
                                            Rml::Event&,
                                            const Rml::VariantList& arguments) {
    const int slot = (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
    onSlotClicked(slot);
}

} // namespace game::scene
