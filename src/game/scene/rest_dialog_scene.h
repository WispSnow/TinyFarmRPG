#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/domain/party_rest_service.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>

#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace engine::core {
enum class State;
}

namespace game::scene {

class RestDialogScene final : public engine::scene::Scene {
private:
    struct RestRecoveryMemberViewModel {
        Rml::String display_name{};
        Rml::String hp_text{};
        Rml::String mp_text{};
        Rml::String hp_delta_text{};
        Rml::String mp_delta_text{};
        bool has_hp_gain{false};
        bool has_mp_gain{false};
    };

    engine::core::State previous_state_{};
    int selected_hours_{24};
    entt::entity player_{entt::null};
    bool context_pushed_{false};
    bool data_types_registered_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    Rml::String hours_text_{"24h"};
    Rml::String recovery_summary_text_{"Full recovery"};
    Rml::String recovery_empty_text_{"No active party members."};
    bool has_recovery_members_{false};
    std::vector<RestRecoveryMemberViewModel> recovery_members_{};
    std::vector<game::domain::RestRecoveryPreview> recovery_previews_{};

public:
    static constexpr int MIN_REST_HOURS = 1;
    static constexpr int MAX_REST_HOURS = 24;

    RestDialogScene(std::string_view name,
                    engine::core::Context& context,
                    entt::entity player,
                    std::vector<game::domain::RestRecoveryPreview> recovery_previews);
    ~RestDialogScene() override;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void shutdownUI();
    void disconnectRuntimeListeners();
    void updateHoursLabel();
    void refreshRecoveryPreview();
    void adjustHours(int delta);
    [[nodiscard]] const game::domain::RestRecoveryPreview* currentRecoveryPreview() const;
    bool onMenuCancelPressed();
    void onConfirm();
    void onCancel();
};

} // namespace game::scene
