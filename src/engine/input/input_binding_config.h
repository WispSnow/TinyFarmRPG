#pragma once

#include "engine/input/input_context_registry.h"
#include "engine/input/input_manager.h"

#include <optional>
#include <string_view>

namespace engine::input {

[[nodiscard]] std::optional<InputMappingTable> loadInputMappingsConfig(std::string_view config_path);

[[nodiscard]] bool persistInputBindingsConfig(
    std::string_view config_path,
    const std::vector<entt::id_type>& action_dispatch_order,
    const std::unordered_map<entt::id_type, ActionEntry>& actions,
    const std::unordered_map<entt::id_type, std::vector<BindingDefinition>>& action_bindings);

} // namespace engine::input
