#include "ui_preset_debug_panel.h"

#include <imgui.h>

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "engine/render/image.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_preset_manager.h"

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

bool editColor(const char* label, engine::utils::FColor& color) {
    float value[4]{};
    colorToArray(color, value);
    if (!ImGui::ColorEdit4(label, value, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float)) {
        return false;
    }
    color = arrayToColor(value);
    return true;
}

[[nodiscard]] std::string formatPresetLabel(std::string_view key, entt::id_type id) {
    if (key.empty()) {
        return std::format("0x{:08X}", static_cast<unsigned int>(id));
    }
    return std::format("{} (0x{:08X})##{:08X}",
                       key,
                       static_cast<unsigned int>(id),
                       static_cast<unsigned int>(id));
}

bool editOffset(const char* label, glm::vec2& offset) {
    float value[2]{offset.x, offset.y};
    if (!ImGui::DragFloat2(label, value, 0.25f, -200.0f, 200.0f, "%.2f")) {
        return false;
    }
    offset = {value[0], value[1]};
    return true;
}

void drawImageInfo(const char* label, const std::optional<engine::render::Image>& image) {
    ImGui::PushID(label);
    if (!image) {
        ImGui::Text("%s: <none>", label);
        ImGui::PopID();
        return;
    }

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& img = *image;
        const auto& rect = img.getSourceRect();

        ImGui::Text("Texture ID: 0x%08X", static_cast<unsigned int>(img.getTextureId()));
        const auto texture_path = img.getTexturePath();
        ImGui::Text("Texture Path: %.*s", static_cast<int>(texture_path.size()), texture_path.data());
        ImGui::Text("Source Rect: pos(%.1f, %.1f) size(%.1f, %.1f)", rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
        ImGui::Text("Flipped: %s", img.isFlipped() ? "true" : "false");

        const auto& margins = img.getNineSliceMargins();
        if (margins) {
            ImGui::Text("NineSlice: L %.1f T %.1f R %.1f B %.1f", margins->left, margins->top, margins->right, margins->bottom);
        } else {
            ImGui::TextUnformatted("NineSlice: <none>");
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

engine::utils::FColor fallbackColor(const engine::ui::UIButtonSkin& skin) {
    if (skin.normal_label) return skin.normal_label->color;
    return engine::utils::FColor{1.0f, 1.0f, 1.0f, 1.0f};
}

glm::vec2 fallbackOffset(const engine::ui::UIButtonSkin& skin) {
    if (skin.normal_label) return skin.normal_label->offset;
    return {0.0f, 0.0f};
}

bool drawLabelOverridesEditor(const char* label,
                              engine::ui::UIButtonSkin& skin,
                              std::optional<engine::ui::UIButtonLabelOverrides>& overrides) {
    bool changed = false;

    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    bool enabled = overrides.has_value();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        if (enabled) {
            overrides = engine::ui::UIButtonLabelOverrides{};
        } else {
            overrides.reset();
        }
        changed = true;
    }

    if (!overrides) {
        ImGui::PopID();
        return changed;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        overrides.reset();
        ImGui::PopID();
        return true;
    }

    ImGui::Indent();
    auto& value = *overrides;

    bool color_enabled = value.color.has_value();
    if (ImGui::Checkbox("Color", &color_enabled)) {
        if (color_enabled) {
            value.color = fallbackColor(skin);
        } else {
            value.color.reset();
        }
        changed = true;
    }
    if (value.color) {
        engine::utils::FColor c = *value.color;
        if (editColor("Value##Color", c)) {
            value.color = c;
            changed = true;
        }
    }

    bool offset_enabled = value.offset.has_value();
    if (ImGui::Checkbox("Offset", &offset_enabled)) {
        if (offset_enabled) {
            value.offset = fallbackOffset(skin);
        } else {
            value.offset.reset();
        }
        changed = true;
    }
    if (value.offset) {
        glm::vec2 o = *value.offset;
        if (editOffset("Value##Offset", o)) {
            value.offset = o;
            changed = true;
        }
    }

    ImGui::Unindent();
    ImGui::PopID();
    return changed;
}

} // namespace

UIPresetDebugPanel::UIPresetDebugPanel(engine::resource::ResourceManager& resource_manager)
    : resource_manager_(resource_manager) {
    button_key_.fill('\0');
}

std::string_view UIPresetDebugPanel::name() const {
    return "UI Presets";
}

void UIPresetDebugPanel::onShow() {
    ensureSelectionsValid();
}

void UIPresetDebugPanel::draw(bool& is_open) {
    if (!ImGui::Begin("UI Presets", &is_open)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Edits apply to in-memory UIPresetManager data only (no JSON read/write).");

    if (ImGui::BeginTabBar("UIPresetTabs")) {
        if (ImGui::BeginTabItem("Buttons")) {
            drawButtonsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Images")) {
            drawImagesTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void UIPresetDebugPanel::ensureSelectionsValid() {
    auto& presets = resource_manager_.getUIPresetManager();

    const auto button_ids = presets.listButtonPresetIds();
    if (button_ids.empty()) {
        selected_button_preset_ = entt::null;
    } else if (selected_button_preset_ == entt::null ||
               std::ranges::find(button_ids, selected_button_preset_) == button_ids.end()) {
        selected_button_preset_ = button_ids.front();
    }

    const auto image_ids = presets.listImagePresetIds();
    if (image_ids.empty()) {
        selected_image_preset_ = entt::null;
    } else if (selected_image_preset_ == entt::null ||
               std::ranges::find(image_ids, selected_image_preset_) == image_ids.end()) {
        selected_image_preset_ = image_ids.front();
    }
}

void UIPresetDebugPanel::drawButtonsTab() {
    auto& presets = resource_manager_.getUIPresetManager();
    const auto button_ids = presets.listButtonPresetIds();

    ImGui::Text("Button Presets: %zu", button_ids.size());
    if (button_ids.empty()) {
        ImGui::TextUnformatted("No button presets loaded.");
        return;
    }

    ensureSelectionsValid();

    ImGui::Separator();

    ImGui::InputTextWithHint("Key", "e.g. secondary", button_key_.data(), button_key_.size());
    ImGui::SameLine();
    if (ImGui::Button("Select")) {
        if (button_key_[0] != '\0') {
            selected_button_preset_ = entt::hashed_string{button_key_.data()}.value();
        }
    }
    const bool has_selected = (selected_button_preset_ != entt::null) && (presets.getButtonPreset(selected_button_preset_) != nullptr);
    ImGui::SameLine();
    const std::string_view selected_key = presets.getButtonPresetKey(selected_button_preset_);
    if (selected_key.empty()) {
        ImGui::Text("Selected: 0x%08X %s",
                    static_cast<unsigned int>(selected_button_preset_),
                    has_selected ? "(found)" : "(missing)");
    } else {
        ImGui::Text("Selected: %.*s (0x%08X) %s",
                    static_cast<int>(selected_key.size()),
                    selected_key.data(),
                    static_cast<unsigned int>(selected_button_preset_),
                    has_selected ? "(found)" : "(missing)");
    }

    if (ImGui::BeginTable("UIPresetButtonsLayout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Presets", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::BeginChild("ButtonPresetList", ImVec2(0.0f, 0.0f), true);
        for (const auto id : button_ids) {
            const auto label = formatPresetLabel(presets.getButtonPresetKey(id), id);
            const bool selected = (id == selected_button_preset_);
            if (ImGui::Selectable(label.c_str(), selected)) {
                selected_button_preset_ = id;
            }
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        auto* skin = presets.getButtonPresetMutable(selected_button_preset_);
        if (!skin) {
            ImGui::TextUnformatted("Select a button preset to inspect.");
            ImGui::EndTable();
            return;
        }

        const std::string_view preset_key = presets.getButtonPresetKey(selected_button_preset_);
        if (!preset_key.empty()) {
            ImGui::Text("Preset Key: %.*s", static_cast<int>(preset_key.size()), preset_key.data());
        }
        ImGui::Text("Preset ID: 0x%08X", static_cast<unsigned int>(selected_button_preset_));
        if (skin->nine_slice_margins) {
            const auto& m = *skin->nine_slice_margins;
            ImGui::Text("NineSlice (skin): L %.1f T %.1f R %.1f B %.1f", m.left, m.top, m.right, m.bottom);
        } else {
            ImGui::TextUnformatted("NineSlice (skin): <none>");
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Images");
        drawImageInfo("Normal", skin->normal_image);
        drawImageInfo("Hover", skin->hover_image);
        drawImageInfo("Pressed", skin->pressed_image);
        drawImageInfo("Disabled", skin->disabled_image);

        ImGui::Separator();
        ImGui::TextUnformatted("Label");
        if (!skin->normal_label) {
            ImGui::TextUnformatted("Normal label style: <none>");
            ImGui::EndTable();
            return;
        }

        auto& normal_label = *skin->normal_label;
        ImGui::Text("Text: %s", normal_label.text.c_str());
        ImGui::Text("Font: %s (%d)", normal_label.font_path.c_str(), normal_label.font_size);
        bool changed = false;
        changed |= editColor("Normal Color", normal_label.color);
    changed |= editOffset("Normal Offset", normal_label.offset);

    ImGui::Separator();
    ImGui::TextUnformatted("Overrides");
    changed |= drawLabelOverridesEditor("hover", *skin, skin->hover_label);
    changed |= drawLabelOverridesEditor("pressed", *skin, skin->pressed_label);
    changed |= drawLabelOverridesEditor("disabled", *skin, skin->disabled_label);

    if (changed) {
        // Live edits: UIButton renders from UIPresetManager directly.
    }

        ImGui::EndTable();
    }
}

void UIPresetDebugPanel::drawImagesTab() {
    auto& presets = resource_manager_.getUIPresetManager();
    const auto image_ids = presets.listImagePresetIds();

    ImGui::Text("Image Presets: %zu", image_ids.size());
    if (image_ids.empty()) {
        ImGui::TextUnformatted("No image presets loaded.");
        return;
    }

    ensureSelectionsValid();

    if (ImGui::BeginTable("UIPresetImagesLayout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Presets", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::BeginChild("ImagePresetList", ImVec2(0.0f, 0.0f), true);
        for (const auto id : image_ids) {
            const auto label = formatPresetLabel(presets.getImagePresetKey(id), id);
            const bool selected = (id == selected_image_preset_);
            if (ImGui::Selectable(label.c_str(), selected)) {
                selected_image_preset_ = id;
            }
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        const auto* image = presets.getImagePreset(selected_image_preset_);
        if (!image) {
            ImGui::TextUnformatted("Select an image preset to inspect.");
            ImGui::EndTable();
            return;
        }

        const std::string_view preset_key = presets.getImagePresetKey(selected_image_preset_);
        if (!preset_key.empty()) {
            ImGui::Text("Preset Key: %.*s", static_cast<int>(preset_key.size()), preset_key.data());
        }
        ImGui::Text("Preset ID: 0x%08X", static_cast<unsigned int>(selected_image_preset_));
        const auto& rect = image->getSourceRect();
        ImGui::Text("Texture ID: 0x%08X", static_cast<unsigned int>(image->getTextureId()));
        const auto texture_path = image->getTexturePath();
        ImGui::Text("Texture Path: %.*s", static_cast<int>(texture_path.size()), texture_path.data());
        ImGui::Text("Source Rect: pos(%.1f, %.1f) size(%.1f, %.1f)", rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
        ImGui::Text("Flipped: %s", image->isFlipped() ? "true" : "false");

        const auto& margins = image->getNineSliceMargins();
        if (margins) {
            ImGui::Text("NineSlice: L %.1f T %.1f R %.1f B %.1f", margins->left, margins->top, margins->right, margins->bottom);
        } else {
            ImGui::TextUnformatted("NineSlice: <none>");
        }

        ImGui::EndTable();
    }
}

} // namespace engine::debug
