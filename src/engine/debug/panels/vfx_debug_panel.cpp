#include "vfx_debug_panel.h"

#include "engine/render/opengl/gl_renderer.h"
#include "engine/vfx/vfx_service.h"
#include "engine/vfx/vfx_types.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace engine::debug {

namespace {

[[nodiscard]] std::vector<std::string> scanEffectFiles(const std::string_view root_dir) {
    std::vector<std::string> paths{};
    if (root_dir.empty()) {
        return paths;
    }

    const std::filesystem::path root_path{root_dir};
    std::error_code ec{};
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        return paths;
    }

    std::filesystem::recursive_directory_iterator it{
        root_path,
        std::filesystem::directory_options::skip_permission_denied,
        ec};
    const std::filesystem::recursive_directory_iterator end{};
    while (!ec && it != end) {
        const auto& entry = *it;
        if (entry.is_regular_file(ec)) {
            const auto ext = entry.path().extension().string();
            if (ext == ".efkefc" || ext == ".efk") {
                paths.push_back(entry.path().string());
            }
        }
        it.increment(ec);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace

VfxDebugPanel::VfxDebugPanel(engine::render::opengl::GLRenderer& gl_renderer)
    : gl_renderer_(gl_renderer) {
}

std::string_view VfxDebugPanel::name() const {
    return "VFX";
}

void VfxDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    updateAutoSpawn(std::max(0.0f, ImGui::GetIO().DeltaTime));

    if (!ImGui::Begin("VFX Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    backend_ = vfx_service_ ? vfx_service_->backend() : nullptr;

    const bool service_ready = vfx_service_ != nullptr;
    const bool backend_ready = backend_ != nullptr;

    ImGui::Text("VfxService: %s", service_ready ? "Ready" : "Unavailable");
    ImGui::Text("VfxBackend: %s", backend_ready ? "Ready" : "Unavailable");
    ImGui::Separator();

    if (ImGui::Button("Rescan Effects")) {
        rescanEffectFiles();
    }
    ImGui::SameLine();
    ImGui::Text("Effects: %zu", effect_paths_.size());

    if (effect_paths_.empty()) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f},
                           "No .efkefc/.efk files under %s",
                           effect_root_dir_.c_str());
    } else if (ImGui::BeginCombo("Effect", selectedEffectLabel())) {
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

    ImGui::Separator();

    int spawn_channel_index = spawn_channel_ == engine::vfx::VfxChannel::World ? 0 : 1;
    if (ImGui::Combo("Spawn Channel", &spawn_channel_index, "World\0Overlay\0")) {
        spawn_channel_ = spawn_channel_index == 0 ? engine::vfx::VfxChannel::World : engine::vfx::VfxChannel::Overlay;
        if (spawn_channel_ == engine::vfx::VfxChannel::World) {
            (void)syncSpawnPositionToPlayer();
        }
    }

    const char* spawn_position_label = spawn_channel_ == engine::vfx::VfxChannel::World
        ? "Spawn Position (World)"
        : "Spawn Position (Overlay Logical)";
    if (ImGui::DragFloat2(spawn_position_label, &spawn_position_.x, 1.0f, -50000.0f, 50000.0f, "%.1f")) {
        spawn_position_initialized_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(spawn_channel_ != engine::vfx::VfxChannel::World);
    if (ImGui::Button("Player Position")) {
        (void)syncSpawnPositionToPlayer();
    }
    ImGui::EndDisabled();

    ImGui::DragFloat("Spawn Z", &spawn_z_, 0.1f, -1000.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("Spawn Scale", &spawn_scale_, 0.01f, 0.01f, 20.0f, "%.2f");
    ImGui::Checkbox("Loop", &spawn_loop_);

    ImGui::SliderInt("Burst Count", &burst_count_, 1, 64);
    if (ImGui::DragFloat("Burst Radius", &burst_radius_, 1.0f, 0.0f, 5000.0f, "%.1f")) {
        burst_radius_ = std::max(0.0f, burst_radius_);
    }

    const bool can_spawn = canSpawn();
    ImGui::BeginDisabled(!can_spawn);
    if (ImGui::Button("Spawn")) {
        spawnSelectedEffect();
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst Spawn")) {
        spawnBurst();
    }
    ImGui::EndDisabled();

    ImGui::Checkbox("Auto Spawn", &auto_spawn_);
    if (auto_spawn_) {
        if (ImGui::DragFloat("Interval (s)", &auto_spawn_interval_, 0.1f, 0.1f, 10.0f, "%.1f")) {
            auto_spawn_interval_ = std::max(0.1f, auto_spawn_interval_);
        }
    }

    ImGui::Separator();
    if (vfx_service_) {
        ImGui::Text("Pending Requests: %zu", vfx_service_->pendingRequestCount());
    }
    if (backend_) {
        ImGui::Text("Backend Draw Calls: %u", backend_->getLastDrawCallCount());
        ImGui::Text("Backend Instances: %u", backend_->getLastInstanceCount());
    }

    const auto& world_vfx_stats =
        gl_renderer_.getPassStats(engine::render::opengl::GLRenderer::PassType::WorldVfx);
    const auto& overlay_vfx_stats =
        gl_renderer_.getPassStats(engine::render::opengl::GLRenderer::PassType::OverlayVfx);
    ImGui::Text("WorldVfxPass Stats: draw=%u sprites=%u", world_vfx_stats.draw_calls, world_vfx_stats.sprite_count);
    ImGui::Text(
        "OverlayVfxPass Stats: draw=%u sprites=%u", overlay_vfx_stats.draw_calls, overlay_vfx_stats.sprite_count);

    if (ImGui::Button("Reset Defaults")) {
        resetDefaults();
    }

    ImGui::End();
}

void VfxDebugPanel::onShow() {
    backend_ = vfx_service_ ? vfx_service_->backend() : nullptr;
    if (effect_paths_.empty()) {
        rescanEffectFiles();
    }
    if (!spawn_position_initialized_) {
        (void)syncSpawnPositionToPlayer();
    }
}

void VfxDebugPanel::onHide() {
    auto_spawn_timer_ = 0.0f;
}

void VfxDebugPanel::setVfxService(engine::vfx::VfxService* vfx_service) {
    vfx_service_ = vfx_service;
    backend_ = vfx_service_ ? vfx_service_->backend() : nullptr;
}

void VfxDebugPanel::clearVfxService() {
    vfx_service_ = nullptr;
    backend_ = nullptr;
    auto_spawn_ = false;
    auto_spawn_timer_ = 0.0f;
}

void VfxDebugPanel::setPlayerPositionProvider(PlayerPositionProvider provider) {
    player_position_provider_ = std::move(provider);
    spawn_position_initialized_ = false;
    (void)syncSpawnPositionToPlayer();
}

void VfxDebugPanel::clearPlayerPositionProvider() {
    player_position_provider_ = {};
}

bool VfxDebugPanel::canSpawn() const {
    return vfx_service_ != nullptr && !effect_paths_.empty() && selected_effect_ >= 0 &&
           selected_effect_ < static_cast<int>(effect_paths_.size());
}

const char* VfxDebugPanel::selectedEffectLabel() const {
    if (effect_paths_.empty()) {
        return "<none>";
    }
    const auto index = std::clamp(selected_effect_, 0, static_cast<int>(effect_paths_.size()) - 1);
    return effect_paths_[index].c_str();
}

bool VfxDebugPanel::syncSpawnPositionToPlayer() {
    if (!player_position_provider_) {
        return false;
    }
    const auto position = player_position_provider_();
    if (!position.has_value()) {
        return false;
    }
    spawn_position_ = *position;
    spawn_position_initialized_ = true;
    return true;
}

void VfxDebugPanel::rescanEffectFiles() {
    effect_paths_ = scanEffectFiles(effect_root_dir_);
    ensureSelectedEffectInRange();
}

void VfxDebugPanel::ensureSelectedEffectInRange() {
    if (effect_paths_.empty()) {
        selected_effect_ = 0;
        return;
    }
    selected_effect_ = std::clamp(selected_effect_, 0, static_cast<int>(effect_paths_.size()) - 1);
}

void VfxDebugPanel::resetDefaults() {
    selected_effect_ = 0;
    spawn_z_ = 0.0f;
    spawn_scale_ = 1.0f;
    spawn_loop_ = false;
    spawn_channel_ = engine::vfx::VfxChannel::Overlay;
    burst_count_ = 8;
    burst_radius_ = 120.0f;
    auto_spawn_ = false;
    auto_spawn_interval_ = 2.0f;
    auto_spawn_timer_ = 0.0f;

    spawn_position_initialized_ = false;
    if (!syncSpawnPositionToPlayer()) {
        spawn_position_ = glm::vec2{0.0f, 0.0f};
    }
}

void VfxDebugPanel::updateAutoSpawn(const float delta_seconds) {
    if (!auto_spawn_ || !canSpawn()) {
        return;
    }
    auto_spawn_timer_ += delta_seconds;
    const float interval = std::max(0.1f, auto_spawn_interval_);
    while (auto_spawn_timer_ >= interval) {
        auto_spawn_timer_ -= interval;
        spawnSelectedEffect();
    }
}

void VfxDebugPanel::spawnSelectedEffect() {
    if (!canSpawn()) {
        return;
    }
    spawnEffectAt(spawn_position_);
}

void VfxDebugPanel::spawnBurst() {
    if (!canSpawn()) {
        return;
    }
    const int count = std::max(1, burst_count_);
    if (count == 1 || burst_radius_ <= 0.0f) {
        spawnEffectAt(spawn_position_);
        return;
    }

    constexpr float kTau = 6.2831853071795864769f;
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count);
        const float angle = t * kTau;
        const glm::vec2 offset{std::cos(angle) * burst_radius_, std::sin(angle) * burst_radius_};
        spawnEffectAt(spawn_position_ + offset);
    }
}

void VfxDebugPanel::spawnEffectAt(const glm::vec2& world_position) {
    if (!vfx_service_ || effect_paths_.empty()) {
        return;
    }

    const int index = std::clamp(selected_effect_, 0, static_cast<int>(effect_paths_.size()) - 1);

    engine::vfx::VfxPlayRequest request{};
    request.effect_path = effect_paths_[index];
    request.world_position = world_position;
    request.z = spawn_z_;
    request.scale = spawn_scale_;
    request.loop = spawn_loop_;
    request.channel = spawn_channel_;
    vfx_service_->submit(request);
}

} // namespace engine::debug
