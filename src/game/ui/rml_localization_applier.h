#pragma once

namespace Rml {
class ElementDocument;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::ui {

void applyRmlLocalization(Rml::ElementDocument& document,
                          const game::runtime::LocalizationService& localization);

} // namespace game::ui
