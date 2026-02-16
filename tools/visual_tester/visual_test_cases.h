#pragma once
#include "visual_test_case.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>

namespace engine::loader {
class LevelLoader;
}

namespace engine::scene {
class Scene;
}

namespace engine::system {
class AutoTileSystem;
}

namespace tools::visual {

class TileAutoTileYSortVisualTest final : public VisualTestCase {
public:
    TileAutoTileYSortVisualTest();
    ~TileAutoTileYSortVisualTest() override;
    void onEnter(engine::core::Context& context) override;
    void onExit(engine::core::Context& context) override;
    void onUpdate(float delta_time, engine::core::Context& context) override;
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void initialize(engine::core::Context& context);

    bool initialized_{false};

    glm::ivec2 map_size_{16, 12};
    glm::ivec2 tile_size_{16, 16};
    std::vector<entt::entity> tile_entities_{};

    glm::ivec2 hovered_tile_{-1, -1};
    glm::vec2 hovered_world_{0.0f, 0.0f};

    bool allow_tile_toggle_{true};
    bool show_grid_{true};
    bool show_hover_highlight_{true};
    bool show_ysort_demo_{true};

    entt::entity actor_entity_{entt::null};
    entt::entity tree_entity_{entt::null};
    glm::vec2 actor_base_pos_{128.0f, 120.0f};
    float actor_y_{120.0f};

    glm::vec2 saved_camera_pos_{0.0f, 0.0f};
    float saved_camera_zoom_{1.0f};
    float saved_camera_rotation_{0.0f};
    bool saved_camera_{false};

    std::unique_ptr<engine::scene::Scene> scene_{};
    std::unique_ptr<engine::loader::LevelLoader> level_loader_{};
    std::unique_ptr<engine::system::AutoTileSystem> auto_tile_system_{};
};

class RenderPassCoverageVisualTest final : public VisualTestCase {
public:
    RenderPassCoverageVisualTest();
    void onEnter(engine::core::Context& context) override;
    void onExit(engine::core::Context& context) override;
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults(engine::core::Context& context);

    bool saved_{false};
    bool saved_point_lights_enabled_{true};
    bool saved_spot_lights_enabled_{true};
    bool saved_directional_lights_enabled_{true};
    bool saved_bloom_enabled_{true};
    bool saved_emissive_enabled_{true};
    bool saved_pixel_snap_enabled_{true};
    glm::vec3 saved_ambient_{0.0f, 0.0f, 0.0f};
    float saved_bloom_strength_{1.0f};
    float saved_bloom_sigma_{1.0f};

    float emissive_intensity_{0.8f};
};

class CameraViewportClippingVisualTest final : public VisualTestCase {
public:
    CameraViewportClippingVisualTest();
    void onEnter(engine::core::Context& context) override;
    void onExit(engine::core::Context& context) override;
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void rebuildMarkers();
    void applyDefaults(engine::core::Context& context);

    bool saved_{false};
    bool saved_viewport_clipping_enabled_{true};
    glm::vec2 saved_camera_pos_{0.0f, 0.0f};
    float saved_camera_zoom_{1.0f};
    float saved_camera_rotation_{0.0f};

    bool show_view_outline_{true};
    bool show_culling_rect_{true};
    bool show_axes_{true};
    bool show_marker_grid_{true};
    bool show_stats_{true};

    int marker_half_extent_{20};
    int marker_step_{64};
    glm::vec2 marker_size_{10.0f, 10.0f};

    std::vector<glm::vec2> marker_positions_{};
};

class SpriteAndPrimitiveVisualTest final : public VisualTestCase {
public:
    SpriteAndPrimitiveVisualTest();
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults();

    bool show_circle_{true};
    bool show_gradient_rect_{true};
    bool show_sprite_{true};

    float circle_radius_{100.0f};
    glm::vec2 rect_pos_{300.0f, 300.0f};
    glm::vec2 rect_size_{300.0f, 300.0f};
    float rect_gradient_angle_deg_{65.0f};
    glm::vec2 sprite_pos_{-100.0f, -100.0f};
    glm::vec2 sprite_size_{128.0f, 32.0f};
};

class LightingVisualTest final : public VisualTestCase {
public:
    LightingVisualTest();
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults();

    bool enable_point_{true};
    bool enable_spot_{true};
    bool enable_directional_{true};

    glm::vec2 point_pos_{300.0f, 300.0f};
    float point_radius_{200.0f};
    float point_intensity_{1.0f};

    glm::vec2 spot_pos_{200.0f, 200.0f};
    glm::vec2 spot_dir_{0.0f, -1.0f};
    float spot_radius_{300.0f};
    float spot_intensity_{1.0f};

    glm::vec2 dir_light_dir_{0.0f, -1.0f};
    float dir_light_intensity_{1.0f};
};

class EmissiveVisualTest final : public VisualTestCase {
public:
    EmissiveVisualTest();
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults();

    bool show_rect_{true};
    bool show_sprite_{true};
    bool show_cursor_{true};

    float rect_intensity_{0.3f};
    float sprite_intensity_{1.0f};
    float cursor_intensity_{1.0f};
};

class UiVisualTest final : public VisualTestCase {
public:
    UiVisualTest();
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults();

    bool show_ui_image_{true};
    bool show_ui_rect_{true};
    glm::vec2 ui_image_pos_{-200.0f, -300.0f};
    glm::vec2 ui_image_size_{100.0f, 100.0f};
    glm::vec2 ui_rect_pos_{-100.0f, -100.0f};
    glm::vec2 ui_rect_size_{100.0f, 100.0f};
    glm::vec4 ui_rect_color_{0.5f, 0.5f, 0.5f, 1.0f};
};

class TextRenderingVisualTest final : public VisualTestCase {
private:
    bool font_loaded_{false};
    entt::id_type font_id_{0};
    int font_size_{16};
    const char* font_path_{"assets/fonts/VonwaonBitmap-16px.ttf"};

    bool draw_world_{true};
    bool draw_ui_{true};
    bool show_bounds_{true};

    int alignment_{0}; // 0=left,1=center,2=right
    glm::vec2 world_anchor_{-220.0f, -160.0f};
    glm::vec2 ui_anchor_{-220.0f, 20.0f};

    bool use_gradient_{true};
    float gradient_angle_deg_{45.0f};
    glm::vec4 gradient_start_{1.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 gradient_end_{1.0f, 0.0f, 0.0f, 1.0f};

    float letter_spacing_{0.0f};
    float line_spacing_scale_{1.0f};
    glm::vec2 glyph_scale_{1.0f, 1.0f};

public:
    TextRenderingVisualTest();
    void onEnter(engine::core::Context& context) override;
    void onExit(engine::core::Context& context) override;
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;

private:
    void applyDefaults();
    void ensureFontLoaded(engine::core::Context& context);
};

class AudioVisualTest final : public VisualTestCase {
private:
    entt::id_type music_id_{entt::hashed_string::value("assets/audio/01_spring_journey.ogg")};
    entt::id_type sound_id_{entt::hashed_string::value("assets/audio/calf-and-cow.wav")};
    bool audio_loaded_{false};
    const char* music_path_{"assets/audio/01_spring_journey.ogg"};
    const char* sound_path_{"assets/audio/calf-and-cow.wav"};

public:
    AudioVisualTest();
    void onEnter(engine::core::Context& context) override;
    void onExit(engine::core::Context& context) override;
    void onRender(engine::core::Context& context) override;
    void onImGui(engine::core::Context& context) override;
};

} // namespace tools::visual
