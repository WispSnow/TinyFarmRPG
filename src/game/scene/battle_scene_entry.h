#pragma once

#include "game/battle/battle_types.h"
#include "game/scene/battle_scene_types.h"

#include <entt/entity/fwd.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace game::data {
class AppearanceCatalog;
struct AppearanceProfile;
} // namespace game::data

namespace game::scene {

inline constexpr std::string_view DEFAULT_BATTLE_PLAYER_ACTOR_ID{"actor.player"};

/// @brief 从探索态玩家实体捕获进入战斗表现层所需的外观快照。
[[nodiscard]] std::optional<AppearanceSnapshot> capturePlayerBattleAppearance(entt::registry& registry);

/// @brief 从外观 profile 构造战斗表现快照，供独立工具使用与正式入口保持一致。
[[nodiscard]] AppearanceSnapshot makeBattleAppearanceSnapshot(const game::data::AppearanceProfile& profile);

/// @brief 使用外观目录默认 profile 构造战斗表现快照。
[[nodiscard]] std::optional<AppearanceSnapshot> defaultBattleAppearanceSnapshot(
    const game::data::AppearanceCatalog& catalog);

/// @brief 根据 BattleUnit 来源信息构造 BattleScene 表现种子。
[[nodiscard]] std::vector<BattleSpriteSeed> buildBattleSpriteSeeds(
    const std::vector<game::battle::BattleUnit>& units,
    std::optional<AppearanceSnapshot> player_appearance = std::nullopt,
    std::string_view player_actor_id = DEFAULT_BATTLE_PLAYER_ACTOR_ID);

} // namespace game::scene
