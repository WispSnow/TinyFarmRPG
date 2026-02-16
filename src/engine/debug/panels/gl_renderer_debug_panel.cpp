#include "gl_renderer_debug_panel.h"
#include "engine/render/opengl/gl_renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>

namespace engine::render::opengl {

namespace {
ImTextureID toImTextureID(GLuint texture) {
    return static_cast<ImTextureID>(static_cast<uintptr_t>(texture));
}
} // namespace

GLRendererDebugPanel::GLRendererDebugPanel(GLRenderer& renderer)
    : renderer_(renderer) {
    syncFromRenderer();
}

std::string_view GLRendererDebugPanel::name() const {
    return "OpenGL Renderer";
}

void GLRendererDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("OpenGL Renderer Debug", &is_open)) {
        ImGui::End();
        return;
    }

    swap_interval_ = renderer_.getSwapInterval();
    vsync_enabled_ = swap_interval_ > 0;
    pixel_snap_enabled_ = renderer_.isPixelSnapEnabled();
    viewport_clipping_enabled_ = renderer_.isViewportClippingEnabled();

    if (ImGui::CollapsingHeader("Presentation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Swap Interval: %d", swap_interval_);
        if (ImGui::Checkbox("VSync", &vsync_enabled_)) {
            renderer_.setVSyncEnabled(vsync_enabled_);
            swap_interval_ = renderer_.getSwapInterval();
        }
        if (ImGui::Checkbox("Pixel Snap", &pixel_snap_enabled_)) {
            renderer_.setPixelSnapEnabled(pixel_snap_enabled_);
        }

        ImGui::Separator();
        const auto logical_size = renderer_.getLogicalSize();
        const auto window_size_pixels = renderer_.getWindowSizePixels();
        const auto viewport_pixels = renderer_.getViewportPixels();
        const auto letterbox = renderer_.getLetterboxMetricsPixels();
        ImGui::Text("Logical Size: %.0f x %.0f", logical_size.x, logical_size.y);
        ImGui::Text("Window Size (pixels): %.0f x %.0f", window_size_pixels.x, window_size_pixels.y);
        ImGui::Text("Viewport (pixels): (%.0f, %.0f)  %.0f x %.0f",
                    viewport_pixels.pos.x, viewport_pixels.pos.y,
                    viewport_pixels.size.x, viewport_pixels.size.y);
        ImGui::Text("Letterbox Scale (px/logical): %.3f", letterbox.scale);

        if (ImGui::Checkbox("Viewport Clipping", &viewport_clipping_enabled_)) {
            renderer_.setViewportClippingEnabled(viewport_clipping_enabled_);
        }

        if (const auto view_rect = renderer_.getCurrentViewRect(); view_rect.has_value()) {
            ImGui::Text("View Rect (world): (%.1f, %.1f)  %.1f x %.1f",
                        view_rect->pos.x, view_rect->pos.y,
                        view_rect->size.x, view_rect->size.y);
        } else {
            ImGui::TextUnformatted("View Rect (world): <unavailable>");
        }
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Point Lights", &point_lights_enabled_)) {
            renderer_.setPointLightsEnabled(point_lights_enabled_);
        }
        if (ImGui::Checkbox("Spot Lights", &spot_lights_enabled_)) {
            renderer_.setSpotLightsEnabled(spot_lights_enabled_);
        }
        if (ImGui::Checkbox("Directional Lights", &directional_lights_enabled_)) {
            renderer_.setDirectionalLightsEnabled(directional_lights_enabled_);
        }
        if (ImGui::Checkbox("Emissive", &emissive_enabled_)) {
            renderer_.setEmissiveEnabled(emissive_enabled_);
        }
    }

    if (ImGui::CollapsingHeader("Ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::ColorEdit3("Ambient Color", glm::value_ptr(ambient_), ImGuiColorEditFlags_Float)) {
            renderer_.setAmbient(ambient_);
        }
    }

    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Bloom Enabled", &bloom_enabled_)) {
            renderer_.setBloomEnabled(bloom_enabled_);
        }

        const auto& bloom_stats = renderer_.getPassStats(GLRenderer::PassType::Bloom);
        ImGui::Text("Bloom Levels (last frame): %u", renderer_.getBloomLevelCount());
        ImGui::Text("Bloom Draw Calls (last frame): %u", bloom_stats.draw_calls);

        ImGui::BeginDisabled(!bloom_enabled_);
        if (ImGui::SliderFloat("Bloom Strength", &bloom_strength_, 0.0f, 5.0f, "%.2f")) {
            renderer_.setBloomStrength(bloom_strength_);
        }
        if (ImGui::SliderFloat("Blur Sigma", &bloom_sigma_, 0.5f, 12.0f, "%.2f")) {
            renderer_.setBloomSigma(bloom_sigma_);
        }
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Pass Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        const struct {
            const char* label;
            GLRenderer::PassType type;
        } rows[] = {
            {"ScenePass", GLRenderer::PassType::Scene},
            {"LightingPass", GLRenderer::PassType::Lighting},
            {"EmissivePass", GLRenderer::PassType::Emissive},
            {"BloomPass", GLRenderer::PassType::Bloom},
            {"UIPass", GLRenderer::PassType::UI}
        };

        uint32_t total_draws = 0;
        uint32_t total_sprites = 0;
        uint32_t total_vertices = 0;
        uint32_t total_indices = 0;

        constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("PassStatsTable", 5, table_flags)) {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Sprites");
            ImGui::TableSetupColumn("Draw Calls");
            ImGui::TableSetupColumn("Vertices");
            ImGui::TableSetupColumn("Indices");
            ImGui::TableHeadersRow();

            for (const auto& row : rows) {
                const auto& stats = renderer_.getPassStats(row.type);
                total_draws += stats.draw_calls;
                total_sprites += stats.sprite_count;
                total_vertices += stats.vertex_count;
                total_indices += stats.index_count;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", stats.sprite_count);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", stats.draw_calls);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", stats.vertex_count);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%u", stats.index_count);
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Total");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", total_sprites);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", total_draws);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", total_vertices);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", total_indices);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Pass Preview")) {
        const auto logical_size = renderer_.getLogicalSize();
        const float aspect = (logical_size.x > 0.0f) ? (logical_size.y / logical_size.x) : 1.0f;
        const float preview_w = 220.0f;
        const ImVec2 preview_size{preview_w, preview_w * aspect};
        const ImVec2 uv0{0.0f, 1.0f};
        const ImVec2 uv1{1.0f, 0.0f};

        auto drawTexturePreview = [&](const char* label, GLuint texture) {
            ImGui::TextUnformatted(label);
            if (texture == 0) {
                ImGui::TextUnformatted("<no texture>");
                return;
            }
            ImGui::Image(toImTextureID(texture), preview_size, uv0, uv1);
        };

        constexpr ImGuiTableFlags preview_table_flags = ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("PassPreviewTable", 2, preview_table_flags)) {
            ImGui::TableNextColumn();
            drawTexturePreview("Scene", renderer_.getSceneColorTex());
            ImGui::TableNextColumn();
            drawTexturePreview("Light", renderer_.getLightColorTex());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawTexturePreview("Emissive", renderer_.getEmissiveColorTex());
            ImGui::TableNextColumn();
            drawTexturePreview("Bloom", renderer_.getBloomTex());

            ImGui::EndTable();
        }
    }

    ImGui::End();

}

void GLRendererDebugPanel::onShow() {
    syncFromRenderer();
}

void GLRendererDebugPanel::syncFromRenderer() {
    swap_interval_ = renderer_.getSwapInterval();
    vsync_enabled_ = swap_interval_ > 0;
    pixel_snap_enabled_ = renderer_.isPixelSnapEnabled();
    viewport_clipping_enabled_ = renderer_.isViewportClippingEnabled();
    point_lights_enabled_ = renderer_.isPointLightsEnabled();
    spot_lights_enabled_ = renderer_.isSpotLightsEnabled();
    directional_lights_enabled_ = renderer_.isDirectionalLightsEnabled();
    emissive_enabled_ = renderer_.isEmissiveEnabled();
    bloom_enabled_ = renderer_.isBloomEnabled();
    ambient_ = renderer_.getAmbient();
    bloom_strength_ = renderer_.getBloomStrength();
    bloom_sigma_ = renderer_.getBloomSigma();
}

} // namespace engine::render::opengl
