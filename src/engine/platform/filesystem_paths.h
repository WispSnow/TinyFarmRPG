#pragma once

#include <filesystem>
#include <string_view>

namespace engine::platform {

inline constexpr std::string_view WEB_PERSISTENT_ROOT{"/persistent"};

[[nodiscard]] std::filesystem::path persistentRoot();
[[nodiscard]] std::filesystem::path saveRoot();
[[nodiscard]] std::filesystem::path userOverridePathFor(std::string_view readonly_path);
[[nodiscard]] std::filesystem::path writableConfigPath(std::string_view config_path);
[[nodiscard]] bool shouldWriteMissingReadonlyDefaults();

} // namespace engine::platform
