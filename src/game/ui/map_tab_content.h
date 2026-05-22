#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "game/ui/map_marker_provider.h"
#include "game/ui/map_preview_builder.h"
#include "game/ui/menu_tab_content.h"
#include "game/ui/quest_map_marker_resolver.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Event;
}

namespace game::data {
class QuestCatalog;
class RpgCatalog;
class ShopCatalog;
}

namespace game::world {
class WorldState;
struct MapInfo;
}

namespace game::ui {

inline constexpr float MAP_TAB_PREVIEW_FRAME_WIDTH = 218.0F;
inline constexpr float MAP_TAB_PREVIEW_FRAME_HEIGHT = 126.0F;
inline constexpr float MAP_TAB_MARKER_SIZE = 16.0F;
inline constexpr float MAP_TAB_MARKER_SELECTED_SIZE = 16.0F;

struct MapTabPreviewInput {
    Rml::String source_uri{};
    int width{0};
    int height{0};
};

struct MapMarkerViewModel {
    int marker_index{0};
    Rml::String kind{};
    Rml::String icon_decorator{"none"};
    Rml::String left{"0dp"};
    Rml::String top{"0dp"};
    Rml::String width{"0dp"};
    Rml::String height{"0dp"};
    int z_index{0};
    glm::vec2 normal_top_left{};
    glm::vec2 selected_top_left{};
    int normal_z_index{0};
    Rml::String title{};
    Rml::String type_label{};
    Rml::String description{};
    bool is_selected{false};
};

struct MapTabViewState {
    Rml::String map_title{"Map"};
    Rml::String map_preview_src{};
    Rml::String map_preview_left{"0dp"};
    Rml::String map_preview_top{"0dp"};
    Rml::String map_preview_width{"0dp"};
    Rml::String map_preview_height{"0dp"};
    Rml::String map_status_text{"No map data"};
    std::vector<MapMarkerViewModel> map_markers{};
    Rml::String map_detail_title{};
    Rml::String map_detail_type{};
    Rml::String map_detail_description{};
    bool has_map_preview{false};
    bool has_map_markers{false};
    bool has_place_markers{false};
    bool has_map_detail{false};
};

[[nodiscard]] bool registerMapTabDataTypes(Rml::DataModelConstructor& constructor);
[[nodiscard]] MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                                   entt::entity player,
                                                   const game::world::WorldState* world_state,
                                                   entt::id_type map_id,
                                                   const MapTabPreviewInput& preview);
[[nodiscard]] MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                                   entt::entity player,
                                                   const game::world::WorldState* world_state,
                                                   entt::id_type map_id,
                                                   const MapTabPreviewInput& preview,
                                                   const std::vector<MapObjectMarker>& static_place_markers,
                                                   const std::vector<QuestRuntimeMarker>& quest_markers,
                                                   const game::data::ShopCatalog* shop_catalog,
                                                   const game::data::RpgCatalog* rpg_catalog,
                                                   int selected_marker_index);
[[nodiscard]] int defaultMapMarkerSelection(bool has_player_marker, std::size_t object_marker_count);

class MapTabContent final : public IMenuTabContent {
public:
    MapTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                  entt::registry& game_registry,
                  entt::entity player,
                  const game::world::WorldState* world_state,
                  const game::data::QuestCatalog* quest_catalog,
                  const game::data::ShopCatalog* shop_catalog,
                  const game::data::RpgCatalog* rpg_catalog,
                  engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images);
    ~MapTabContent() override = default;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;
    void invalidateQuestMarkers();

private:
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    const game::world::WorldState* world_state_{nullptr};
    const game::data::QuestCatalog* quest_catalog_{nullptr};
    const game::data::ShopCatalog* shop_catalog_{nullptr};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images_{nullptr};
    MapPreviewBuilder preview_builder_{};
    MapMarkerProvider marker_provider_{};
    engine::ui::rmlui::RmlGeneratedImageRegistry::Registration preview_registration_{};
    entt::id_type preview_map_id_{entt::null};
    MapTabPreviewInput current_preview_input_{};
    entt::id_type current_marker_map_id_{entt::null};
    int selected_marker_index_{-1};

    Rml::String map_title_{"Map"};
    Rml::String map_preview_src_{};
    Rml::String map_preview_left_{"0dp"};
    Rml::String map_preview_top_{"0dp"};
    Rml::String map_preview_width_{"0dp"};
    Rml::String map_preview_height_{"0dp"};
    Rml::String map_status_text_{"No map data"};
    std::vector<MapMarkerViewModel> map_markers_{};
    Rml::String map_detail_title_{};
    Rml::String map_detail_type_{};
    Rml::String map_detail_description_{};
    bool has_map_preview_{false};
    bool has_map_markers_{false};
    bool has_place_markers_{false};
    bool has_map_detail_{false};

    [[nodiscard]] MapTabPreviewInput buildPreviewInput(entt::id_type map_id);
    void syncViewState();
    void syncDetailFromSelection();
    void markDetailDirty();
    void selectMarker(int marker_index);
    void onMarkerFocus(int marker_index, Rml::Event& event);
    void onMarkerHover(int marker_index, Rml::Event& event);
    void onMarkerClick(int marker_index, Rml::Event& event);
};

} // namespace game::ui
