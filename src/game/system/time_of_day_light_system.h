#pragma once
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::utils {
struct HourChangedEvent;
}

namespace game::system {

class TimeOfDayLightSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    TimeOfDayLightSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~TimeOfDayLightSystem();

private:
    void subscribe();
    void unsubscribe();

    void onHourChanged(const engine::utils::HourChangedEvent& evt);
    void applyVisibilityFromGameTime();
};

} // namespace game::system
