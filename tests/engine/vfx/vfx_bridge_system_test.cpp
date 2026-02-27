#include <gtest/gtest.h>

#include "../../shared/test_file_utils.h"
#include "engine/vfx/vfx_backend.h"
#include "engine/vfx/vfx_bridge_system.h"
#include "engine/vfx/vfx_catalog.h"
#include "engine/vfx/vfx_service.h"
#include "engine/vfx/vfx_types.h"
#include "../../shared/recording_vfx_backend.h"

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include <memory>
#include <string>

namespace {

std::string createVfxCatalogFixture() {
    const auto temp_root = test::utils::createUniqueTempDir("vfx_catalog_fixture");
    const auto catalog_path = temp_root / "vfx_catalog.json";
    test::utils::writeTextFile(
        catalog_path,
        R"json({
  "effects": {
    "laser01": "assets/vfx/00_Basic/Laser01.efkefc"
  }
})json");
    return catalog_path.string();
}

} // namespace

namespace engine::vfx {
namespace {

TEST(VfxBridgeSystemTest, PlayCommandSubmitsResolvedRequestToVfxService) {
    entt::dispatcher dispatcher;

    VfxCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createVfxCatalogFixture()));

    auto backend = std::make_unique<::test::vfx::RecordingVfxBackend>();
    auto* backend_ptr = backend.get();
    VfxService service(std::move(backend));

    VfxBridgeSystem bridge_system(dispatcher, service, &catalog);

    PlayVfxCommand command{};
    command.effect_id = entt::hashed_string{"laser01"}.value();
    command.world_position = {128.0f, 64.0f};
    command.z = 3.0f;
    command.scale = 1.5f;
    command.loop = true;
    command.channel = VfxChannel::World;
    dispatcher.trigger(command);

    ASSERT_EQ(service.pendingRequestCount(), 1u);
    service.update(0.016f);

    ASSERT_EQ(backend_ptr->requests.size(), 1u);
    const auto& request = backend_ptr->requests.front();
    EXPECT_EQ(request.effect_id, command.effect_id);
    EXPECT_EQ(request.effect_path, "assets/vfx/00_Basic/Laser01.efkefc");
    EXPECT_FLOAT_EQ(request.world_position.x, 128.0f);
    EXPECT_FLOAT_EQ(request.world_position.y, 64.0f);
    EXPECT_FLOAT_EQ(request.z, 3.0f);
    EXPECT_FLOAT_EQ(request.scale, 1.5f);
    EXPECT_TRUE(request.loop);
    EXPECT_EQ(request.channel, VfxChannel::World);
}

TEST(VfxBridgeSystemTest, MissingEffectMappingIsIgnored) {
    entt::dispatcher dispatcher;

    VfxCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createVfxCatalogFixture()));

    auto backend = std::make_unique<::test::vfx::RecordingVfxBackend>();
    auto* backend_ptr = backend.get();
    VfxService service(std::move(backend));

    VfxBridgeSystem bridge_system(dispatcher, service, &catalog);

    PlayVfxCommand command{};
    command.effect_id = entt::hashed_string{"missing_effect"}.value();
    dispatcher.trigger(command);

    EXPECT_EQ(service.pendingRequestCount(), 0u);
    service.update(0.016f);
    EXPECT_TRUE(backend_ptr->requests.empty());
}

TEST(VfxBridgeSystemTest, NullCatalogDisablesCatalogDrivenPlayback) {
    entt::dispatcher dispatcher;

    auto backend = std::make_unique<::test::vfx::RecordingVfxBackend>();
    auto* backend_ptr = backend.get();
    VfxService service(std::move(backend));

    VfxBridgeSystem bridge_system(dispatcher, service, nullptr);

    PlayVfxCommand command{};
    command.effect_id = entt::hashed_string{"laser01"}.value();
    dispatcher.trigger(command);

    EXPECT_EQ(service.pendingRequestCount(), 0u);
    service.update(0.016f);
    EXPECT_TRUE(backend_ptr->requests.empty());
}

} // namespace
} // namespace engine::vfx
