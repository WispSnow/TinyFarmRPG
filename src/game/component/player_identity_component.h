#pragma once

#include <string>

namespace game::component {

/// @brief Player-facing identity values that are chosen at new-game creation.
struct PlayerIdentityComponent {
    std::string display_name_{};
};

} // namespace game::component
