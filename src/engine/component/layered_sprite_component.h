#pragma once

#include <entt/core/fwd.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::component {

struct LayeredAnimationLayout {
    entt::id_type texture_id_{};
    std::size_t direction_block_index_{0};
    std::size_t frames_per_direction_{0};
    float frame_width_{0.0f};
    float frame_height_{0.0f};
    std::vector<std::uint16_t> source_frame_index_by_runtime_frame_{};
    bool use_animation_flip_{false};
};

struct LayeredSpriteLayer {
    static constexpr entt::id_type INVALID_TEXTURE_ID{};

    std::string slot_{};
    float depth_offset_{0.0f};
    std::unordered_map<entt::id_type, LayeredAnimationLayout> layout_by_animation_id_{};

    [[nodiscard]] const LayeredAnimationLayout* resolveLayout(entt::id_type animation_id) const {
        if (const auto it = layout_by_animation_id_.find(animation_id); it != layout_by_animation_id_.end()) {
            return &it->second;
        }
        return nullptr;
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
