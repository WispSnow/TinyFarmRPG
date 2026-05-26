#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/Types.h>

namespace game::runtime {
class UserSettingsService;
}

namespace game::ui {

/// @brief Inventory 菜单中的 Options 标签页。
///
/// 四项偏好：战斗动画速度 / 伤害飘字 / 敌方 HP 条 / 光标记忆。
/// UI 字号固定为 Normal，不再在此页暴露调节控件。
/// 数据源自 UserSettingsService；用户操作通过 setter 立即生效，标签关闭时落盘。
class OptionsTabContent final : public IMenuTabContent {
public:
    OptionsTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                      game::runtime::UserSettingsService* settings) noexcept;
    ~OptionsTabContent() override = default;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;

private:
    void syncFromSettings();
    void onBattleSpeedStep(int direction);
    void onToggleDamagePopup();
    void onToggleEnemyHpBar();
    void onToggleCursorMemory();

    engine::ui::rmlui::RmlDocumentController& document_controller_;
    game::runtime::UserSettingsService* settings_{nullptr};

    Rml::String options_battle_speed_text_{"x1.0"};
    Rml::String options_damage_popup_text_{"On"};
    Rml::String options_enemy_hp_bar_text_{"On"};
    Rml::String options_cursor_memory_text_{"On"};
    bool options_show_damage_popup_{true};
    bool options_show_enemy_hp_bar_{true};
    bool options_cursor_memory_{true};
};

} // namespace game::ui
