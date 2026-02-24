#pragma once

#include <sol/sol.hpp>

#include <string_view>

namespace engine::script {

[[nodiscard]] sol::table createReadOnlyProxy(sol::state& lua, sol::table source, std::string_view name);

} // namespace engine::script
