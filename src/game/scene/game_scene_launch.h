#pragma once

#include "game/scene/appearance_customize_types.h"

#include <optional>
#include <string>
#include <variant>

namespace game::scene {

/// Options for starting a fresh GameScene instance.
struct NewGameOptions {
    /// Initial player appearance selected before the game world is entered.
    std::optional<AppearanceSelection> initial_appearance{};
    std::string player_name{};
};

/// Options for starting GameScene from an existing save slot.
struct LoadGameOptions {
    int slot{0};
};

/// Mutually exclusive GameScene launch modes.
using GameSceneLaunch = std::variant<NewGameOptions, LoadGameOptions>;

} // namespace game::scene
