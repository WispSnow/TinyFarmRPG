#pragma once

#include "system_bundle.h"

#include <entt/entity/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::runtime {

class ScriptRuntimeFactory final {
public:
    static void tryInitScriptHost(entt::registry& registry,
                                  engine::core::Context& context,
                                  GameRuntimeServices& services);
};

} // namespace game::runtime
