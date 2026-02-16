#pragma once

#include "engine/debug/debug_panel.h"
#include <vector>

namespace engine::resource {
    class ResourceManager;
    struct TextureDebugInfo;
    struct FontDebugInfo;
    struct AudioDebugInfo;
    struct AutoTileRuleDebugInfo;
}

namespace engine::debug {

class ResMgrDebugPanel final : public DebugPanel {
    engine::resource::ResourceManager& resource_manager_;
    std::vector<engine::resource::TextureDebugInfo> textures_;
    std::vector<engine::resource::FontDebugInfo> fonts_;
    std::vector<engine::resource::AudioDebugInfo> sounds_;
    std::vector<engine::resource::AudioDebugInfo> music_;
    std::vector<engine::resource::AutoTileRuleDebugInfo> auto_tiles_;
    std::size_t cached_font_atlas_bytes_{0};

public:
    explicit ResMgrDebugPanel(engine::resource::ResourceManager& resource_manager);
    ~ResMgrDebugPanel();

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void refreshCaches();
    void drawTexturesSection();
    void drawFontsSection();
    void drawAudioSection();
    void drawAudioTable(const char* label, const std::vector<engine::resource::AudioDebugInfo>& data);
    void drawAutoTileSection();
};

} // namespace engine::debug
