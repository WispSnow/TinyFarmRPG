#pragma once

#include "game/defs/commands.h"

#include <entt/signal/fwd.hpp>

namespace engine::vfx {
class VfxService;
}

namespace game::data {
class VfxCatalog;
}

namespace game::system {

class VfxBridgeSystem final {
public:
    VfxBridgeSystem(entt::dispatcher& dispatcher,
                    engine::vfx::VfxService& vfx_service,
                    const game::data::VfxCatalog* vfx_catalog);
    ~VfxBridgeSystem();

private:
    void onPlayVfxCommand(const game::defs::PlayVfxCommand& command);

    entt::dispatcher& dispatcher_;
    engine::vfx::VfxService& vfx_service_;
    const game::data::VfxCatalog* vfx_catalog_{nullptr};
};

} // namespace game::system
