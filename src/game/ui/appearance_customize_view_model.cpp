#include "game/ui/appearance_customize_view_model.h"

#include "game/data/appearance_catalog.h"
#include "game/scene/appearance_customize_types.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <algorithm>
#include <utility>

namespace game::ui {
namespace {

[[nodiscard]] Rml::String toRmlString(const std::string& value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] std::size_t variantIndex(const std::vector<std::string>& variants,
                                       std::string_view variant) {
    const auto it = std::find(variants.begin(), variants.end(), variant);
    if (it == variants.end()) {
        return 0U;
    }
    return static_cast<std::size_t>(std::distance(variants.begin(), it));
}

} // namespace

bool registerAppearanceCustomizeDataTypes(Rml::DataModelConstructor& constructor) {
    if (auto slot_handle = constructor.RegisterStruct<AppearanceSlotViewModel>()) {
        slot_handle.RegisterMember("slot_index", &AppearanceSlotViewModel::slot_index);
        slot_handle.RegisterMember("slot_id", &AppearanceSlotViewModel::slot_id);
        slot_handle.RegisterMember("label", &AppearanceSlotViewModel::label);
        slot_handle.RegisterMember("variant_label", &AppearanceSlotViewModel::variant_label);
        slot_handle.RegisterMember("index_label", &AppearanceSlotViewModel::index_label);
    } else {
        return false;
    }

    return constructor.RegisterArray<AppearanceSlotViewModels>();
}

AppearanceSlotViewModels buildAppearanceSlotViewModels(const game::data::AppearanceCatalog& catalog,
                                                       const game::scene::AppearanceSelection& selection) {
    AppearanceSlotViewModels view_models;
    const auto slots = game::scene::runtimeAppearanceSlots(catalog);
    view_models.reserve(slots.size());

    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto& slot = slots[index];
        const auto& variants = catalog.variantsForSlot(slot);
        const auto current_it = selection.slot_variants.find(slot);
        const std::string_view current_variant =
            current_it == selection.slot_variants.end() ? std::string_view{} : std::string_view(current_it->second);
        const std::size_t current_index = variantIndex(variants, current_variant);
        const std::string resolved_variant = variants.empty() ? std::string{} : variants[current_index];
        const std::string index_label = variants.empty()
                                            ? std::string{"0/0"}
                                            : std::to_string(current_index + 1U) + "/" + std::to_string(variants.size());

        AppearanceSlotViewModel model{};
        model.slot_index = static_cast<int>(index);
        model.slot_id = toRmlString(slot);
        model.label = toRmlString(displayLabelForAppearanceSlot(slot));
        model.variant_label = toRmlString(displayLabelForAppearanceVariant(resolved_variant));
        model.index_label = toRmlString(index_label);
        view_models.push_back(std::move(model));
    }

    return view_models;
}

std::string displayLabelForAppearanceSlot(std::string_view slot) {
    if (slot == "skin") {
        return "Skin";
    }
    if (slot == "eyes") {
        return "Eyes";
    }
    if (slot == "clothes") {
        return "Clothes";
    }
    if (slot == "hair") {
        return "Hair";
    }
    if (slot == "acc") {
        return "Accessory";
    }
    return std::string(slot);
}

std::string displayLabelForAppearanceVariant(std::string_view variant) {
    std::string label;
    label.reserve(variant.size());
    for (const char ch : variant) {
        label.push_back(ch == '/' ? ' ' : ch);
    }
    return label.empty() ? std::string{"None"} : label;
}

} // namespace game::ui
