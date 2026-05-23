#include "game_runtime_assembler.h"

#include "runtime_service_factory.h"
#include "system_factory.h"

namespace game::runtime {

bool GameRuntimeAssembler::assembleServices(ServiceBuildParams params) {
    return RuntimeServiceFactory::assemble(params);
}

bool GameRuntimeAssembler::assembleSystems(SystemBuildParams params) {
    return SystemFactory::assemble(params);
}

} // namespace game::runtime
