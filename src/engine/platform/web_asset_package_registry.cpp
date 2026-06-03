#include "web_asset_package_registry.h"

#include "web_asset_package.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>

namespace engine::platform::web {
namespace {

struct PackageDefinition {
    std::string_view id;
    std::string_view url;
};

struct PackageRuntimeStatus {
    bool loaded{false};
    int attempts{0};
    int last_load_ms{0};
    std::string last_error{};
};

constexpr std::array<PackageDefinition, 3> PACKAGE_DEFINITIONS{{
    {PACKAGE_SHARED_UI, "web-packages/shared-ui.tfpack"},
    {PACKAGE_HOME_MAP, "web-packages/home-map.tfpack"},
    {PACKAGE_AUDIO_CORE, "web-packages/audio-core.tfpack"},
}};

[[nodiscard]] const PackageDefinition* findPackage(std::string_view package_id) {
    for (const auto& definition : PACKAGE_DEFINITIONS) {
        if (definition.id == package_id) {
            return &definition;
        }
    }
    return nullptr;
}

std::unordered_map<std::string, PackageRuntimeStatus>& packageStatuses() {
    static std::unordered_map<std::string, PackageRuntimeStatus> statuses{};
    return statuses;
}

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
[[nodiscard]] int elapsedMillis(std::chrono::steady_clock::time_point started) {
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
#endif

} // namespace

bool loadPackage(std::string_view package_id) {
    const auto* definition = findPackage(package_id);
    if (!definition) {
        auto& status = packageStatuses()[std::string{package_id}];
        status.last_error = "unknown package id";
        spdlog::error("WebAssetPackageRegistry: unknown package '{}'.", package_id);
        return false;
    }

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    auto& status = packageStatuses()[std::string{definition->id}];
    if (status.loaded || isAssetPackageLoaded(definition->id)) {
        status.loaded = true;
        return true;
    }

    ++status.attempts;
    const auto started = std::chrono::steady_clock::now();
    spdlog::info(
        "WebAssetPackageRegistry: loading package '{}' from '{}'.",
        definition->id,
        definition->url);

    const bool loaded = loadAssetPackage(definition->id, definition->url);
    status.last_load_ms = elapsedMillis(started);
    status.loaded = loaded || isAssetPackageLoaded(definition->id);
    if (status.loaded) {
        status.last_error.clear();
        spdlog::info(
            "WebAssetPackageRegistry: package '{}' ready in {} ms.",
            definition->id,
            status.last_load_ms);
        return true;
    }

    status.last_error = "load failed from " + std::string{definition->url};
    spdlog::error(
        "WebAssetPackageRegistry: package '{}' failed after {} ms url='{}'.",
        definition->id,
        status.last_load_ms,
        definition->url);
    return false;
#else
    auto& status = packageStatuses()[std::string{definition->id}];
    status.loaded = true;
    status.last_error.clear();
    status.last_load_ms = 0;
    return true;
#endif
}

bool isPackageLoaded(std::string_view package_id) {
    const auto* definition = findPackage(package_id);
    if (!definition) {
        return false;
    }

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    if (isAssetPackageLoaded(definition->id)) {
        auto& status = packageStatuses()[std::string{definition->id}];
        status.loaded = true;
        return true;
    }
    const auto& statuses = packageStatuses();
    const auto it = statuses.find(std::string{definition->id});
    return it != statuses.end() && it->second.loaded;
#else
    return true;
#endif
}

std::string lastPackageError(std::string_view package_id) {
    const auto& statuses = packageStatuses();
    const auto it = statuses.find(std::string{package_id});
    return it == statuses.end() ? std::string{} : it->second.last_error;
}

std::string_view packageUrl(std::string_view package_id) {
    const auto* definition = findPackage(package_id);
    return definition ? definition->url : std::string_view{};
}

} // namespace engine::platform::web
