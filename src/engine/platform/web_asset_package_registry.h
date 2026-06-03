#pragma once

#include <string>
#include <string_view>

namespace engine::platform::web {

inline constexpr std::string_view PACKAGE_SHARED_UI{"shared-ui"};
inline constexpr std::string_view PACKAGE_HOME_MAP{"home-map"};
inline constexpr std::string_view PACKAGE_AUDIO_CORE{"audio-core"};

[[nodiscard]] bool loadPackage(std::string_view package_id);
[[nodiscard]] bool isPackageLoaded(std::string_view package_id);
[[nodiscard]] std::string lastPackageError(std::string_view package_id);
[[nodiscard]] std::string_view packageUrl(std::string_view package_id);

} // namespace engine::platform::web
