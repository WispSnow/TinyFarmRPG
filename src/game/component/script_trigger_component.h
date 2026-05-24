#pragma once

#include <string>

namespace game::component {

/// @brief Optional Lua metadata attached from Tiled script_* properties.
///
/// Format conventions are documented in docs/game/map_data_pipeline.md.
struct ScriptTriggerComponent {
    std::string module_{};
    std::string event_{};
    std::string once_key_{};
};

} // namespace game::component
