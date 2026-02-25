#include "engine/vfx/null_vfx_backend.h"

namespace engine::vfx {

void NullVfxBackend::enqueue(const VfxPlayRequest&) {}

void NullVfxBackend::update(float) {}

void NullVfxBackend::render(const VfxRenderContext&) {}

std::uint32_t NullVfxBackend::getLastDrawCallCount() const {
    return 0u;
}

std::uint32_t NullVfxBackend::getLastInstanceCount() const {
    return 0u;
}

} // namespace engine::vfx
