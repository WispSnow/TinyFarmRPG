#pragma once

#include "engine/debug/debug_panel.h"

#include <array>
#include <entt/core/fwd.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace game::factory {
class BlueprintManager;
}

namespace game::debug {

class BlueprintInspectorDebugPanel final : public engine::debug::DebugPanel {
    const game::factory::BlueprintManager& blueprint_manager_;
    std::string resource_mapping_path_;

    bool mapping_loaded_{false};
    std::unordered_set<entt::id_type> mapped_sound_ids_;

    std::vector<std::string> missing_texture_paths_;
    std::vector<entt::id_type> missing_sound_ids_;

    std::array<char, 64> actor_key_{};
    std::array<char, 64> animal_key_{};
    std::array<char, 64> crop_key_{};

public:
    explicit BlueprintInspectorDebugPanel(const game::factory::BlueprintManager& blueprint_manager,
                                          std::string resource_mapping_path = "assets/data/resource_mapping.json");

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void reloadResourceMapping();
    void rescan();
};

} // namespace game::debug

