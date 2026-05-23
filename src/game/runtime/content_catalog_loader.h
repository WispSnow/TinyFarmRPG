#pragma once

#include "system_bundle.h"

namespace engine::resource {
class AssetRegistry;
}

namespace game::runtime {

class ContentCatalogLoader final {
public:
    [[nodiscard]] static bool ensureBlueprintManager(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureItemCatalog(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureAppearanceCatalog(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureVfxCatalog(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureRpgCatalog(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureQuestCatalog(GameRuntimeServices& services);
    [[nodiscard]] static bool ensureShopCatalog(GameRuntimeServices& services);
    static void ensureAudioCueCatalog(GameRuntimeServices& services,
                                      const engine::resource::AssetRegistry& asset_registry);
};

} // namespace game::runtime
