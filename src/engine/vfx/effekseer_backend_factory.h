#pragma once

#include "engine/vfx/vfx_backend.h"

#include <memory>

#ifdef TF_ENABLE_EFFEKSEER

namespace engine::vfx {

[[nodiscard]] std::unique_ptr<VfxBackend> createEffekseerBackend();

} // namespace engine::vfx

#endif // TF_ENABLE_EFFEKSEER
