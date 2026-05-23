#pragma once

#include "engine/input/input_manager.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::input {

using InputMappingTable = std::map<std::string, std::vector<std::string>>;

[[nodiscard]] InputMappingTable defaultInputMappings();
[[nodiscard]] bool isMenuNavigationActionName(std::string_view action_name);
[[nodiscard]] bool isMenuLikeContext(std::optional<InputContextId> context_id);

[[nodiscard]] std::unordered_map<InputContextId, InputContextDefinition> buildInputContextDefinitions(
    const std::vector<entt::id_type>& action_dispatch_order,
    const std::unordered_map<entt::id_type, ActionEntry>& actions);

} // namespace engine::input
