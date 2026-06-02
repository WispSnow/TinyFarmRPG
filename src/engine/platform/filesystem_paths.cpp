#include "filesystem_paths.h"

#include <string>

namespace engine::platform {
namespace {

#if defined(__EMSCRIPTEN__)
[[nodiscard]] std::filesystem::path normalizeWebConfigPath(std::string_view path) {
    std::string value{path};
    if (value.rfind("/persistent/", 0) == 0) {
        return std::filesystem::path{value};
    }
    if (value.rfind("/config/", 0) == 0) {
        value.erase(0, 1);
    }
    if (value.rfind("config/", 0) == 0) {
        return std::filesystem::path{WEB_PERSISTENT_ROOT} / value;
    }
    return std::filesystem::path{value};
}
#endif

} // namespace

std::filesystem::path persistentRoot() {
#if defined(__EMSCRIPTEN__)
    return std::filesystem::path{WEB_PERSISTENT_ROOT};
#else
    return {};
#endif
}

std::filesystem::path saveRoot() {
#if defined(__EMSCRIPTEN__)
    return std::filesystem::path{WEB_PERSISTENT_ROOT} / "saves";
#else
    return std::filesystem::path{"saves"};
#endif
}

std::filesystem::path userOverridePathFor(std::string_view readonly_path) {
#if defined(__EMSCRIPTEN__)
    return normalizeWebConfigPath(readonly_path);
#else
    return std::filesystem::path{readonly_path};
#endif
}

std::filesystem::path writableConfigPath(std::string_view config_path) {
#if defined(__EMSCRIPTEN__)
    return normalizeWebConfigPath(config_path);
#else
    return std::filesystem::path{config_path};
#endif
}

bool shouldWriteMissingReadonlyDefaults() {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return true;
#endif
}

} // namespace engine::platform
