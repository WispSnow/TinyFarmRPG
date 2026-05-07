#pragma once

#include "engine/component/sprite_component.h"
#include "engine/resource/texture_loader.h"
#include "engine/utils/math.h"
#include "game/data/battle_background_id.h"

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace engine::render {
class Camera;
class Renderer;
}

namespace engine::resource {
class ResourceManager;
}

namespace game::scene {

enum class BattleBackgroundLayerKind {
    Ground,
    Backdrop
};

struct BattleBackgroundLayerRef {
    BattleBackgroundLayerKind kind{BattleBackgroundLayerKind::Backdrop};
    entt::id_type texture_id{entt::null};
    std::string path{};
};

struct BattleBackgroundLayer {
    BattleBackgroundLayerRef ref{};
    engine::resource::TextureHandle texture{};
    engine::component::Sprite sprite{};
    glm::vec2 texture_size{0.0f, 0.0f};
    bool valid{false};
};

struct BattleBackgroundDrawRect {
    engine::utils::Rect source_rect{};
    engine::utils::Rect screen_rect{};
};

[[nodiscard]] bool isValidBattleBackgroundId(std::string_view id);
[[nodiscard]] BattleBackgroundLayerRef makeBattleBackgroundLayerRef(std::string_view id,
                                                                    BattleBackgroundLayerKind kind);
[[nodiscard]] std::optional<BattleBackgroundDrawRect> computeTopAnchoredCropDrawRect(
    glm::vec2 source_size,
    engine::utils::Rect target_rect);
[[nodiscard]] std::optional<BattleBackgroundDrawRect> computeBottomAnchoredCropDrawRect(
    glm::vec2 source_size,
    engine::utils::Rect target_rect);

class BattleBackgroundRenderer final {
public:
    void load(std::string_view id, engine::resource::ResourceManager& resource_manager);
    void render(engine::render::Renderer& renderer, const engine::render::Camera& camera) const;

    [[nodiscard]] bool hasAnyLayer() const;
    [[nodiscard]] std::string_view id() const { return id_; }

private:
    [[nodiscard]] BattleBackgroundLayer loadLayer(std::string_view id,
                                                  BattleBackgroundLayerKind kind,
                                                  engine::resource::ResourceManager& resource_manager) const;

    std::string id_{};
    BattleBackgroundLayer backdrop_{};
    BattleBackgroundLayer ground_{};
};

} // namespace game::scene
