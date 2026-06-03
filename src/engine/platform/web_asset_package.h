#pragma once

#include <string_view>

namespace engine::platform::web {

[[nodiscard]] bool loadAssetPackage(std::string_view package_id, std::string_view package_url);
[[nodiscard]] bool isAssetPackageLoaded(std::string_view package_id);

} // namespace engine::platform::web
