#pragma once

#include "engine/scene/scene.h"
#include "engine/render/image.h"
#include "engine/system/animation_system.h"
#include "engine/system/render_system.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/scene/appearance_customize_types.h"
#include "game/ui/appearance_customize_view_model.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <functional>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class DataModelHandle;
class Event;
}

namespace engine::core {
enum class State;
}

namespace game::data {
class AppearanceCatalog;
}

namespace game::scene {

class AppearanceCustomizeScene final : public engine::scene::Scene {
public:
    /// Builds the new-game customization scene. Confirm returns the selected look to the caller.
    using SceneFactory = std::function<std::unique_ptr<engine::scene::Scene>(AppearanceSelection selection)>;

    enum class Mode {
        NewGame,
        Closet,
    };

private:
    Mode mode_{Mode::NewGame};
    SceneFactory on_new_game_confirm_{};
    entt::registry* game_registry_{nullptr};
    entt::entity player_{entt::null};
    std::shared_ptr<game::data::AppearanceCatalog> catalog_{};

    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    AppearanceSelection draft_selection_{};
    AppearanceSelection original_selection_{};
    std::vector<std::string> runtime_slots_{};
    game::ui::AppearanceSlotViewModels slot_view_models_{};
    Rml::String title_text_{};
    Rml::String subtitle_text_{};

    entt::registry preview_registry_{};
    entt::dispatcher preview_dispatcher_{};
    engine::system::AnimationSystem preview_animation_system_{preview_registry_, preview_dispatcher_};
    engine::system::RenderSystem preview_render_system_{};
    entt::entity preview_entity_{entt::null};
    engine::render::Image menu_panel_image_{};
    bool menu_panel_texture_loaded_{false};
    bool lighting_suspended_{false};
    bool previous_lighting_enabled_{true};
    std::mt19937 rng_{};

public:
    /// Creates the pre-new-game appearance picker; cancel pops back to the title scene.
    AppearanceCustomizeScene(std::string_view name,
                             engine::core::Context& context,
                             SceneFactory on_confirm);

    /// Creates the in-game closet editor for the current player entity.
    AppearanceCustomizeScene(std::string_view name,
                             engine::core::Context& context,
                             entt::registry& game_registry,
                             entt::entity player,
                             std::shared_ptr<game::data::AppearanceCatalog> catalog);
    ~AppearanceCustomizeScene() override;

    bool init() override;
    void update(float delta_time) override;
    void render(float interpolation_alpha) override;
    void clean() override;
    [[nodiscard]] engine::scene::SceneUiCoverage uiCoverage() const override;

private:
    [[nodiscard]] bool ensureCatalog();
    [[nodiscard]] bool initUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    [[nodiscard]] bool initMenuPanelImage();
    [[nodiscard]] bool initPreviewEntity();
    void releaseMenuPanelImage();
    void suspendClosetLighting();
    void restoreClosetLighting();
    void shutdownUI();
    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    void syncSlotViewModels(bool mark_dirty = true);
    void rebuildPreviewCache();
    void updatePreviewPosition();
    void focusDefaultAction();

    void onSlotStep(int slot_index, int direction);
    void onRandomize();
    void onReset();
    void onConfirm();
    void onCancel();
    bool onMenuCancelPressed();
    void onSlotPrevEvent(Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& arguments);
    void onSlotNextEvent(Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& arguments);
};

} // namespace game::scene
