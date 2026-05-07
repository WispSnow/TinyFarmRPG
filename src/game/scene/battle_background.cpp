#include "game/scene/battle_background.h"

#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/resource/resource_manager.h"

#include <entt/core/hashed_string.hpp>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>

namespace game::scene {
namespace {

constexpr std::string_view BATTLE_BACKGROUND_ROOT = "assets/textures/BattleBg";
/// @brief RPG Maker 1000x740 远景顶部通常有云层留白；略向下采样可让山脉/地平线在 640x256 战场中更自然。
/// @note 调整该经验值时需同步更新 BattleBackgroundTest.BackdropCropSamplesRpgMakerImageTopWithoutStretch。
constexpr float BACKDROP_VERTICAL_SAMPLE_OFFSET_RATIO = 0.18f;

[[nodiscard]] const char* layerDirectory(const BattleBackgroundLayerKind kind) {
    switch (kind) {
        case BattleBackgroundLayerKind::Ground:
            return "battlebacks1";
        case BattleBackgroundLayerKind::Backdrop:
            return "battlebacks2";
    }
    return "battlebacks2";
}

[[nodiscard]] const char* layerTextureIdPrefix(const BattleBackgroundLayerKind kind) {
    switch (kind) {
        case BattleBackgroundLayerKind::Ground:
            return "battlebg1:";
        case BattleBackgroundLayerKind::Backdrop:
            return "battlebg2:";
    }
    return "battlebg2:";
}

[[nodiscard]] const char* layerLabel(const BattleBackgroundLayerKind kind) {
    switch (kind) {
        case BattleBackgroundLayerKind::Ground:
            return "battlebacks1";
        case BattleBackgroundLayerKind::Backdrop:
            return "battlebacks2";
    }
    return "battlebacks2";
}

[[nodiscard]] engine::utils::Rect screenRectToWorldRect(const engine::render::Camera& camera,
                                                        const engine::utils::Rect& screen_rect) {
    const glm::vec2 top_left = camera.screenToWorld(screen_rect.pos);
    const glm::vec2 bottom_right = camera.screenToWorld(screen_rect.pos + screen_rect.size);
    return engine::utils::Rect{top_left, bottom_right - top_left};
}

void drawLayer(engine::render::Renderer& renderer,
               const engine::render::Camera& camera,
               const BattleBackgroundLayer& layer,
               const BattleBackgroundDrawRect& draw_rect) {
    if (!layer.valid) {
        return;
    }

    const engine::component::Sprite sprite{layer.sprite.texture_id_, draw_rect.source_rect};
    const engine::utils::Rect world_rect = screenRectToWorldRect(camera, draw_rect.screen_rect);
    renderer.drawSprite(sprite, world_rect.pos, world_rect.size);
}

} // namespace

bool isValidBattleBackgroundId(const std::string_view id) {
    return game::data::isValidBattleBackgroundId(id);
}

BattleBackgroundLayerRef makeBattleBackgroundLayerRef(const std::string_view id,
                                                      const BattleBackgroundLayerKind kind) {
    BattleBackgroundLayerRef ref{};
    ref.kind = kind;
    if (!isValidBattleBackgroundId(id)) {
        return ref;
    }

    std::string key{layerTextureIdPrefix(kind)};
    key += id;
    ref.texture_id = entt::hashed_string{key.c_str(), key.size()}.value();
    ref.path = std::string{BATTLE_BACKGROUND_ROOT};
    ref.path += '/';
    ref.path += layerDirectory(kind);
    ref.path += '/';
    ref.path += id;
    ref.path += ".png";
    return ref;
}

std::optional<BattleBackgroundDrawRect> computeTopAnchoredCropDrawRect(
    const glm::vec2 source_size,
    const engine::utils::Rect target_rect) {
    if (source_size.x <= 0.0f || source_size.y <= 0.0f ||
        target_rect.size.x <= 0.0f || target_rect.size.y <= 0.0f) {
        return std::nullopt;
    }

    const float target_aspect = target_rect.size.x / target_rect.size.y;
    const float source_aspect = source_size.x / source_size.y;
    engine::utils::Rect source_rect{glm::vec2{0.0f, 0.0f}, source_size};
    if (source_aspect > target_aspect) {
        source_rect.size.x = source_size.y * target_aspect;
        source_rect.pos.x = (source_size.x - source_rect.size.x) * 0.5f;
    } else {
        source_rect.size.y = source_size.x / target_aspect;
        source_rect.pos.y = (source_size.y - source_rect.size.y) * BACKDROP_VERTICAL_SAMPLE_OFFSET_RATIO;
    }

    return BattleBackgroundDrawRect{
        .source_rect = source_rect,
        .screen_rect = target_rect};
}

std::optional<BattleBackgroundDrawRect> computeBottomAnchoredCropDrawRect(
    const glm::vec2 source_size,
    const engine::utils::Rect target_rect) {
    if (source_size.x <= 0.0f || source_size.y <= 0.0f ||
        target_rect.size.x <= 0.0f || target_rect.size.y <= 0.0f) {
        return std::nullopt;
    }

    const float target_aspect = target_rect.size.x / target_rect.size.y;
    const float source_aspect = source_size.x / source_size.y;
    engine::utils::Rect source_rect{glm::vec2{0.0f, 0.0f}, source_size};
    if (source_aspect > target_aspect) {
        source_rect.size.x = source_size.y * target_aspect;
        source_rect.pos.x = (source_size.x - source_rect.size.x) * 0.5f;
    } else {
        source_rect.size.y = source_size.x / target_aspect;
        source_rect.pos.y = source_size.y - source_rect.size.y;
    }

    return BattleBackgroundDrawRect{
        .source_rect = source_rect,
        .screen_rect = target_rect};
}

void BattleBackgroundRenderer::load(std::string_view id, engine::resource::ResourceManager& resource_manager) {
    id_ = std::string{id};
    backdrop_ = {};
    ground_ = {};

    if (!isValidBattleBackgroundId(id_)) {
        spdlog::warn("BattleBackgroundRenderer: battle_background_id='{}' 非法，回退纯色背景。", id_);
        id_.clear();
        return;
    }

    backdrop_ = loadLayer(id_, BattleBackgroundLayerKind::Backdrop, resource_manager);
    ground_ = loadLayer(id_, BattleBackgroundLayerKind::Ground, resource_manager);
    if (!backdrop_.valid && !ground_.valid) {
        spdlog::warn("BattleBackgroundRenderer: battle_background_id='{}' 双层资源均加载失败，回退纯色背景。", id_);
    }
}

void BattleBackgroundRenderer::render(engine::render::Renderer& renderer, const engine::render::Camera& camera) const {
    const glm::vec2 logical_size = camera.getLogicalSize();
    const engine::utils::Rect screen_rect{glm::vec2{0.0f, 0.0f}, logical_size};

    if (ground_.valid) {
        if (const auto draw_rect = computeBottomAnchoredCropDrawRect(ground_.texture_size, screen_rect)) {
            drawLayer(renderer, camera, ground_, *draw_rect);
        }
    }

    if (backdrop_.valid) {
        if (const auto draw_rect = computeTopAnchoredCropDrawRect(backdrop_.texture_size, screen_rect)) {
            drawLayer(renderer, camera, backdrop_, *draw_rect);
        }
    }
}

bool BattleBackgroundRenderer::hasAnyLayer() const {
    return backdrop_.valid || ground_.valid;
}

BattleBackgroundLayer BattleBackgroundRenderer::loadLayer(
    const std::string_view id,
    const BattleBackgroundLayerKind kind,
    engine::resource::ResourceManager& resource_manager) const {
    BattleBackgroundLayer layer{};
    layer.ref = makeBattleBackgroundLayerRef(id, kind);
    if (layer.ref.texture_id == entt::null || layer.ref.path.empty()) {
        return layer;
    }

    if (!std::filesystem::exists(layer.ref.path)) {
        return layer;
    }

    layer.texture = resource_manager.loadTexture(layer.ref.texture_id, layer.ref.path);
    if (!layer.texture) {
        return layer;
    }

    layer.texture_size = glm::vec2{static_cast<float>(layer.texture->width),
                                   static_cast<float>(layer.texture->height)};
    if (layer.texture_size.x <= 0.0f || layer.texture_size.y <= 0.0f ||
        !std::isfinite(layer.texture_size.x) || !std::isfinite(layer.texture_size.y)) {
        spdlog::warn("BattleBackgroundRenderer: {} 背景层尺寸非法 id='{}', path='{}'",
                     layerLabel(kind),
                     id,
                     layer.ref.path);
        return layer;
    }

    layer.sprite = engine::component::Sprite{
        layer.ref.texture_id,
        engine::utils::Rect{glm::vec2{0.0f, 0.0f}, layer.texture_size}};
    layer.valid = true;
    return layer;
}

} // namespace game::scene
