#pragma once

namespace engine::resource {
class AssetRegistry;
}

namespace game::data {
class AppearanceCatalog;
class ItemCatalog;
}

namespace game::factory {
class BlueprintManager;
}

namespace game::world {
class WorldState;
}

namespace game::runtime {

class AssetPreloadRegistrar final {
public:
    static void collectBlueprintAssets(const game::factory::BlueprintManager& manager,
                                       engine::resource::AssetRegistry& registry);
    static void collectItemCatalogAssets(const game::data::ItemCatalog& catalog,
                                         engine::resource::AssetRegistry& registry);
    static void collectAppearanceAssets(const game::data::AppearanceCatalog& catalog,
                                        engine::resource::AssetRegistry& registry);
    static void collectWorldMapAssets(const game::world::WorldState& world_state,
                                      engine::resource::AssetRegistry& registry);
};

} // namespace game::runtime
