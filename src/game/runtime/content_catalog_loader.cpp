#include "content_catalog_loader.h"

#include "engine/resource/asset_registry.h"
#include "engine/vfx/vfx_catalog.h"
#include "game/data/appearance_catalog.h"
#include "game/data/audio_cue_catalog.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/factory/blueprint_manager.h"
#include "game/runtime/game_content_manifest.h"
#include "game/runtime/rpg_catalog_loader.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace game::runtime {

bool ContentCatalogLoader::ensureBlueprintManager(GameRuntimeServices& services) {
    if (!services.blueprint_manager) {
        services.blueprint_manager = std::make_shared<game::factory::BlueprintManager>();
        if (!services.blueprint_manager->loadActorBlueprints(GameContentManifest::ActorBlueprints)) {
            spdlog::error("加载角色蓝图失败");
            return false;
        }
        if (!services.blueprint_manager->loadAnimalBlueprints(GameContentManifest::AnimalBlueprints)) {
            spdlog::error("加载动物蓝图失败");
            return false;
        }
        if (!services.blueprint_manager->loadCropBlueprints(GameContentManifest::CropBlueprints)) {
            spdlog::error("加载作物蓝图失败");
            return false;
        }
    }
    return true;
}

bool ContentCatalogLoader::ensureItemCatalog(GameRuntimeServices& services) {
    if (!services.item_catalog) {
        services.item_catalog = std::make_shared<game::data::ItemCatalog>();
        if (!services.item_catalog->loadIconConfig(GameContentManifest::ItemIcons)) {
            spdlog::error("加载物品图标配置失败");
            return false;
        }
        if (!services.item_catalog->loadItemConfig(GameContentManifest::Items)) {
            spdlog::error("加载物品配置失败");
            return false;
        }
    }
    return true;
}

bool ContentCatalogLoader::ensureAppearanceCatalog(GameRuntimeServices& services) {
    if (!services.appearance_catalog) {
        services.appearance_catalog = std::make_shared<game::data::AppearanceCatalog>();
        if (!services.appearance_catalog->loadFromFile(GameContentManifest::AppearanceCatalog)) {
            spdlog::error("加载外观目录配置失败");
            return false;
        }
    }
    return true;
}

bool ContentCatalogLoader::ensureVfxCatalog(GameRuntimeServices& services) {
    if (!services.vfx_catalog) {
        services.vfx_catalog = std::make_shared<engine::vfx::VfxCatalog>();
        if (!services.vfx_catalog->loadFromFile(GameContentManifest::VfxCatalog)) {
            spdlog::warn("加载 VFX 目录配置失败，将继续运行但禁用 catalog 驱动播放。");
        }
    }
    return true;
}

bool ContentCatalogLoader::ensureRpgCatalog(GameRuntimeServices& services) {
    if (services.rpg_catalog) {
        return true;
    }

    services.rpg_catalog = std::make_shared<game::data::RpgCatalog>();
    game::runtime::RpgCatalogLoadOptions options{};
    options.item_catalog = services.item_catalog.get();
    std::string load_error{};
    if (!game::runtime::loadRpgCatalogFromManifest(*services.rpg_catalog, options, load_error)) {
        spdlog::error("{}", load_error);
        return false;
    }

    return true;
}

bool ContentCatalogLoader::ensureQuestCatalog(GameRuntimeServices& services) {
    if (services.quest_catalog) {
        return true;
    }

    if (!services.rpg_catalog || !services.item_catalog) {
        spdlog::error("QuestCatalog 依赖 RpgCatalog 和 ItemCatalog。");
        return false;
    }

    services.quest_catalog = std::make_shared<game::data::QuestCatalog>();
    if (!services.quest_catalog->loadFromFile(GameContentManifest::Quests)) {
        spdlog::error("加载 QuestCatalog 失败: {}", GameContentManifest::Quests);
        return false;
    }

    std::string reference_error{};
    if (!services.quest_catalog->validateReferences(services.rpg_catalog.get(),
                                                    services.item_catalog.get(),
                                                    reference_error)) {
        spdlog::error("QuestCatalog 引用校验失败: {}", reference_error);
        return false;
    }

    return true;
}

bool ContentCatalogLoader::ensureShopCatalog(GameRuntimeServices& services) {
    if (services.shop_catalog) {
        return true;
    }

    if (!services.item_catalog) {
        spdlog::error("ShopCatalog 依赖 ItemCatalog。");
        return false;
    }

    services.shop_catalog = std::make_shared<game::data::ShopCatalog>();
    if (!services.shop_catalog->loadFromFile(GameContentManifest::Shops)) {
        spdlog::error("加载 ShopCatalog 失败: {}", GameContentManifest::Shops);
        return false;
    }

    std::string reference_error{};
    if (!services.shop_catalog->validateReferences(services.item_catalog.get(), reference_error)) {
        spdlog::error("ShopCatalog 引用校验失败: {}", reference_error);
        return false;
    }

    return true;
}

void ContentCatalogLoader::ensureAudioCueCatalog(GameRuntimeServices& services,
                                                 const engine::resource::AssetRegistry& asset_registry) {
    if (services.audio_cue_catalog) {
        std::string reference_error{};
        if (!services.audio_cue_catalog->validateReferences(asset_registry, reference_error)) {
            spdlog::error("AudioCueCatalog 引用校验失败: {}", reference_error);
            services.audio_cue_catalog.reset();
        }
        return;
    }

    auto catalog = std::make_shared<game::data::AudioCueCatalog>();
    if (!catalog->loadFromFile(GameContentManifest::AudioCues)) {
        spdlog::error("加载 AudioCueCatalog 失败: {}", GameContentManifest::AudioCues);
        return;
    }

    std::string reference_error{};
    if (!catalog->validateReferences(asset_registry, reference_error)) {
        spdlog::error("AudioCueCatalog 引用校验失败: {}", reference_error);
        return;
    }

    services.audio_cue_catalog = std::move(catalog);
}

} // namespace game::runtime
