#include <gtest/gtest.h>

#include "engine/vfx/vfx_backend.h"
#include "engine/vfx/vfx_service.h"
#include "../../shared/recording_vfx_backend.h"

#include <memory>

namespace engine::vfx {
namespace {

TEST(VfxServiceTest, UpdateFlushesQueuedRequestsToBackend) {
    auto backend = std::make_unique<::test::vfx::RecordingVfxBackend>();
    auto* backend_ptr = backend.get();
    VfxService service(std::move(backend));

    VfxPlayRequest first{};
    first.effect_path = "assets/vfx/00_Basic/Laser01.efkefc";
    first.position = {10.0f, 20.0f};
    first.scale = 1.25f;
    service.submit(first);

    VfxPlayRequest second{};
    second.effect_path = "assets/vfx/00_Basic/Laser01.efkefc";
    second.position = {30.0f, 40.0f};
    second.z = 2.0f;
    service.submit(second);

    ASSERT_EQ(service.pendingRequestCount(), 2u);
    service.update(0.125f);

    ASSERT_EQ(backend_ptr->requests.size(), 2u);
    EXPECT_EQ(backend_ptr->enqueue_batch_call_count, 1u);
    EXPECT_EQ(backend_ptr->last_enqueued_batch_size, 2u);
    EXPECT_FLOAT_EQ(backend_ptr->requests[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(backend_ptr->requests[1].position.y, 40.0f);
    EXPECT_EQ(backend_ptr->update_call_count, 1u);
    EXPECT_FLOAT_EQ(backend_ptr->last_delta_time, 0.125f);
    EXPECT_EQ(service.pendingRequestCount(), 0u);
}

TEST(VfxServiceTest, ClearPendingRequestsDropsQueueBeforeUpdate) {
    auto backend = std::make_unique<::test::vfx::RecordingVfxBackend>();
    auto* backend_ptr = backend.get();
    VfxService service(std::move(backend));

    VfxPlayRequest request{};
    request.effect_path = "assets/vfx/00_Basic/Laser01.efkefc";
    service.submit(request);
    ASSERT_EQ(service.pendingRequestCount(), 1u);

    service.clearPendingRequests();
    EXPECT_EQ(service.pendingRequestCount(), 0u);

    service.update(0.016f);
    EXPECT_TRUE(backend_ptr->requests.empty());
    EXPECT_EQ(backend_ptr->enqueue_batch_call_count, 0u);
    EXPECT_EQ(backend_ptr->update_call_count, 1u);
}

TEST(VfxServiceTest, NullBackendFallsBackToNoOpBackend) {
    VfxService service(nullptr);
    VfxPlayRequest request{};
    request.effect_path = "assets/vfx/00_Basic/Laser01.efkefc";
    service.submit(request);

    EXPECT_EQ(service.pendingRequestCount(), 1u);
    service.update(0.016f);
    EXPECT_EQ(service.pendingRequestCount(), 0u);
}

} // namespace
} // namespace engine::vfx
