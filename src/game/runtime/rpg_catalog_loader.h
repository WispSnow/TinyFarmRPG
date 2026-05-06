#pragma once

#include <string>
#include <string_view>

namespace game::data {
class ItemCatalog;
class RpgCatalog;
} // namespace game::data

namespace game::runtime {

struct RpgCatalogLoadOptions {
    std::string manifest_path{"assets/data/rpg/manifest.json"};
    std::string root_path{"assets/data/rpg"};
    const game::data::ItemCatalog* item_catalog{nullptr};
};

/// @brief 从 RPG manifest 加载并校验完整 RPG catalog。
[[nodiscard]] bool loadRpgCatalogFromManifest(game::data::RpgCatalog& catalog,
                                              const RpgCatalogLoadOptions& options,
                                              std::string& out_error);

} // namespace game::runtime
