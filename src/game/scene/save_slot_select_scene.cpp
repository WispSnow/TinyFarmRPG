#include "save_slot_select_scene.h"

#include "game/save/save_service.h"
#include "game/save/save_slot_summary.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/layout/ui_grid_layout.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_input_blocker.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_panel.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>

using namespace entt::literals;

namespace {

constexpr int SLOT_COUNT = 10;
constexpr int GRID_COLUMNS = 2;
constexpr glm::vec2 GRID_SPACING{8.0f, 8.0f};
constexpr glm::vec2 SLOT_BUTTON_SIZE{160.0f, 32.0f};

constexpr glm::vec2 BACK_BUTTON_SIZE{160.0f, 32.0f};

constexpr engine::ui::Thickness PANEL_PADDING{24.0f, 24.0f, 24.0f, 24.0f};
constexpr float PANEL_INTERNAL_SPACING = 24.0f;

constexpr glm::vec2 CONFIRM_PANEL_SIZE{280.0f, 140.0f};
constexpr engine::ui::Thickness CONFIRM_PADDING{24.0f, 24.0f, 24.0f, 24.0f};
constexpr glm::vec2 CONFIRM_BUTTON_SIZE{100.0f, 32.0f};
constexpr float CONFIRM_BUTTON_SPACING = 12.0f;

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

} // namespace

namespace game::scene {

SaveSlotSelectScene::SaveSlotSelectScene(std::string_view name, engine::core::Context& context, SlotSelectCallback on_select, Mode mode)
    : engine::scene::Scene(name, context),
      on_select_(std::move(on_select)),
      mode_(mode) {
}

SaveSlotSelectScene::~SaveSlotSelectScene() {
    context_.getInputManager().onAction("pause"_hs).disconnect<&SaveSlotSelectScene::onPausePressed>(this);
}

bool SaveSlotSelectScene::init() {
    if (!initUI()) return false;
    context_.getInputManager().onAction("pause"_hs).connect<&SaveSlotSelectScene::onPausePressed>(this);
    if (!Scene::init()) return false;
    return true;
}

bool SaveSlotSelectScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);

    buildLayout();
    refreshSlotButtons();
    return true;
}

