#pragma once

#include "engine/debug/debug_panel.h"

#include <array>
#include <entt/entity/entity.hpp>

namespace engine::resource {
class ResourceManager;
}

namespace engine::debug {

class UIPresetDebugPanel final : public DebugPanel {
    engine::resource::ResourceManager& resource_manager_;

    entt::id_type selected_button_preset_{entt::null};
    entt::id_type selected_image_preset_{entt::null};
    std::array<char, 128> button_key_{};

public:
    explicit UIPresetDebugPanel(engine::resource::ResourceManager& resource_manager);

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void ensureSelectionsValid();
    void drawButtonsTab();
    void drawImagesTab();
};

} // namespace engine::debug

