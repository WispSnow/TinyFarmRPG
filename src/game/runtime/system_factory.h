#pragma once

#include "game_runtime_assembler.h"

namespace game::runtime {

class SystemFactory final {
public:
    [[nodiscard]] static bool assemble(GameRuntimeAssembler::SystemBuildParams params);
};

} // namespace game::runtime
