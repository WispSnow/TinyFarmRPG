#include "game/ui/rml_localization_applier.h"

#include "engine/ui/rmlui/rml_element_helpers.h"
#include "game/runtime/localization_service.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Types.h>

#include <string>

namespace game::ui {

namespace {

void applyElementText(Rml::Element& element, const game::runtime::LocalizationService& localization) {
    const std::string key = element.GetAttribute<std::string>("data-i18n", "");
    if (key.empty()) {
        return;
    }
    element.SetInnerRML(engine::ui::rmlui::textToInnerRml(localization.tr(key)));
}

void applyTitle(Rml::Element& element, const game::runtime::LocalizationService& localization) {
    const std::string key = element.GetAttribute<std::string>("data-i18n-title", "");
    if (key.empty()) {
        return;
    }
    element.SetAttribute("title", localization.tr(key));
}

} // namespace

void applyRmlLocalization(Rml::ElementDocument& document,
                          const game::runtime::LocalizationService& localization) {
    Rml::ElementList text_elements{};
    document.QuerySelectorAll(text_elements, "[data-i18n]");
    for (Rml::Element* element : text_elements) {
        if (element) {
            applyElementText(*element, localization);
        }
    }

    Rml::ElementList title_elements{};
    document.QuerySelectorAll(title_elements, "[data-i18n-title]");
    for (Rml::Element* element : title_elements) {
        if (element) {
            applyTitle(*element, localization);
        }
    }
}

} // namespace game::ui
