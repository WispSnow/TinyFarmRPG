#include "game/ui/appearance_customize_view_model.h"

#include "game/data/appearance_catalog.h"
#include "game/scene/appearance_customize_types.h"
#include "game/ui/localized_text.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <algorithm>
#include <cctype>
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

[[nodiscard]] std::string fallbackSlotLabel(std::string_view slot) {
    if (slot == "skin") {
        return "Skin";
    }
    if (slot == "eyes") {
        return "Eyes";
    }
    if (slot == "gender") {
        return "Gender";
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

[[nodiscard]] std::string slotLabelKey(std::string_view slot) {
    return "appearance.slot." + std::string(slot);
}

[[nodiscard]] bool isDigits(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const char ch) {
        return std::isdigit(static_cast<unsigned char>(ch)) != 0;
    });
}

[[nodiscard]] std::string variantPartKey(std::string_view part) {
    std::string key{"appearance.variant_part."};
    bool separator_pending = false;
    for (const char ch : part) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) != 0) {
            key.push_back(static_cast<char>(std::tolower(c)));
            separator_pending = false;
        } else if (!separator_pending) {
            key.push_back('_');
            separator_pending = true;
        }
    }
    if (key.back() == '_') {
        key.pop_back();
    }
    return key;
}

[[nodiscard]] std::string localizeVariantPart(const game::runtime::LocalizationService* localization,
                                              std::string_view part) {
    if (isDigits(part)) {
        return std::string(part);
    }
    return localizeTextOrFallback(localization, variantPartKey(part), part);
}

[[nodiscard]] std::string joinVariantParts(const game::runtime::LocalizationService* localization,
                                           const std::string& left,
                                           const std::string& right) {
    return formatTextOrFallback(
        localization,
        "appearance.variant.join",
        {{"left", left}, {"right", right}},
        [&left, &right] { return left + " " + right; });
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
                                                       const game::scene::AppearanceSelection& selection,
                                                       const game::runtime::LocalizationService* localization,
                                                       const bool include_gender) {
    AppearanceSlotViewModels view_models;
    const auto slots = game::scene::appearanceControlSlots(catalog, include_gender);
    view_models.reserve(slots.size());

    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto& slot = slots[index];
        const auto& variants = slot == "gender" ? catalog.genderVariants() : catalog.variantsForSlot(slot);
        const auto current_it = selection.slot_variants.find(slot);
        const std::string_view current_variant = slot == "gender"
                                                     ? std::string_view(selection.gender)
                                                     : (current_it == selection.slot_variants.end()
                                                            ? std::string_view{}
                                                            : std::string_view(current_it->second));
        const std::size_t current_index = variantIndex(variants, current_variant);
        const std::string resolved_variant = variants.empty() ? std::string{} : variants[current_index];
        const std::string index_label = variants.empty()
                                            ? std::string{"0/0"}
                                            : std::to_string(current_index + 1U) + "/" + std::to_string(variants.size());

        AppearanceSlotViewModel model{};
        model.slot_index = static_cast<int>(index);
        model.slot_id = toRmlString(slot);
        model.label = toRmlString(displayLabelForAppearanceSlot(slot, localization));
        model.variant_label = toRmlString(displayLabelForAppearanceVariant(resolved_variant, localization));
        model.index_label = toRmlString(index_label);
        view_models.push_back(std::move(model));
    }

    return view_models;
}

std::string displayLabelForAppearanceSlot(std::string_view slot,
                                          const game::runtime::LocalizationService* localization) {
    return localizeTextOrFallback(localization, slotLabelKey(slot), fallbackSlotLabel(slot));
}

std::string displayLabelForAppearanceVariant(std::string_view variant,
                                             const game::runtime::LocalizationService* localization) {
    if (variant.empty() || variant == "none") {
        return localizeTextOrFallback(localization, "appearance.variant.none", "none");
    }

    std::string label{};
    std::size_t start = 0U;
    while (start <= variant.size()) {
        const std::size_t end = variant.find('/', start);
        const std::string_view part = end == std::string_view::npos
                                          ? variant.substr(start)
                                          : variant.substr(start, end - start);
        const std::string localized_part = localizeVariantPart(localization, part);
        label = label.empty() ? localized_part : joinVariantParts(localization, label, localized_part);
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
    return label;
}

} // namespace game::ui
