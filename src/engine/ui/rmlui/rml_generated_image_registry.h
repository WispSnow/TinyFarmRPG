#pragma once

#include "engine/resource/decoded_image.h"
#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::ui::rmlui {

/// @brief CPU-side image registry for transient RmlUi textures.
class RmlGeneratedImageRegistry final {
public:
    class Registration final {
    public:
        Registration() = default;
        Registration(RmlGeneratedImageRegistry& registry, std::string source_uri);
        ~Registration();

        Registration(const Registration&) = delete;
        Registration& operator=(const Registration&) = delete;

        Registration(Registration&& other) noexcept;
        Registration& operator=(Registration&& other) noexcept;

        void reset();
        [[nodiscard]] const std::string& source() const noexcept { return source_uri_; }
        [[nodiscard]] bool valid() const noexcept { return registry_ != nullptr && !source_uri_.empty(); }

    private:
        RmlGeneratedImageRegistry* registry_{nullptr};
        std::string source_uri_{};
    };

    [[nodiscard]] Registration registerImage(
        std::string source_uri,
        engine::resource::DecodedImage image,
        std::optional<RmlUiTextureFilterMode> texture_filter_override = std::nullopt);
    void unregisterImage(std::string_view source_uri);
    [[nodiscard]] const engine::resource::DecodedImage* find(std::string_view source_uri) const;
    [[nodiscard]] std::optional<RmlUiTextureFilterMode> textureFilterOverrideFor(std::string_view source_uri) const;
    [[nodiscard]] static bool isGeneratedSource(std::string_view source_uri);

private:
    struct ImageEntry {
        engine::resource::DecodedImage image{};
        std::optional<RmlUiTextureFilterMode> texture_filter_override{};
    };

    std::unordered_map<std::string, ImageEntry> images_{};
};

} // namespace engine::ui::rmlui
