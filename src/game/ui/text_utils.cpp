#include "game/ui/text_utils.h"

#include <algorithm>
#include <cctype>

namespace game::ui {

std::string titleCaseLabel(std::string_view value, const char* fallback, const bool strip_path_prefix) {
    if (strip_path_prefix) {
        const std::size_t separator = value.find_last_of(".:/");
        if (separator != std::string_view::npos) {
            value = value.substr(separator + 1U);
        }
    }

    std::string label{value};
    std::replace(label.begin(), label.end(), '_', ' ');
    std::replace(label.begin(), label.end(), '-', ' ');

    bool capitalize = true;
    for (char& ch : label) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) {
            capitalize = true;
            continue;
        }
        if (capitalize && std::isalpha(uch)) {
            ch = static_cast<char>(std::toupper(uch));
        }
        capitalize = false;
    }
    return label.empty() ? fallback : label;
}

std::string humanizeMapName(const std::string_view map_name) {
    return titleCaseLabel(map_name, "Map", false);
}

std::string humanizeId(const std::string_view id, const char* fallback) {
    return titleCaseLabel(id, fallback, true);
}

} // namespace game::ui
