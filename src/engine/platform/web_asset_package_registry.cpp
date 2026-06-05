#include "web_asset_package_registry.h"

#include "web_asset_package.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace engine::platform::web {
namespace {

struct PackageDefinition {
    std::string_view id;
    std::string_view url;
    std::array<std::string_view, 3> dependencies{};
    std::size_t dependency_count{0};
    int files{0};
    std::uint64_t bytes{0};
};

struct PackageRuntimeStatus {
    bool loaded{false};
    int attempts{0};
    int last_load_ms{0};
    int files{0};
    std::uint64_t bytes{0};
    std::string last_error{};
};

constexpr std::array<PackageDefinition, 8> PACKAGE_DEFINITIONS{{
    {PACKAGE_SHARED_UI, "web-packages/shared-ui.tfpack"},
    {PACKAGE_RPG_CORE, "web-packages/rpg-core.tfpack"},
    {PACKAGE_HOME_MAP, "web-packages/home-map.tfpack", {PACKAGE_RPG_CORE}, 1},
    {PACKAGE_TOWN_MAP, "web-packages/town-map.tfpack", {PACKAGE_HOME_MAP}, 1},
    {PACKAGE_SCHOOL_MAP, "web-packages/school-map.tfpack", {PACKAGE_TOWN_MAP}, 1},
    {PACKAGE_BATTLE_CORE, "web-packages/battle-core.tfpack", {PACKAGE_SHARED_UI, PACKAGE_RPG_CORE, PACKAGE_TOWN_MAP}, 3},
    {PACKAGE_VFX_CORE, "web-packages/vfx-core.tfpack", {PACKAGE_BATTLE_CORE}, 1},
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

PackageRuntimeStatus& statusFor(const PackageDefinition& definition) {
    auto& status = packageStatuses()[std::string{definition.id}];
    status.files = definition.files;
    status.bytes = definition.bytes;
    return status;
}

#if defined(__EMSCRIPTEN__)
[[nodiscard]] std::string dependencyListForDiagnostics(const PackageDefinition& definition) {
    std::string result{};
    for (std::size_t index = 0; index < definition.dependency_count; ++index) {
        if (!result.empty()) {
            result.push_back('\n');
        }
        result += definition.dependencies[index];
    }
    return result;
}
#endif

void publishPackageDiagnostics(const PackageDefinition& definition, const PackageRuntimeStatus& status) {
#if defined(__EMSCRIPTEN__)
    const std::string id{definition.id};
    const std::string url{definition.url};
    const std::string dependencies = dependencyListForDiagnostics(definition);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif
    EM_ASM({
        const diagnostics = globalThis.TinyFarmRPGWebReleaseDiagnostics || (globalThis.TinyFarmRPGWebReleaseDiagnostics = {});
        const packages = diagnostics.packages || (diagnostics.packages = {});
        const id = UTF8ToString($0);
        const dependencyText = UTF8ToString($6);
        const entry = packages[id] || (packages[id] = {});
        entry.id = id;
        entry.url = UTF8ToString($1);
        entry.loaded = !!$2;
        entry.attempts = $3;
        entry.lastLoadMs = $4;
        entry.lastError = UTF8ToString($5);
        entry.dependencies = dependencyText.length ? dependencyText.split("\n") : [];
        entry.lastUpdatedMs = Date.now();
    },
    id.c_str(),
    url.c_str(),
    status.loaded ? 1 : 0,
    status.attempts,
    status.last_load_ms,
    status.last_error.c_str(),
    dependencies.c_str());
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#else
    (void)definition;
    (void)status;
#endif
}

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
[[nodiscard]] int elapsedMillis(std::chrono::steady_clock::time_point started) {
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
#endif

[[nodiscard]] bool loadPackagePayload(const PackageDefinition& definition) {
#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    auto& status = statusFor(definition);
    if (status.loaded || isAssetPackageLoaded(definition.id)) {
        status.loaded = true;
        publishPackageDiagnostics(definition, status);
        return true;
    }

    ++status.attempts;
    const auto started = std::chrono::steady_clock::now();
    spdlog::info(
        "WebAssetPackageRegistry: loading package '{}' from '{}'.",
        definition.id,
        definition.url);

    const bool loaded = loadAssetPackage(definition.id, definition.url);
    status.last_load_ms = elapsedMillis(started);
    status.loaded = loaded || isAssetPackageLoaded(definition.id);
    if (status.loaded) {
        status.last_error.clear();
        publishPackageDiagnostics(definition, status);
        spdlog::info(
            "WebAssetPackageRegistry: package '{}' ready in {} ms.",
            definition.id,
            status.last_load_ms);
        return true;
    }

    status.last_error = "load failed from " + std::string{definition.url};
    publishPackageDiagnostics(definition, status);
    spdlog::error(
        "WebAssetPackageRegistry: package '{}' failed after {} ms url='{}'.",
        definition.id,
        status.last_load_ms,
        definition.url);
    return false;
#else
    auto& status = statusFor(definition);
    status.loaded = true;
    status.last_error.clear();
    status.last_load_ms = 0;
    publishPackageDiagnostics(definition, status);
    return true;
#endif
}

[[nodiscard]] bool loadPackageImpl(std::string_view package_id, std::size_t depth) {
    const auto* definition = findPackage(package_id);
    if (!definition) {
        auto& status = packageStatuses()[std::string{package_id}];
        status.last_error = "unknown package id";
        spdlog::error("WebAssetPackageRegistry: unknown package '{}'.", package_id);
        return false;
    }

    if (depth > PACKAGE_DEFINITIONS.size()) {
        auto& status = statusFor(*definition);
        status.last_error = "dependency cycle detected";
        spdlog::error("WebAssetPackageRegistry: dependency cycle while loading package '{}'.", package_id);
        return false;
    }

    bool dependencies_loaded = true;
    for (std::size_t index = 0; index < definition->dependency_count; ++index) {
        dependencies_loaded = loadPackageImpl(definition->dependencies[index], depth + 1) && dependencies_loaded;
    }
    return dependencies_loaded && loadPackagePayload(*definition);
}

} // namespace

bool loadPackage(std::string_view package_id) {
    return loadPackageImpl(package_id, 0);
}

bool loadGroup(std::initializer_list<std::string_view> package_ids) {
    bool loaded = true;
    for (const std::string_view package_id : package_ids) {
        loaded = loadPackage(package_id) && loaded;
    }
    return loaded;
}

bool isPackageLoaded(std::string_view package_id) {
    const auto* definition = findPackage(package_id);
    if (!definition) {
        return false;
    }

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    if (isAssetPackageLoaded(definition->id)) {
        auto& status = statusFor(*definition);
        status.loaded = true;
        publishPackageDiagnostics(*definition, status);
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

int packageFiles(std::string_view package_id) {
    const auto& statuses = packageStatuses();
    const auto status_it = statuses.find(std::string{package_id});
    if (status_it != statuses.end()) {
        return status_it->second.files;
    }
    const auto* definition = findPackage(package_id);
    return definition ? definition->files : 0;
}

std::uint64_t packageBytes(std::string_view package_id) {
    const auto& statuses = packageStatuses();
    const auto status_it = statuses.find(std::string{package_id});
    if (status_it != statuses.end()) {
        return status_it->second.bytes;
    }
    const auto* definition = findPackage(package_id);
    return definition ? definition->bytes : 0;
}

} // namespace engine::platform::web
