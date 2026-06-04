#include "save_slot_select_scene.h"

#include "game/defs/options_events.h"
#include "game/save/save_service.h"
#include "game/save/save_slot_summary.h"
#include "game/ui/localized_text.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
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

[[nodiscard]] std::string_view modeName(game::scene::SaveSlotSelectScene::Mode mode) noexcept {
    switch (mode) {
        case game::scene::SaveSlotSelectScene::Mode::Save: return "save";
        case game::scene::SaveSlotSelectScene::Mode::Load:
        default: return "load";
    }
}

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

SaveSlotSelectScene::SaveSlotSelectScene(std::string_view name,
                                         engine::core::Context& context,
                                         SlotSelectCallback on_select,
                                         Mode mode,
                                         const game::runtime::LocalizationService* localization)
    : engine::scene::Scene(name, context),
      on_select_(std::move(on_select)),
      mode_(mode),
      localization_(localization) {
}

SaveSlotSelectScene::~SaveSlotSelectScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool SaveSlotSelectScene::init() {
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs).connect<&SaveSlotSelectScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .connect<&SaveSlotSelectScene::onLanguageChanged>(this);
    if (!Scene::init()) {
        return false;
    }
    return true;
}

void SaveSlotSelectScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

void SaveSlotSelectScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&SaveSlotSelectScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .disconnect<&SaveSlotSelectScene::onLanguageChanged>(this);
}

void SaveSlotSelectScene::shutdownUI() {
    document_controller_.unload();
}

bool SaveSlotSelectScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
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

    data_types_registered_ = true;
    return true;
}

bool SaveSlotSelectScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("SaveSlotSelectScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("SaveSlotSelectScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("SaveSlotSelectScene: 注册 slot data types 失败。");
        document_controller_.unload();
        return false;
    }

    if (!constructor.Bind("slots", &slots_) ||
        !constructor.Bind("confirm_visible", &confirm_visible_) ||
        !constructor.Bind("confirm_text", &confirm_text_)) {
        spdlog::error("SaveSlotSelectScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(constructor, "slot_select", &SaveSlotSelectScene::onSlotSelectEvent, this) ||
        !document_controller_.bindSimpleEvent(constructor, "back", [this] { onBackClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "confirm_yes", [this] { onOverwriteConfirmYes(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "confirm_no", [this] { onOverwriteConfirmNo(); })) {
        spdlog::error("SaveSlotSelectScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("SaveSlotSelectScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    refreshSlotButtons();
    document_controller_.markAllDirty();
    return true;
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
            label_text = game::ui::localizeTextOrFallback(localization_, "save_slot.status.error", "Error");
            enabled = false;
        } else if (!exists) {
            label_text = game::ui::localizeTextOrFallback(localization_, "save_slot.status.empty", "Empty");
            enabled = (mode_ == Mode::Save);
        } else {
            std::string summary_error;
            if (const auto summary = game::save::tryReadSlotSummary(path, summary_error)) {
                const auto day_text = std::to_string(summary->day);
                if (!summary->timestamp.empty()) {
                    const auto formatted = formatTimestampForDisplay(summary->timestamp);
                    const auto timestamp_text = formatted.empty() ? summary->timestamp : formatted;
                    label_text = game::ui::formatTextOrFallback(
                        localization_,
                        "save_slot.status.day_with_timestamp",
                        {{"day", day_text}, {"timestamp", timestamp_text}},
                        [&] { return "Day " + day_text + " - " + timestamp_text; });
                } else {
                    label_text = game::ui::formatTextOrFallback(
                        localization_,
                        "save_slot.status.day",
                        {{"day", day_text}},
                        [&] { return "Day " + day_text; });
                }
            } else {
                label_text = game::ui::localizeTextOrFallback(localization_, "save_slot.status.invalid", "Invalid");
                enabled = (mode_ == Mode::Save);
                spdlog::warn("SaveSlotSelectScene: slot {} summary 读取失败: {}", i, summary_error);
            }
        }

        SlotViewModel slot{};
        slot.slot_index = i;
        slot.label = Rml::String{label_text.data(), label_text.size()};
        slot.enabled = enabled;
        slots_.push_back(std::move(slot));
    }

    document_controller_.markDirty("slots");
}

void SaveSlotSelectScene::refreshLocalizedBindings() {
    refreshSlotButtons();
    refreshOverwriteConfirmText();
}

void SaveSlotSelectScene::refreshOverwriteConfirmText() {
    if (!pending_overwrite_slot_) {
        return;
    }

    const auto slot_text = std::to_string(*pending_overwrite_slot_ + 1);
    const auto text = game::ui::formatTextOrFallback(
        localization_,
        "save_slot.confirm.overwrite",
        {{"slot", slot_text}},
        [&] {
            return "Overwrite slot " + slot_text + "?";
        });
    if (updateBoundString(confirm_text_, text)) {
        document_controller_.markDirty("confirm_text");
    }
}

void SaveSlotSelectScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    refreshLocalizedBindings();
}

void SaveSlotSelectScene::onSlotClicked(int slot) {
    spdlog::info(
        "SaveSlotSelectScene: slot {} clicked in {} mode.",
        slot,
        modeName(mode_));
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

    refreshOverwriteConfirmText();
    if (updateBoundBool(confirm_visible_, true)) {
        document_controller_.markDirty("confirm_visible");
    }
}

void SaveSlotSelectScene::hideOverwriteConfirm() {
    pending_overwrite_slot_.reset();
    if (updateBoundBool(confirm_visible_, false)) {
        document_controller_.markDirty("confirm_visible");
    }
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
