#pragma once

#include "engine/vfx/vfx_backend.h"

#ifdef TF_ENABLE_EFFEKSEER

#include <Effekseer.h>
#include <EffekseerRendererGL.h>

#include <entt/entity/fwd.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::vfx {

class EffekseerBackend final : public VfxBackend {
public:
    [[nodiscard]] static std::unique_ptr<EffekseerBackend> create();
    ~EffekseerBackend() override = default;

    void enqueue(const VfxPlayRequest& request) override;
    void update(float delta_time_seconds) override;
    void render(const VfxRenderContext& context) override;

    [[nodiscard]] std::uint32_t getLastDrawCallCount() const override;
    [[nodiscard]] std::uint32_t getLastInstanceCount() const override;

private:
    EffekseerBackend() = default;

    [[nodiscard]] bool init();
    [[nodiscard]] Effekseer::EffectRef loadEffect(entt::id_type effect_id, std::string_view effect_path);

    EffekseerRendererGL::RendererRef renderer_{};
    Effekseer::ManagerRef manager_{};
    std::unordered_map<entt::id_type, Effekseer::EffectRef> cached_effects_{};
    std::vector<Effekseer::Handle> active_handles_{};

    std::uint32_t last_draw_call_count_{0u};
    std::uint32_t last_instance_count_{0u};
};

} // namespace engine::vfx

#endif // TF_ENABLE_EFFEKSEER
