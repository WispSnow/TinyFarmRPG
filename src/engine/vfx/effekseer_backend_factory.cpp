#include "engine/vfx/effekseer_backend_factory.h"

#include "engine/vfx/effekseer_backend.h"

namespace engine::vfx {

std::unique_ptr<VfxBackend> createEffekseerBackend() {
    return EffekseerBackend::create();
}

} // namespace engine::vfx
