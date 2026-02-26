#pragma once

#include "engine/vfx/vfx_backend.h"

namespace engine::vfx {

class NullVfxBackend final : public VfxBackend {
public:
    void enqueueBatch(std::span<const VfxPlayRequest>) override;
    void update(float) override;
    void render(const VfxRenderContext&) override;

    [[nodiscard]] std::uint32_t getLastDrawCallCount() const override;
    [[nodiscard]] std::uint32_t getLastInstanceCount() const override;
};

} // namespace engine::vfx
