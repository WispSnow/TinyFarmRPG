#pragma once

#include "engine/vfx/vfx_backend.h"

#include <memory>

namespace engine::vfx {

[[nodiscard]] std::unique_ptr<VfxBackend> createEffekseerBackend();

} // namespace engine::vfx
