#pragma once

#include "game/runtime/user_settings.h"

#include <string>

namespace game::defs {

/// @brief 用户设置 change 事件。
/// 由 game::runtime::UserSettingsService::set* 在写值后通过 entt::dispatcher 派发。
///
/// 订阅者请用 `sink<game::defs::XXXChangedEvent>().connect<&Type::handler>(this)`。

struct BattleAnimationSpeedChangedEvent {
    float new_speed{1.0f};
};

struct DamagePopupVisibilityChangedEvent {
    bool visible{true};
};

struct EnemyHpBarVisibilityChangedEvent {
    bool visible{true};
};

struct CursorMemoryChangedEvent {
    bool enabled{true};
};

struct UiFontScaleChangedEvent {
    game::runtime::UiFontScale scale{game::runtime::UiFontScale::Normal};
};

struct LanguageChangedEvent {
    std::string language_tag{"en-US"};
};

} // namespace game::defs
