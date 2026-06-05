#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace engine::platform::web {

inline constexpr std::string_view PACKAGE_SHARED_UI{"shared-ui"};
inline constexpr std::string_view PACKAGE_RPG_CORE{"rpg-core"};
inline constexpr std::string_view PACKAGE_HOME_MAP{"home-map"};
inline constexpr std::string_view PACKAGE_TOWN_MAP{"town-map"};
inline constexpr std::string_view PACKAGE_SCHOOL_MAP{"school-map"};
inline constexpr std::string_view PACKAGE_BATTLE_CORE{"battle-core"};
inline constexpr std::string_view PACKAGE_VFX_CORE{"vfx-core"};
inline constexpr std::string_view PACKAGE_AUDIO_CORE{"audio-core"};

[[nodiscard]] bool loadPackage(std::string_view package_id);
[[nodiscard]] bool loadGroup(std::initializer_list<std::string_view> package_ids);
[[nodiscard]] bool isPackageLoaded(std::string_view package_id);
[[nodiscard]] std::string lastPackageError(std::string_view package_id);
[[nodiscard]] std::string_view packageUrl(std::string_view package_id);
[[nodiscard]] int packageFiles(std::string_view package_id);
[[nodiscard]] std::uint64_t packageBytes(std::string_view package_id);

} // namespace engine::platform::web
