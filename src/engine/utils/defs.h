#pragma once
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include "math.h"

namespace engine::utils {

struct GL_Texture {
    GLuint texture;
    int width;
    int height;

    GL_Texture(GLuint texture, int width, int height) : texture(texture), width(width), height(height) {}
    GL_Texture() : texture(0), width(0), height(0) {}
};

struct ColorOptions {
    FColor start_color{FColor::white()};
    FColor end_color{FColor::white()};
    bool use_gradient{false};
    float angle_radians{0.0f};
};

struct TransformOptions {
    float rotation_radians{0.0f};
    glm::vec2 pivot{0.5f, 0.5f};
    bool flip_horizontal{false};
};

struct ShadowOptions {
    bool enabled{true};
    glm::vec2 offset{1.0f, 1.0f};
    FColor color{FColor::grey()};
};

struct LayoutOptions {
    float letter_spacing{0.0f};              ///< @brief 额外字距，单位像素。
    float line_spacing_scale{1.0f};          ///< @brief 行距缩放比例，基于字体默认行高。
    glm::vec2 glyph_scale{1.0f, 1.0f};       ///< @brief 字形宽高缩放比例。

    [[nodiscard]] bool operator==(const LayoutOptions& other) const noexcept = default;
};

struct TextRenderParams {
    ColorOptions color{};
    ShadowOptions shadow{};
    LayoutOptions layout{};
};

struct TextRenderOverrides {
    std::optional<FColor> color{};
    std::optional<bool> shadow_enabled{};
    std::optional<glm::vec2> shadow_offset{};
    std::optional<FColor> shadow_color{};
    std::optional<glm::vec2> glyph_scale{};

    [[nodiscard]] bool isEmpty() const noexcept {
        return !color && !shadow_enabled && !shadow_offset && !shadow_color && !glyph_scale;
    }
};

struct PointLightOptions {
    float intensity{1.0f};
    FColor color{FColor::white()};
};

struct SpotLightOptions {
    float inner_angle_deg{25.0f};
    float outer_angle_deg{45.0f};
    float intensity{1.0f};
    FColor color{FColor::white()};
};

struct DirectionalLightOptions {
    float intensity{1.0f};
    FColor color{FColor::white()};
    float offset{0.5f};
    float softness{0.25f};
    float midday_blend{0.0f};
    float zoom{1.0f};
};

struct EmissiveParams {
    float intensity{1.0f};
    ColorOptions color{};
    TransformOptions transform{};
};

}   // namespace engine::utils
