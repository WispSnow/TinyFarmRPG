#pragma once

#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "game/defs/events_appearance.h"
#include "game/ui/appearance_portrait_builder.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <array>
#include <string>
#include <string_view>

namespace game::data {
class AppearanceCatalog;
}

namespace game::ui {

/// Owns the player's generated portrait images for the current GameScene lifetime.
class PlayerPortraitService final {
public:
    PlayerPortraitService(entt::dispatcher& dispatcher,
                          entt::registry& registry,
                          engine::ui::rmlui::RmlGeneratedImageRegistry& generated_images,
                          const game::data::AppearanceCatalog& catalog,
                          entt::entity player,
                          std::string source_prefix);
    ~PlayerPortraitService();

    PlayerPortraitService(const PlayerPortraitService&) = delete;
    PlayerPortraitService& operator=(const PlayerPortraitService&) = delete;
    PlayerPortraitService(PlayerPortraitService&&) = delete;
    PlayerPortraitService& operator=(PlayerPortraitService&&) = delete;

    /// @brief Rebuilds and registers generated portrait images for the current player appearance.
    /// @return True when a fresh image set was registered; false when refresh failed and any previous images were kept.
    [[nodiscard]] bool refresh();
    /// @brief Returns true once all generated portrait image variants are registered.
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    /// @brief Returns a generated image URI for `<img src>` bindings, or empty when unavailable.
    [[nodiscard]] std::string_view sourceUri(PortraitImageKind kind) const;
    /// @brief Returns a complete `image(...)` decorator string, or `none` when unavailable.
    [[nodiscard]] std::string decoratorString(PortraitImageKind kind) const;

private:
    entt::dispatcher& dispatcher_;
    entt::registry& registry_;
    engine::ui::rmlui::RmlGeneratedImageRegistry& generated_images_;
    const game::data::AppearanceCatalog& catalog_;
    entt::entity player_{entt::null};
    std::string source_prefix_{};
    AppearancePortraitBuilder builder_{};
    bool ready_{false};
    std::array<std::string, 3> source_uris_{};
    std::array<engine::ui::rmlui::RmlGeneratedImageRegistry::Registration, 3> registrations_{};

    void onAppearanceChanged(const game::defs::AppearanceChangedEvent& event);
    [[nodiscard]] static std::size_t kindIndex(PortraitImageKind kind) noexcept;
};

} // namespace game::ui
