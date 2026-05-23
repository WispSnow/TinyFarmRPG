#pragma once

#include "game_runtime_assembler.h"

namespace game::runtime {

class RuntimeServiceFactory final {
public:
    [[nodiscard]] static bool assemble(GameRuntimeAssembler::ServiceBuildParams params);
};

} // namespace game::runtime
