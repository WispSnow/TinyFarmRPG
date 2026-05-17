#pragma once

#include <RmlUi/Core/Types.h>

#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace game::data {
class AppearanceCatalog;
}

namespace game::scene {
struct AppearanceSelection;
}

namespace game::ui {

struct AppearanceSlotViewModel {
    int slot_index{0};
    Rml::String slot_id{};
    Rml::String label{};
    Rml::String variant_label{};
    Rml::String index_label{};
};

using AppearanceSlotViewModels = std::vector<AppearanceSlotViewModel>;

[[nodiscard]] bool registerAppearanceCustomizeDataTypes(Rml::DataModelConstructor& constructor);
[[nodiscard]] AppearanceSlotViewModels buildAppearanceSlotViewModels(
    const game::data::AppearanceCatalog& catalog,
    const game::scene::AppearanceSelection& selection);
[[nodiscard]] std::string displayLabelForAppearanceSlot(std::string_view slot);
[[nodiscard]] std::string displayLabelForAppearanceVariant(std::string_view variant);

} // namespace game::ui
