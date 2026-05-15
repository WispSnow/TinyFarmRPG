#include "game/ui/options_tab_content.h"

#include "game/runtime/user_settings.h"
#include "game/runtime/user_settings_service.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace game::ui {
namespace {

[[nodiscard]] std::string formatBattleSpeed(float speed) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "x%.1f", static_cast<double>(speed));
    return std::string{buf};
}

[[nodiscard]] std::string_view toggleLabel(bool value) noexcept {
    return value ? "On" : "Off";
}

[[nodiscard]] std::string_view fontScaleLabel(game::runtime::UiFontScale scale) noexcept {
    switch (scale) {
        case game::runtime::UiFontScale::Small: return "Small";
        case game::runtime::UiFontScale::Large: return "Large";
        case game::runtime::UiFontScale::Normal:
        default:                                return "Normal";
    }
}

[[nodiscard]] std::size_t indexOfSpeed(float speed) noexcept {
    const auto& choices = game::runtime::BATTLE_ANIMATION_SPEED_CHOICES;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (choices[i] == speed) {
            return i;
        }
    }
    return 0;
}

[[nodiscard]] float speedAtIndex(std::size_t index) noexcept {
    const auto& choices = game::runtime::BATTLE_ANIMATION_SPEED_CHOICES;
    if (choices.empty()) {
        return 1.0f;
    }
    return choices[index % choices.size()];
}

} // namespace

OptionsTabContent::OptionsTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                                     game::runtime::UserSettingsService* settings) noexcept
    : document_controller_(document_controller),
      settings_(settings) {}

bool OptionsTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (!constructor.Bind("options_battle_speed_text", &options_battle_speed_text_) ||
        !constructor.Bind("options_damage_popup_text", &options_damage_popup_text_) ||
        !constructor.Bind("options_enemy_hp_bar_text", &options_enemy_hp_bar_text_) ||
        !constructor.Bind("options_cursor_memory_text", &options_cursor_memory_text_) ||
        !constructor.Bind("options_font_scale_text", &options_font_scale_text_) ||
        !constructor.Bind("options_show_damage_popup", &options_show_damage_popup_) ||
        !constructor.Bind("options_show_enemy_hp_bar", &options_show_enemy_hp_bar_) ||
        !constructor.Bind("options_cursor_memory", &options_cursor_memory_)) {
        spdlog::error("OptionsTabContent: 绑定 options 页 data model 失败。");
        return false;
    }

    if (!document_controller_.bindSimpleEvent(constructor, "options_battle_speed_prev",
                                              [this] { onBattleSpeedStep(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_battle_speed_next",
                                              [this] { onBattleSpeedStep(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_toggle_damage_popup",
                                              [this] { onToggleDamagePopup(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_toggle_enemy_hp_bar",
                                              [this] { onToggleEnemyHpBar(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_toggle_cursor_memory",
                                              [this] { onToggleCursorMemory(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_font_scale_prev",
                                              [this] { onFontScaleStep(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "options_font_scale_next",
                                              [this] { onFontScaleStep(1); })) {
        spdlog::error("OptionsTabContent: 绑定 options 页 event 回调失败。");
        return false;
    }

    return true;
}

void OptionsTabContent::onActivated() {
    syncFromSettings();
}

void OptionsTabContent::onDeactivated() {
    if (settings_) {
        (void)settings_->flushIfDirty();
    }
}

void OptionsTabContent::update(float /*delta_time*/) {
}

bool OptionsTabContent::onCancel() {
    return false;
}

void OptionsTabContent::syncFromSettings() {
    if (!settings_) {
        return;
    }
    const auto& s = settings_->snapshot();
    options_battle_speed_text_ = formatBattleSpeed(s.battle_animation_speed);
    options_damage_popup_text_ = std::string{toggleLabel(s.show_damage_popup)};
    options_enemy_hp_bar_text_ = std::string{toggleLabel(s.show_enemy_hp_bar)};
    options_cursor_memory_text_ = std::string{toggleLabel(s.cursor_memory)};
    options_font_scale_text_ = std::string{fontScaleLabel(s.ui_font_scale)};
    options_show_damage_popup_ = s.show_damage_popup;
    options_show_enemy_hp_bar_ = s.show_enemy_hp_bar;
    options_cursor_memory_ = s.cursor_memory;

    document_controller_.markDirty("options_battle_speed_text");
    document_controller_.markDirty("options_damage_popup_text");
    document_controller_.markDirty("options_enemy_hp_bar_text");
    document_controller_.markDirty("options_cursor_memory_text");
    document_controller_.markDirty("options_font_scale_text");
    document_controller_.markDirty("options_show_damage_popup");
    document_controller_.markDirty("options_show_enemy_hp_bar");
    document_controller_.markDirty("options_cursor_memory");
}

void OptionsTabContent::onBattleSpeedStep(int direction) {
    if (!settings_) {
        return;
    }
    const auto& choices = game::runtime::BATTLE_ANIMATION_SPEED_CHOICES;
    const std::size_t current = indexOfSpeed(settings_->snapshot().battle_animation_speed);
    const std::size_t next = (current + choices.size() + static_cast<std::size_t>(direction > 0 ? 1 : -1))
                             % choices.size();
    settings_->setBattleAnimationSpeed(speedAtIndex(next));
    syncFromSettings();
}

void OptionsTabContent::onFontScaleStep(int direction) {
    if (!settings_) {
        return;
    }
    constexpr std::array<game::runtime::UiFontScale, 3> kOrder{
        game::runtime::UiFontScale::Small,
        game::runtime::UiFontScale::Normal,
        game::runtime::UiFontScale::Large,
    };
    const auto current = settings_->snapshot().ui_font_scale;
    std::size_t index = 0;
    for (std::size_t i = 0; i < kOrder.size(); ++i) {
        if (kOrder[i] == current) {
            index = i;
            break;
        }
    }
    const std::size_t next = (index + kOrder.size() + static_cast<std::size_t>(direction > 0 ? 1 : -1))
                             % kOrder.size();
    settings_->setUiFontScale(kOrder[next]);
    syncFromSettings();
}

void OptionsTabContent::onToggleDamagePopup() {
    if (!settings_) {
        return;
    }
    settings_->setShowDamagePopup(!settings_->snapshot().show_damage_popup);
    syncFromSettings();
}

void OptionsTabContent::onToggleEnemyHpBar() {
    if (!settings_) {
        return;
    }
    settings_->setShowEnemyHpBar(!settings_->snapshot().show_enemy_hp_bar);
    syncFromSettings();
}

void OptionsTabContent::onToggleCursorMemory() {
    if (!settings_) {
        return;
    }
    settings_->setCursorMemory(!settings_->snapshot().cursor_memory);
    syncFromSettings();
}

} // namespace game::ui
