#include "save_slot_select_scene.h"

#include "game/save/save_service.h"
#include "game/save/save_slot_summary.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
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
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

using namespace entt::literals;

namespace {

constexpr int SLOT_COUNT = 10;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/save_slot_select.rml";
constexpr std::string_view MODEL_NAME = "save_slot_select";

bool tryToLocalTm(std::time_t value, std::tm& out) {
#if defined(_WIN32)
    return localtime_s(&out, &value) == 0;
#else
    return localtime_r(&value, &out) != nullptr;
#endif
}

std::string formatTimestampForDisplay(std::string_view timestamp) {
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

template <typename T>
bool assignIfChanged(T& target, const T& value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

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
    context_.getInputManager().onAction("pause"_hs).disconnect<&SaveSlotSelectScene::onPausePressed>(this);
}

bool SaveSlotSelectScene::init() {
    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("pause"_hs).connect<&SaveSlotSelectScene::onPausePressed>(this);
    if (!Scene::init()) {
        return false;
    }
    return true;
}

void SaveSlotSelectScene::clean() {
    removeEventListeners();
    data_bridge_.destroy();
    document_ = nullptr;
    Scene::clean();
}

bool SaveSlotSelectScene::initUI() {
    auto* rml_layer = context_.getGLRenderer().getRmlUILayer();
    if (!rml_layer || !rml_layer->getContext()) {
        spdlog::error("SaveSlotSelectScene: RmlUILayer 或 Context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_layer->getContext(), MODEL_NAME);
    if (!constructor) {
        spdlog::error("SaveSlotSelectScene: 创建 data model '{}' 失败。", MODEL_NAME);
        return false;
    }

    static bool data_types_registered = false;
    if (!data_types_registered) {
        auto slot_struct = constructor.RegisterStruct<SlotViewModel>();
        if (!slot_struct) {
            spdlog::error("SaveSlotSelectScene: 注册 SlotViewModel 失败。");
            return false;
        }

        slot_struct.RegisterMember("slot_index", &SlotViewModel::slot_index);
        slot_struct.RegisterMember("label", &SlotViewModel::label);
        slot_struct.RegisterMember("enabled", &SlotViewModel::enabled);

        if (!constructor.RegisterArray<Rml::Vector<SlotViewModel>>()) {
            spdlog::error("SaveSlotSelectScene: 注册 slots 数组类型失败。");
            return false;
        }

        data_types_registered = true;
    }

    constructor.Bind("slots", &slots_);
    constructor.Bind("panel_title", &panel_title_);
    constructor.Bind("back_text", &back_text_);
    constructor.Bind("confirm_visible", &confirm_visible_);
    constructor.Bind("confirm_text", &confirm_text_);

    panel_title_ = (mode_ == Mode::Save) ? Rml::String{"Save Slot"} : Rml::String{"Load Slot"};
    back_text_ = "Back";
    confirm_visible_ = false;
    confirm_text_ = "Overwrite?";

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        spdlog::error("SaveSlotSelectScene: 加载文档 '{}' 失败。", DOCUMENT_PATH);
        data_bridge_.destroy();
        return false;
    }

    bindEvents();
    refreshSlotButtons();
    data_bridge_.markAllDirty();
    return true;
}

void SaveSlotSelectScene::bindEvents() {
    event_bridge_.on("slot", [this](Rml::Event& event) {
        const auto slot = extractSlotIndex(event);
        if (!slot.has_value()) {
            spdlog::warn("SaveSlotSelectScene: slot 事件缺少有效 data-slot-index。");
            return;
        }
        onSlotClicked(*slot);
    });

    event_bridge_.on("back", [this](Rml::Event&) { onBackClicked(); });
    event_bridge_.on("confirm_yes", [this](Rml::Event&) { onOverwriteConfirmYes(); });
    event_bridge_.on("confirm_no", [this](Rml::Event&) { onOverwriteConfirmNo(); });

    if (document_) {
        event_bridge_.registerTo(document_, "click");
        click_listener_registered_ = true;
    }
}

void SaveSlotSelectScene::removeEventListeners() {
    if (!click_listener_registered_ || !document_) {
        return;
    }

    document_->RemoveEventListener("click", &event_bridge_);
    click_listener_registered_ = false;
}

void SaveSlotSelectScene::refreshSlotButtons() {
    slots_.clear();
    slots_.reserve(static_cast<size_t>(SLOT_COUNT));

    for (int i = 0; i < SLOT_COUNT; ++i) {
        const auto path = game::save::SaveService::slotPath(i);
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);

        SlotViewModel slot{};
        slot.slot_index = i;
        slot.enabled = true;

        if (ec) {
            slot.label = "Error";
            slot.enabled = false;
        } else if (!exists) {
            slot.label = "Empty";
            slot.enabled = (mode_ == Mode::Save);
        } else {
            std::string summary_error;
            if (const auto summary = game::save::tryReadSlotSummary(path, summary_error)) {
                std::string label_text = "Day " + std::to_string(summary->day);
                if (!summary->timestamp.empty()) {
                    const auto formatted = formatTimestampForDisplay(summary->timestamp);
                    label_text += " - " + (formatted.empty() ? summary->timestamp : formatted);
                }
                slot.label = label_text;
            } else {
                slot.label = "Invalid";
                spdlog::warn("SaveSlotSelectScene: slot {} summary 读取失败: {}", i, summary_error);
            }
        }

        slots_.push_back(std::move(slot));
    }

    data_bridge_.markDirty("slots");
}

std::optional<int> SaveSlotSelectScene::extractSlotIndex(Rml::Event& event) const {
    auto* element = event.GetTargetElement();
    while (element) {
        const auto slot_attr = element->GetAttribute<Rml::String>("data-slot-index", "");
        if (!slot_attr.empty()) {
            int slot = -1;
            const char* begin = slot_attr.data();
            const char* end = begin + slot_attr.size();
            const auto [ptr, ec] = std::from_chars(begin, end, slot);
            if (ec == std::errc{} && ptr == end) {
                return slot;
            }
            return std::nullopt;
        }
        element = element->GetParentNode();
    }
    return std::nullopt;
}

void SaveSlotSelectScene::onSlotClicked(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) {
        spdlog::warn("SaveSlotSelectScene: 非法 slot {}。", slot);
        return;
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

bool SaveSlotSelectScene::onPausePressed() {
    if (confirm_visible_) {
        hideOverwriteConfirm();
        return true;
    }

    requestPopScene();
    return true;
}

void SaveSlotSelectScene::showOverwriteConfirm(int slot) {
    pending_overwrite_slot_ = slot;

    const Rml::String next_text = "Overwrite slot " + std::to_string(slot + 1) + "?";
    if (assignIfChanged(confirm_text_, next_text)) {
        data_bridge_.markDirty("confirm_text");
    }
    if (assignIfChanged(confirm_visible_, true)) {
        data_bridge_.markDirty("confirm_visible");
    }
}

void SaveSlotSelectScene::hideOverwriteConfirm() {
    pending_overwrite_slot_.reset();

    if (assignIfChanged(confirm_visible_, false)) {
        data_bridge_.markDirty("confirm_visible");
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

} // namespace game::scene
