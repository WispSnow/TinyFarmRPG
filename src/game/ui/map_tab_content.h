#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "game/ui/map_preview_builder.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>

namespace Rml {
class DataModelConstructor;
}

namespace game::world {
class WorldState;
struct MapInfo;
}

namespace game::ui {

inline constexpr float MAP_TAB_PREVIEW_FRAME_WIDTH = 218.0F;
inline constexpr float MAP_TAB_PREVIEW_FRAME_HEIGHT = 126.0F;
inline constexpr float MAP_TAB_PLAYER_MARKER_SIZE = 8.0F;

struct MapTabPreviewInput {
    Rml::String source_uri{};
    int width{0};
    int height{0};
};

struct MapTabViewState {
    Rml::String map_title{"Map"};
    Rml::String map_preview_src{};
    Rml::String map_preview_left{"0dp"};
    Rml::String map_preview_top{"0dp"};
    Rml::String map_preview_width{"0dp"};
    Rml::String map_preview_height{"0dp"};
    Rml::String player_marker_left{"0dp"};
    Rml::String player_marker_top{"0dp"};
    Rml::String map_status_text{"No map data"};
    bool has_map_preview{false};
    bool has_player_marker{false};
};

[[nodiscard]] bool registerMapTabDataTypes(Rml::DataModelConstructor& constructor);
[[nodiscard]] MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                                   entt::entity player,
                                                   const game::world::WorldState* world_state,
                                                   entt::id_type map_id,
                                                   const MapTabPreviewInput& preview);

class MapTabContent final : public IMenuTabContent {
public:
    MapTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                  entt::registry& game_registry,
                  entt::entity player,
                  const game::world::WorldState* world_state,
                  engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images);
    ~MapTabContent() override = default;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;

private:
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    const game::world::WorldState* world_state_{nullptr};
    engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images_{nullptr};
    MapPreviewBuilder preview_builder_{};
    engine::ui::rmlui::RmlGeneratedImageRegistry::Registration preview_registration_{};
    entt::id_type preview_map_id_{entt::null};
    MapTabPreviewInput current_preview_input_{};

    Rml::String map_title_{"Map"};
    Rml::String map_preview_src_{};
    Rml::String map_preview_left_{"0dp"};
    Rml::String map_preview_top_{"0dp"};
    Rml::String map_preview_width_{"0dp"};
    Rml::String map_preview_height_{"0dp"};
    Rml::String player_marker_left_{"0dp"};
    Rml::String player_marker_top_{"0dp"};
    Rml::String map_status_text_{"No map data"};
    bool has_map_preview_{false};
    bool has_player_marker_{false};

    [[nodiscard]] MapTabPreviewInput buildPreviewInput(entt::id_type map_id);
    void syncViewState();
};

} // namespace game::ui
