#pragma once

#include <string>
#include <string_view>

namespace game::ui {

/// @brief Convert an identifier-like string into title case for compact UI labels.
/// @param value Source text, usually an id, object name, or map name.
/// @param fallback Returned when the normalized label is empty.
/// @param strip_path_prefix When true, drops text before the last `.`, `:`, or `/`.
[[nodiscard]] std::string titleCaseLabel(std::string_view value,
                                         const char* fallback,
                                         bool strip_path_prefix);

/// @brief Humanize a map name without stripping dotted or path-like prefixes.
[[nodiscard]] std::string humanizeMapName(std::string_view map_name);

/// @brief Humanize an id or Tiled object name, stripping common id prefixes first.
[[nodiscard]] std::string humanizeId(std::string_view id, const char* fallback = "Place");

} // namespace game::ui
