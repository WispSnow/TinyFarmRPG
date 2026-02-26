#pragma once

#include "engine/debug/debug_panel.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

namespace engine::render::opengl {
class GLRenderer;
}

namespace engine::vfx {
class VfxService;
class VfxBackend;
}

namespace engine::debug {

class VfxDebugPanel final : public DebugPanel {
public:
    using PlayerPositionProvider = std::function<std::optional<glm::vec2>()>;

    explicit VfxDebugPanel(engine::render::opengl::GLRenderer& gl_renderer);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;
    void onHide() override;

    void setVfxService(engine::vfx::VfxService* vfx_service);
    void clearVfxService();

    void setPlayerPositionProvider(PlayerPositionProvider provider);
    void clearPlayerPositionProvider();

private:
    [[nodiscard]] bool canSpawn() const;
    [[nodiscard]] const char* selectedEffectLabel() const;
    [[nodiscard]] bool syncSpawnPositionToPlayer();

    void rescanEffectFiles();
    void ensureSelectedEffectInRange();
    void resetDefaults();
    void updateAutoSpawn(float delta_seconds);
    void spawnSelectedEffect();
    void spawnBurst();
    void spawnEffectAt(const glm::vec2& world_position);

    engine::render::opengl::GLRenderer& gl_renderer_;
    engine::vfx::VfxService* vfx_service_{nullptr};
    engine::vfx::VfxBackend* backend_{nullptr};
    PlayerPositionProvider player_position_provider_{};

    std::string effect_root_dir_{"assets/vfx"};
    std::vector<std::string> effect_paths_{};

    int selected_effect_{0};
    glm::vec2 spawn_position_{0.0f, 0.0f};
    float spawn_z_{0.0f};
    float spawn_scale_{1.0f};
    bool spawn_loop_{false};

    int burst_count_{8};
    float burst_radius_{120.0f};

    bool auto_spawn_{false};
    float auto_spawn_interval_{2.0f};
    float auto_spawn_timer_{0.0f};

    bool spawn_position_initialized_{false};
};

} // namespace engine::debug
