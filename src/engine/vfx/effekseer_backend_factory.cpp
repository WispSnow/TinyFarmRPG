#include "engine/vfx/effekseer_backend_factory.h"

#ifdef TF_ENABLE_EFFEKSEER
#include "engine/vfx/effekseer_backend.h"
#endif

namespace engine::vfx {

std::unique_ptr<VfxBackend> createEffekseerBackend() {
#ifdef TF_ENABLE_EFFEKSEER
    return EffekseerBackend::create();
#else
    return nullptr;
#endif
}

} // namespace engine::vfx
