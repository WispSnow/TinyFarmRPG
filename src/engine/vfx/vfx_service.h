#pragma once

#include "engine/vfx/vfx_backend.h"

#include <memory>
#include <vector>

namespace engine::vfx {

class VfxService final {
public:
    explicit VfxService(std::unique_ptr<VfxBackend> backend);

    void submit(const VfxPlayRequest& request);
    void update(float delta_time_seconds);
    void clearPendingRequests();

    [[nodiscard]] VfxBackend* backend() const { return backend_.get(); }
    [[nodiscard]] std::size_t pendingRequestCount() const { return pending_requests_.size(); }

private:
    std::unique_ptr<VfxBackend> backend_;
    std::vector<VfxPlayRequest> pending_requests_{};
};

} // namespace engine::vfx
