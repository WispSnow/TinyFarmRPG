#include "engine/vfx/vfx_service.h"

#include "engine/vfx/null_vfx_backend.h"

namespace engine::vfx {

VfxService::VfxService(std::unique_ptr<VfxBackend> backend)
    : backend_(std::move(backend)) {
    if (!backend_) {
        backend_ = std::make_unique<NullVfxBackend>();
    }
}

void VfxService::submit(const VfxPlayRequest& request) {
    pending_requests_.push_back(request);
}

void VfxService::update(float delta_time_seconds) {
    for (const auto& request : pending_requests_) {
        backend_->enqueue(request);
    }
    pending_requests_.clear();

    backend_->update(delta_time_seconds);
}

void VfxService::clearPendingRequests() {
    pending_requests_.clear();
}

} // namespace engine::vfx
