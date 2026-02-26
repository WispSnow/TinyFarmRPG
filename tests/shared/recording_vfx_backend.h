#pragma once

#include "engine/vfx/vfx_backend.h"

#include <cstdint>
#include <span>
#include <vector>

namespace test::vfx {

class RecordingVfxBackend final : public engine::vfx::VfxBackend {
public:
    void enqueueBatch(std::span<const engine::vfx::VfxPlayRequest> batch) override {
        ++enqueue_batch_call_count;
        last_enqueued_batch_size = static_cast<std::uint32_t>(batch.size());
        requests.insert(requests.end(), batch.begin(), batch.end());
    }

    void update(float delta_time_seconds) override {
        ++update_call_count;
        last_delta_time = delta_time_seconds;
    }

    void render(const engine::vfx::VfxRenderContext& context) override {
        ++render_call_count;
        last_render_context = context;
    }

    [[nodiscard]] std::uint32_t getLastDrawCallCount() const override {
        return 0u;
    }

    [[nodiscard]] std::uint32_t getLastInstanceCount() const override {
        return static_cast<std::uint32_t>(requests.size());
    }

    std::vector<engine::vfx::VfxPlayRequest> requests{};
    std::uint32_t enqueue_batch_call_count{0u};
    std::uint32_t last_enqueued_batch_size{0u};
    std::uint32_t update_call_count{0u};
    std::uint32_t render_call_count{0u};
    float last_delta_time{0.0f};
    engine::vfx::VfxRenderContext last_render_context{};
};

} // namespace test::vfx
