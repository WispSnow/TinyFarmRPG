#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
class AppearanceCatalog;
}

namespace game::defs {
struct SetAppearanceSlotCommand;
struct RefreshAppearanceCommand;
}

namespace game::system {

class AppearanceSystem {
public:
    AppearanceSystem(entt::registry& registry, entt::dispatcher& dispatcher, const game::data::AppearanceCatalog& catalog);
    ~AppearanceSystem();

private:
    void onSetAppearanceSlotCommand(const game::defs::SetAppearanceSlotCommand& command);
    void onRefreshAppearanceCommand(const game::defs::RefreshAppearanceCommand& command);
    void rebuildLayerCache(entt::entity entity);

    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::AppearanceCatalog& catalog_;
};

} // namespace game::system
