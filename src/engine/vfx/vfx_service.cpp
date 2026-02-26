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
    if (!pending_requests_.empty()) {
        backend_->enqueueBatch(pending_requests_);
        pending_requests_.clear();
    }

    backend_->update(delta_time_seconds);
}

void VfxService::clearPendingRequests() {
    pending_requests_.clear();
}

} // namespace engine::vfx
