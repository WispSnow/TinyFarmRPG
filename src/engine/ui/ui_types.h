#pragma once

#include "engine/resource/default_resource_ids.h"

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <string_view>

namespace engine::ui {

struct Thickness {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    [[nodiscard]] glm::vec2 horizontal() const { return {left, right}; }
    [[nodiscard]] glm::vec2 vertical() const { return {top, bottom}; }
    [[nodiscard]] float width() const { return left + right; }
    [[nodiscard]] float height() const { return top + bottom; }
};

struct SlotItem {
    entt::id_type item_id{entt::null};
    int count{0};
};

inline constexpr std::string_view DEFAULT_UI_FONT_PATH{engine::resource::defaults::UI_DEFAULT_FONT_PATH};
inline constexpr int DEFAULT_UI_FONT_SIZE_PX{engine::resource::defaults::UI_DEFAULT_FONT_SIZE_PX};
inline constexpr entt::id_type DEFAULT_UI_FONT_ID{engine::resource::defaults::UI_DEFAULT_FONT_ID};

[[nodiscard]] inline entt::id_type resolveUIFontId(entt::id_type font_id) {
    if (font_id != entt::null && font_id != entt::id_type{}) {
        return font_id;
    }
    return DEFAULT_UI_FONT_ID;
}

} // namespace engine::ui
