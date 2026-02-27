#include "engine/vfx/vfx_bridge_system.h"

#include "engine/vfx/vfx_catalog.h"
#include "engine/vfx/vfx_service.h"
#include "engine/vfx/vfx_types.h"

#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace engine::vfx {

VfxBridgeSystem::VfxBridgeSystem(entt::dispatcher& dispatcher,
                                 VfxService& vfx_service,
                                 const VfxCatalog* vfx_catalog)
    : dispatcher_(dispatcher),
      vfx_service_(vfx_service),
      vfx_catalog_(vfx_catalog) {
    dispatcher_.sink<PlayVfxCommand>().connect<&VfxBridgeSystem::onPlayVfxCommand>(this);
}

VfxBridgeSystem::~VfxBridgeSystem() {
    dispatcher_.sink<PlayVfxCommand>().disconnect<&VfxBridgeSystem::onPlayVfxCommand>(this);
}

void VfxBridgeSystem::onPlayVfxCommand(const PlayVfxCommand& command) {
    if (!vfx_catalog_) {
        spdlog::warn("VfxBridgeSystem: VfxCatalog 不可用，忽略播放请求");
        return;
    }

    if (command.effect_id == kInvalidVfxEffectId) {
        return;
    }

    const auto* effect_path = vfx_catalog_->findEffectPath(command.effect_id);
    if (!effect_path) {
        spdlog::warn("VfxBridgeSystem: effect_id={} 未在 VfxCatalog 中配置", command.effect_id);
        return;
    }

    VfxPlayRequest request{};
    request.effect_id = command.effect_id;
    request.effect_path = *effect_path;
    request.world_position = command.world_position;
    request.z = command.z;
    request.scale = command.scale;
    request.loop = command.loop;
    request.channel = command.channel;
    vfx_service_.submit(request);
}

} // namespace engine::vfx