void SaveSlotSelectScene::buildLayout() {
    auto dim = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.6f});
    dim->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    dim->setPivot({0.0f, 0.0f});
    dim->setOrderIndex(0);
    dim_ = dim.get();
    ui_manager_->addElement(std::move(dim));

    auto blocker = std::make_unique<engine::ui::UIInputBlocker>(context_, glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
    blocker->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    blocker->setPivot({0.0f, 0.0f});
    blocker->setOrderIndex(1);
    input_blocker_ = blocker.get();
    ui_manager_->addElement(std::move(blocker));

    const int rows = (SLOT_COUNT + GRID_COLUMNS - 1) / GRID_COLUMNS;
    const float grid_width = GRID_COLUMNS * SLOT_BUTTON_SIZE.x + (GRID_COLUMNS - 1) * GRID_SPACING.x;
    const float grid_height = rows * SLOT_BUTTON_SIZE.y + (rows - 1) * GRID_SPACING.y;
    const glm::vec2 panel_size{
        grid_width + PANEL_PADDING.width(),
        grid_height + BACK_BUTTON_SIZE.y + PANEL_INTERNAL_SPACING + PANEL_PADDING.height(),
    };

    auto panel = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f}, panel_size, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.75f});
    panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    panel->setPivot({0.5f, 0.5f});
    panel->setPadding(PANEL_PADDING);
    panel->setOrderIndex(2);

    auto grid = std::make_unique<engine::ui::UIGridLayout>(glm::vec2{0.0f, 0.0f}, glm::vec2{grid_width, grid_height});
    grid->setColumnCount(GRID_COLUMNS);
    grid->setCellSize(SLOT_BUTTON_SIZE);
    grid->setSpacing(GRID_SPACING);
    grid->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});

    slot_buttons_.clear();
    slot_buttons_.reserve(SLOT_COUNT);
    for (int i = 0; i < SLOT_COUNT; ++i) {
        auto button = engine::ui::UIButton::create(
            context_, "primary", glm::vec2{0.0f, 0.0f}, SLOT_BUTTON_SIZE, [this, i]() { onSlotClicked(i); });
        if (!button) {
            spdlog::error("SaveSlotSelectScene: 创建 slot_button {} 失败。", i);
            slot_buttons_.push_back(nullptr);
            continue;
        }
        button->setTextLayoutScaleToFit(engine::ui::Thickness{10.0f, 6.0f, 10.0f, 6.0f});
        slot_buttons_.push_back(button.get());
        grid->addChild(std::move(button));
    }

    grid_ = grid.get();
    panel->addChild(std::move(grid));

    const float back_x = std::max(0.0f, (grid_width - BACK_BUTTON_SIZE.x) * 0.5f);
    const float back_y = grid_height + PANEL_INTERNAL_SPACING;
    auto back = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{back_x, back_y}, BACK_BUTTON_SIZE, [this]() { onBackClicked(); });
    if (back) {
        back->setLabelText("Back");
        back_button_ = back.get();
        panel->addChild(std::move(back));
    } else {
        spdlog::error("SaveSlotSelectScene: 创建 back_button 失败。");
    }

    panel_ = panel.get();
    ui_manager_->addElement(std::move(panel));

    auto confirm_blocker =
        std::make_unique<engine::ui::UIInputBlocker>(context_, glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
    confirm_blocker->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    confirm_blocker->setPivot({0.0f, 0.0f});
    confirm_blocker->setOrderIndex(3);
    confirm_blocker->setVisible(false);
    confirm_blocker_ = confirm_blocker.get();
    ui_manager_->addElement(std::move(confirm_blocker));

    auto confirm_panel = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f}, CONFIRM_PANEL_SIZE, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.85f});
    confirm_panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    confirm_panel->setPivot({0.5f, 0.5f});
    confirm_panel->setPadding(CONFIRM_PADDING);
    confirm_panel->setOrderIndex(4);
    confirm_panel->setVisible(false);

    auto& text_renderer = context_.getTextRenderer();
    auto label = std::make_unique<engine::ui::UILabel>(text_renderer,
                                                       "Overwrite?",
                                                       engine::ui::DEFAULT_UI_FONT_PATH,
                                                       engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                       glm::vec2{0.0f, 0.0f});
    label->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    label->setPivot({0.5f, 0.0f});
    confirm_label_ = label.get();
    confirm_panel->addChild(std::move(label));

    const float row_width = CONFIRM_BUTTON_SIZE.x * 2.0f + CONFIRM_BUTTON_SPACING;
    auto button_row =
        std::make_unique<engine::ui::UIStackLayout>(glm::vec2{0.0f, 0.0f}, glm::vec2{row_width, CONFIRM_BUTTON_SIZE.y});
    button_row->setOrientation(engine::ui::Orientation::Horizontal);
    button_row->setSpacing(CONFIRM_BUTTON_SPACING);
    button_row->setAnchor({0.5f, 1.0f}, {0.5f, 1.0f});
    button_row->setPivot({0.5f, 1.0f});

    auto yes = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, CONFIRM_BUTTON_SIZE, [this]() { onOverwriteConfirmYes(); });
    if (yes) {
        yes->setLabelText("OK");
        confirm_yes_button_ = yes.get();
        button_row->addChild(std::move(yes));
    } else {
        spdlog::error("SaveSlotSelectScene: 创建 confirm_yes_button 失败。");
    }

    auto no = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{0.0f, 0.0f}, CONFIRM_BUTTON_SIZE, [this]() { onOverwriteConfirmNo(); });
    if (no) {
        no->setLabelText("Cancel");
        confirm_no_button_ = no.get();
        button_row->addChild(std::move(no));
    } else {
        spdlog::error("SaveSlotSelectScene: 创建 confirm_no_button 失败。");
    }
    confirm_button_row_ = button_row.get();
    confirm_panel->addChild(std::move(button_row));

    confirm_panel_ = confirm_panel.get();
    ui_manager_->addElement(std::move(confirm_panel));
}

void SaveSlotSelectScene::refreshSlotButtons() {
    if (slot_buttons_.size() != static_cast<std::size_t>(SLOT_COUNT)) {
        return;
    }

    for (int i = 0; i < SLOT_COUNT; ++i) {
        auto* button = slot_buttons_[static_cast<std::size_t>(i)];
        if (!button) continue;

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

        button->setLabelText(label_text);

        button->setInteractive(enabled);
        button->applyStateVisual(enabled ? engine::ui::UI_IMAGE_NORMAL_ID : engine::ui::UI_IMAGE_DISABLED_ID);
    }
}

void SaveSlotSelectScene::onSlotClicked(int slot) {
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
    if (confirm_panel_ && confirm_panel_->isVisible()) {
        hideOverwriteConfirm();
        return;
    }
    requestPopScene();
}

bool SaveSlotSelectScene::onPausePressed() {
    if (confirm_panel_ && confirm_panel_->isVisible()) {
        hideOverwriteConfirm();
        return true;
    }
    requestPopScene();
    return true;
}

void SaveSlotSelectScene::showOverwriteConfirm(int slot) {
    pending_overwrite_slot_ = slot;

    if (confirm_label_) {
        confirm_label_->setText("Overwrite slot " + std::to_string(slot + 1) + "?");
        confirm_label_->setVisible(true);
    }
    if (confirm_blocker_) {
        confirm_blocker_->setVisible(true);
    }
    if (confirm_panel_) {
        confirm_panel_->setVisible(true);
    }
}

void SaveSlotSelectScene::hideOverwriteConfirm() {
    pending_overwrite_slot_.reset();
    if (confirm_blocker_) {
        confirm_blocker_->setVisible(false);
    }
    if (confirm_panel_) {
        confirm_panel_->setVisible(false);
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
