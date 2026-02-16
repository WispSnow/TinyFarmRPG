#include "text_renderer_debug_panel.h"

#include <algorithm>
#include <imgui.h>
#include <glm/gtc/constants.hpp>

#include "engine/render/text_renderer.h"

namespace engine::debug {

namespace {

void colorToArray(const engine::utils::FColor& color, float (&out)[4]) {
    out[0] = color.r;
    out[1] = color.g;
    out[2] = color.b;
    out[3] = color.a;
}

engine::utils::FColor arrayToColor(const float (&in)[4]) {
    return {in[0], in[1], in[2], in[3]};
}

} // namespace

TextRendererDebugPanel::TextRendererDebugPanel(engine::render::TextRenderer& text_renderer)
    : text_renderer_(text_renderer) {}

std::string_view TextRendererDebugPanel::name() const {
    return "Text";
}

void TextRendererDebugPanel::draw(bool& is_open) {
    if (!ImGui::Begin("Text Renderer", &is_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("TextStyleTabs")) {
        if (ImGui::BeginTabItem("UI")) {
            const auto key = text_renderer_.getDefaultUIStyleKey();
            ImGui::Text("Default: %.*s", static_cast<int>(key.size()), key.data());
            ImGui::Separator();

            const auto keys = text_renderer_.listTextStyleKeys("ui/");
            for (const auto& style_key : keys) {
                auto params = text_renderer_.getTextStyle(style_key);
                bool changed = drawTextParamsSection(style_key.c_str(), params);
                if (changed) {
                    text_renderer_.setTextStyle(style_key, params);
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("World")) {
            const auto key = text_renderer_.getDefaultWorldStyleKey();
            ImGui::Text("Default: %.*s", static_cast<int>(key.size()), key.data());
            ImGui::Separator();

            const auto keys = text_renderer_.listTextStyleKeys("world/");
            for (const auto& style_key : keys) {
                auto params = text_renderer_.getTextStyle(style_key);
                bool changed = drawTextParamsSection(style_key.c_str(), params);
                if (changed) {
                    text_renderer_.setTextStyle(style_key, params);
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

bool TextRendererDebugPanel::drawTextParamsSection(const char* label, engine::utils::TextRenderParams& params) {
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::TextUnformatted("颜色");
        changed |= drawColorOptions(params.color);
        ImGui::Separator();

        ImGui::TextUnformatted("阴影");
        changed |= drawShadowOptions(params.shadow);
        ImGui::Separator();

        ImGui::TextUnformatted("布局");
        changed |= drawLayoutOptions(params.layout);

        if (ImGui::Button("重置该组参数")) {
            params = engine::utils::TextRenderParams{};
            changed = true;
        }
        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

bool TextRendererDebugPanel::drawColorOptions(engine::utils::ColorOptions& options) {
    bool changed = false;
    float start_color[4]{};
    colorToArray(options.start_color, start_color);
    if (ImGui::ColorEdit4("起始颜色", start_color, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float)) {
        options.start_color = arrayToColor(start_color);
        changed = true;
    }

    if (ImGui::Checkbox("启用渐变", &options.use_gradient)) {
        changed = true;
    }

    if (options.use_gradient) {
        float end_color[4]{};
        colorToArray(options.end_color, end_color);
        if (ImGui::ColorEdit4("结束颜色", end_color, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float)) {
            options.end_color = arrayToColor(end_color);
            changed = true;
        }

        float angle_deg = options.angle_radians * 180.0f / glm::pi<float>();
        if (ImGui::DragFloat("渐变角度(度)", &angle_deg, 1.0f, -360.0f, 360.0f)) {
            options.angle_radians = angle_deg * glm::pi<float>() / 180.0f;
            changed = true;
        }
    }

    return changed;
}

bool TextRendererDebugPanel::drawShadowOptions(engine::utils::ShadowOptions& options) {
    bool changed = false;
    if (ImGui::Checkbox("启用阴影", &options.enabled)) {
        changed = true;
    }

    if (!options.enabled) {
        return changed;
    }

    float offset[2] = {options.offset.x, options.offset.y};
    if (ImGui::DragFloat2("偏移", offset, 0.1f, -100.0f, 100.0f)) {
        options.offset = {offset[0], offset[1]};
        changed = true;
    }

    float shadow_color[4]{};
    colorToArray(options.color, shadow_color);
    if (ImGui::ColorEdit4("阴影颜色", shadow_color, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float)) {
        options.color = arrayToColor(shadow_color);
        changed = true;
    }

    return changed;
}

bool TextRendererDebugPanel::drawLayoutOptions(engine::utils::LayoutOptions& options) {
    bool changed = false;

    if (ImGui::DragFloat("字距 (px)", &options.letter_spacing, 0.1f, -100.0f, 100.0f, "%.2f")) {
        options.letter_spacing = std::clamp(options.letter_spacing, -100.0f, 100.0f);
        changed = true;
    }

    if (ImGui::DragFloat("行距缩放", &options.line_spacing_scale, 0.01f, 0.1f, 5.0f, "%.2f")) {
        options.line_spacing_scale = std::clamp(options.line_spacing_scale, 0.1f, 5.0f);
        changed = true;
    }

    float scale[2] = {options.glyph_scale.x, options.glyph_scale.y};
    if (ImGui::DragFloat2("字形缩放", scale, 0.01f, 0.1f, 5.0f, "%.2f")) {
        options.glyph_scale.x = std::clamp(scale[0], 0.1f, 5.0f);
        options.glyph_scale.y = std::clamp(scale[1], 0.1f, 5.0f);
        changed = true;
    }

    if (ImGui::Button("重置布局")) {
        options = engine::utils::LayoutOptions{};
        changed = true;
    }

    return changed;
}

} // namespace engine::debug
