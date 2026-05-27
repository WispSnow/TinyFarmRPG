#pragma once

#include "engine/resource/decoded_image.h"
#include "game/scene/appearance_customize_types.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::component {
struct AppearanceComponent;
}

namespace game::data {
class AppearanceCatalog;
}

namespace game::ui {

enum class PortraitImageKind {
    Standard64,
    Standard64Linear,
    Battle48,
};

struct AppearancePortraitImages {
    engine::resource::DecodedImage standard64{};
    engine::resource::DecodedImage battle48{};
    std::string selection_key{};

    [[nodiscard]] bool valid() const noexcept {
        return standard64.valid() && battle48.valid() && !selection_key.empty();
    }
};

/// Builds generated UI portraits from the player's layered appearance selection.
class AppearancePortraitBuilder final {
public:
    /// @brief Builds cropped portrait images from a mutable customization selection.
    /// @return Empty images when no portrait layer can be decoded or cropped.
    [[nodiscard]] AppearancePortraitImages build(const game::data::AppearanceCatalog& catalog,
                                                 const game::scene::AppearanceSelection& selection);
    /// @brief Builds cropped portrait images from the live player appearance component.
    /// @return Empty images when no portrait layer can be decoded or cropped.
    [[nodiscard]] AppearancePortraitImages build(const game::data::AppearanceCatalog& catalog,
                                                 const game::component::AppearanceComponent& appearance);

    /// @brief Returns a stable cache key using catalog selection order instead of map iteration order.
    [[nodiscard]] static std::string stableSelectionKey(const game::data::AppearanceCatalog& catalog,
                                                        const game::scene::AppearanceSelection& selection);

private:
    std::unordered_map<std::string, engine::resource::DecodedImage> decoded_layer_cache_{};

    [[nodiscard]] const engine::resource::DecodedImage* decodeLayer(std::string_view path);
};

} // namespace game::ui
