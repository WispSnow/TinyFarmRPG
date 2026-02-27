#pragma once

#include "engine/vfx/vfx_types.h"

#include <entt/signal/fwd.hpp>

namespace engine::vfx {

class VfxService;
class VfxCatalog;

class VfxBridgeSystem final {
public:
    VfxBridgeSystem(entt::dispatcher& dispatcher,
                    VfxService& vfx_service,
                    const VfxCatalog* vfx_catalog);
    ~VfxBridgeSystem();

private:
    void onPlayVfxCommand(const PlayVfxCommand& command);

    entt::dispatcher& dispatcher_;
    VfxService& vfx_service_;
    const VfxCatalog* vfx_catalog_{nullptr};
};

} // namespace engine::vfx
