#pragma once

#include "engine/vfx/vfx_types.h"

#include <cstdint>

namespace engine::vfx {

class VfxBackend {
public:
    virtual ~VfxBackend() = default;

    virtual void enqueue(const VfxPlayRequest& request) = 0;
    virtual void update(float delta_time_seconds) = 0;
    virtual void render(const VfxRenderContext& context) = 0;

    [[nodiscard]] virtual std::uint32_t getLastDrawCallCount() const = 0;
    [[nodiscard]] virtual std::uint32_t getLastInstanceCount() const = 0;
};

} // namespace engine::vfx
