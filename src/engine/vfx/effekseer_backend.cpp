#include "engine/vfx/effekseer_backend.h"

#ifdef TF_ENABLE_EFFEKSEER

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

constexpr int32_t kMaxSpriteCount = 4096;
constexpr float kFramesPerSecond = 60.0f;

[[nodiscard]] std::u16string toUtf16Path(std::string_view path) {
    return std::filesystem::path(std::string(path)).u16string();
}

[[nodiscard]] Effekseer::Matrix44 toEffekseerMatrix(const glm::mat4& matrix) {
    Effekseer::Matrix44 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.Values[row][col] = matrix[col][row];
        }
    }
    return result;
}

} // namespace

namespace engine::vfx {

std::unique_ptr<EffekseerBackend> EffekseerBackend::create() {
    auto backend = std::unique_ptr<EffekseerBackend>(new EffekseerBackend());
    if (!backend->init()) {
        return nullptr;
    }
    return backend;
}

bool EffekseerBackend::init() {
    renderer_ = EffekseerRendererGL::Renderer::Create(
        kMaxSpriteCount,
        EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
    if (renderer_.Get() == nullptr) {
        spdlog::error("EffekseerBackend: 创建 EffekseerRendererGL::Renderer 失败");
        return false;
    }

    renderer_->SetRestorationOfStatesFlag(true);

    manager_ = Effekseer::Manager::Create(kMaxSpriteCount);
    if (manager_.Get() == nullptr) {
        spdlog::error("EffekseerBackend: 创建 Effekseer::Manager 失败");
        return false;
    }

    manager_->SetSpriteRenderer(renderer_->CreateSpriteRenderer());
    manager_->SetRibbonRenderer(renderer_->CreateRibbonRenderer());
    manager_->SetRingRenderer(renderer_->CreateRingRenderer());
    manager_->SetTrackRenderer(renderer_->CreateTrackRenderer());
    manager_->SetModelRenderer(renderer_->CreateModelRenderer());

    manager_->SetTextureLoader(renderer_->CreateTextureLoader());
    manager_->SetModelLoader(renderer_->CreateModelLoader());
    manager_->SetMaterialLoader(renderer_->CreateMaterialLoader());
    manager_->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());
    return true;
}

void EffekseerBackend::enqueue(const VfxPlayRequest& request) {
    if (manager_.Get() == nullptr || request.effect_path.empty()) {
        return;
    }

    const entt::id_type resolved_effect_id = request.effect_id != kInvalidVfxEffectId
        ? request.effect_id
        : entt::hashed_string{request.effect_path.data(), request.effect_path.size()}.value();

    const auto effect = loadEffect(resolved_effect_id, request.effect_path);
    if (effect.Get() == nullptr) {
        return;
    }

    const auto handle = manager_->Play(effect, request.world_position.x, request.world_position.y, request.z);
    if (handle < 0) {
        spdlog::warn("EffekseerBackend: 播放特效失败 effect='{}'", request.effect_path);
        return;
    }

    if (request.scale != 1.0f) {
        manager_->SetScale(handle, request.scale, request.scale, request.scale);
    }

    // 当前阶段 loop 参数仅作为数据保留，不覆写特效资源内的生命周期配置。
    (void)request.loop;
    active_handles_.push_back(handle);
}

void EffekseerBackend::update(const float delta_time_seconds) {
    if (manager_.Get() == nullptr) {
        return;
    }

    Effekseer::Manager::UpdateParameter update_parameter{};
    update_parameter.DeltaFrame = std::max(0.0f, delta_time_seconds * kFramesPerSecond);
    manager_->Update(update_parameter);

    std::erase_if(active_handles_, [this](const Effekseer::Handle handle) {
        return !manager_->Exists(handle);
    });

    last_instance_count_ = static_cast<std::uint32_t>(active_handles_.size());
}

void EffekseerBackend::render(const VfxRenderContext& context) {
    if (manager_.Get() == nullptr || renderer_.Get() == nullptr) {
        return;
    }

    renderer_->ResetDrawCallCount();
    renderer_->ResetDrawVertexCount();

    // Effekseer 需要分别设置投影矩阵和相机矩阵才能正确渲染。
    // DrawParameter.ViewProjectionMatrix 仅用于剔除，不参与实际顶点变换。
    const auto proj = toEffekseerMatrix(context.view_projection);
    Effekseer::Matrix44 camera_mat;
    camera_mat.Indentity();
    renderer_->SetProjectionMatrix(proj);
    renderer_->SetCameraMatrix(camera_mat);

    if (!renderer_->BeginRendering()) {
        last_draw_call_count_ = 0u;
        return;
    }

    Effekseer::Manager::DrawParameter draw_parameter{};
    draw_parameter.ViewProjectionMatrix = proj;
    draw_parameter.ZNear = 0.0f;
    draw_parameter.ZFar = 1.0f;
    draw_parameter.CameraPosition = Effekseer::Vector3D(0.0f, 0.0f, 1.0f);
    draw_parameter.CameraFrontDirection = Effekseer::Vector3D(0.0f, 0.0f, -1.0f);
    manager_->Draw(draw_parameter);

    renderer_->EndRendering();

    last_draw_call_count_ = static_cast<std::uint32_t>(std::max(renderer_->GetDrawCallCount(), 0));
    last_instance_count_ = static_cast<std::uint32_t>(active_handles_.size());
}

std::uint32_t EffekseerBackend::getLastDrawCallCount() const {
    return last_draw_call_count_;
}

std::uint32_t EffekseerBackend::getLastInstanceCount() const {
    return last_instance_count_;
}

Effekseer::EffectRef EffekseerBackend::loadEffect(const entt::id_type effect_id,
                                                   std::string_view effect_path) {
    if (const auto it = cached_effects_.find(effect_id); it != cached_effects_.end()) {
        return it->second;
    }

    const auto path_utf16 = toUtf16Path(effect_path);
    auto effect = Effekseer::Effect::Create(manager_, path_utf16.c_str());
    if (effect.Get() == nullptr) {
        spdlog::warn("EffekseerBackend: 加载特效失败 '{}'", effect_path);
        return {};
    }

    cached_effects_[effect_id] = effect;
    return effect;
}

} // namespace engine::vfx

#endif // TF_ENABLE_EFFEKSEER
