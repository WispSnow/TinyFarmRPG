#include "vfx_debug_panel.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/vfx/vfx_backend.h"
#include "engine/vfx/vfx_service.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>

namespace engine::debug {

VfxDebugPanel::VfxDebugPanel(engine::vfx::VfxService& vfx_service,
                             engine::render::opengl::GLRenderer& gl_renderer)
    : vfx_service_(vfx_service), gl_renderer_(gl_renderer) {}

std::string_view VfxDebugPanel::name() const {
    return "VFX Debug";
}

void VfxDebugPanel::onShow() {
    camera_elevation_deg_ = gl_renderer_.getVfxCameraElevation();
    scanEffectFiles();
}

void VfxDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("VFX Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // --- Effect selection ---
    if (effect_paths_.empty()) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f},
                           "assets/vfx 下未发现 .efkefc/.efk 文件");
        if (ImGui::Button("Rescan")) {
            scanEffectFiles();
        }
        ImGui::End();
        return;
    }

    if (ImGui::BeginCombo("Effect", effect_paths_[selected_effect_].c_str())) {
        for (int i = 0; i < static_cast<int>(effect_paths_.size()); ++i) {
            const bool selected = (i == selected_effect_);
            if (ImGui::Selectable(effect_paths_[i].c_str(), selected)) {
                selected_effect_ = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // --- Spawn parameters ---
    ImGui::DragFloat2("Spawn Position", &spawn_position_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
    ImGui::DragFloat("Spawn Z", &spawn_z_, 0.1f, -100.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Spawn Scale", &spawn_scale_, 0.01f, 0.01f, 100.0f, "%.2f");

    // --- Spawn button ---
    if (ImGui::Button("Spawn")) {
        engine::vfx::VfxPlayRequest request{};
        request.effect_path = effect_paths_[selected_effect_];
        request.world_position = spawn_position_;
        request.z = spawn_z_;
        request.scale = spawn_scale_;
        vfx_service_.submit(request);
    }

    // --- Auto spawn ---
    ImGui::SameLine();
    ImGui::Checkbox("Auto Spawn", &auto_spawn_);
    if (auto_spawn_) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::DragFloat("Interval (s)", &auto_spawn_interval_, 0.1f, 0.1f, 10.0f, "%.1f");

        const float dt = ImGui::GetIO().DeltaTime;
        auto_spawn_timer_ += dt;
        if (auto_spawn_timer_ >= auto_spawn_interval_) {
            auto_spawn_timer_ -= auto_spawn_interval_;
            engine::vfx::VfxPlayRequest request{};
            request.effect_path = effect_paths_[selected_effect_];
            request.world_position = spawn_position_;
            request.z = spawn_z_;
            request.scale = spawn_scale_;
            vfx_service_.submit(request);
        }
    }

    // --- Camera elevation (3D→2D viewing angle) ---
    ImGui::Separator();
    ImGui::Text("Camera");
    if (ImGui::DragFloat("Elevation (deg)", &camera_elevation_deg_, 0.5f, 0.0f, 90.0f, "%.1f")) {
        gl_renderer_.setVfxCameraElevation(camera_elevation_deg_);
    }
    ImGui::TextDisabled("0 = front view (pure 2D), 30~45 = 3/4 RPG perspective");

    // --- Stats ---
    ImGui::Separator();
    auto* backend = vfx_service_.backend();
    if (backend) {
        ImGui::Text("VFX Draw Calls: %u", backend->getLastDrawCallCount());
        ImGui::Text("VFX Instances:  %u", backend->getLastInstanceCount());
    }
    ImGui::Text("Pending Requests: %zu", vfx_service_.pendingRequestCount());

    const auto& vfx_stats = gl_renderer_.getPassStats(
        engine::render::opengl::GLRenderer::PassType::Vfx);
    ImGui::Text("VfxPass Stats: draw=%u sprites=%u",
                vfx_stats.draw_calls, vfx_stats.sprite_count);

    // --- Reset / Rescan ---
    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        selected_effect_ = 0;
        spawn_position_ = {0.0f, 0.0f};
        spawn_z_ = 0.0f;
        spawn_scale_ = 1.0f;
        auto_spawn_ = false;
        auto_spawn_interval_ = 2.0f;
        auto_spawn_timer_ = 0.0f;
        camera_elevation_deg_ = 0.0f;
        gl_renderer_.setVfxCameraElevation(0.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan Effects")) {
        scanEffectFiles();
    }

    ImGui::End();
}

void VfxDebugPanel::scanEffectFiles() {
    effect_paths_.clear();
    selected_effect_ = 0;

    const std::filesystem::path root_dir{"assets/vfx"};
    if (!std::filesystem::exists(root_dir) || !std::filesystem::is_directory(root_dir)) {
        return;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (ext == ".efkefc" || ext == ".efk") {
            effect_paths_.push_back(entry.path().string());
        }
    }
    std::sort(effect_paths_.begin(), effect_paths_.end());
}

} // namespace engine::debug
