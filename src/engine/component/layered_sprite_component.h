#pragma once

#include <entt/core/fwd.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::component {

struct LayeredSpriteLayer {
    static constexpr entt::id_type INVALID_TEXTURE_ID{};

    std::string slot_{};
    float depth_offset_{0.0f};
    std::unordered_map<entt::id_type, entt::id_type> texture_by_animation_id_{};

    [[nodiscard]] entt::id_type resolveTexture(entt::id_type animation_id) const {
        if (const auto it = texture_by_animation_id_.find(animation_id); it != texture_by_animation_id_.end()) {
            return it->second;
        }
        return INVALID_TEXTURE_ID;
    }
};

struct LayeredSpriteComponent {
    static constexpr float LAYER_DEPTH_STEP = 0.0001f;

    bool enabled_{true};
    std::vector<LayeredSpriteLayer> layers_{};

    [[nodiscard]] const LayeredSpriteLayer* findLayer(std::string_view slot) const {
        for (const auto& layer : layers_) {
            if (layer.slot_ == slot) {
                return &layer;
            }
        }
        return nullptr;
    }

    [[nodiscard]] LayeredSpriteLayer* findLayer(std::string_view slot) {
        for (auto& layer : layers_) {
            if (layer.slot_ == slot) {
                return &layer;
            }
        }
        return nullptr;
    }
};

} // namespace engine::component
