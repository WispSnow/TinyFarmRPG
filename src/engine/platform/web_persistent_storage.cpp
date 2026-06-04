#include "web_persistent_storage.h"

#if defined(__EMSCRIPTEN__)
#include "filesystem_paths.h"

#include <emscripten.h>
#include <spdlog/spdlog.h>
#endif

namespace engine::platform::web {
namespace {

#if defined(__EMSCRIPTEN__)
struct PendingSync {
    PersistentSyncCallback callback{};
    void* user_data{};
    bool in_progress{false};
    bool populate{false};
};

PendingSync& pendingSync() {
    static PendingSync sync{};
    return sync;
}

void completePendingSync(bool success) {
    auto& sync = pendingSync();
    const auto callback = sync.callback;
    void* user_data = sync.user_data;
    const bool populate = sync.populate;
    sync = {};
    spdlog::log(success ? spdlog::level::info : spdlog::level::warn,
                "TinyFarmRPG persistent FS sync completed direction={} success={}.",
                populate ? "from_browser" : "to_browser",
                success);
    if (callback != nullptr) {
        callback(success, user_data);
    }
}

void startSync(bool populate, PersistentSyncCallback callback, void* user_data) {
    auto& sync = pendingSync();
    if (sync.in_progress) {
        spdlog::warn("TinyFarmRPG persistent FS sync is already in progress.");
        if (callback != nullptr) {
            callback(false, user_data);
        }
        return;
    }
    sync.callback = callback;
    sync.user_data = user_data;
    sync.in_progress = true;
    sync.populate = populate;

    spdlog::info("TinyFarmRPG persistent FS sync started direction={} root='{}'.",
                 populate ? "from_browser" : "to_browser",
                 WEB_PERSISTENT_ROOT);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif
    EM_ASM({
        const root = UTF8ToString($0);
        const direction = $1 !== 0 ? "from_browser" : "to_browser";
        const diagnostics = globalThis.TinyFarmRPGWebReleaseDiagnostics || (globalThis.TinyFarmRPGWebReleaseDiagnostics = {});
        const storage = diagnostics.persistentStorage || (diagnostics.persistentStorage = {});
        storage.root = root;
        storage[direction + "Started"] = (storage[direction + "Started"] || 0) + 1;
        if (typeof FS === "undefined" || typeof IDBFS === "undefined") {
            console.error("TinyFarmRPG persistent FS dependencies are unavailable.");
            storage.lastError = "persistent FS dependencies are unavailable";
            storage[direction + "Failed"] = (storage[direction + "Failed"] || 0) + 1;
            Module._tf_web_persistent_sync_complete(1);
            return;
        }
        if (!Module.tinyFarmPersistentMounted) {
            storage.mountAttempts = (storage.mountAttempts || 0) + 1;
            if (!FS.analyzePath(root).exists) {
                FS.mkdir(root);
            }
            FS.mount(IDBFS, {}, root);
            Module.tinyFarmPersistentMounted = true;
            storage.mounted = true;
        }
        FS.syncfs($1 !== 0, (err) => {
            if (err) {
                storage.lastError = String(err);
                storage[direction + "Failed"] = (storage[direction + "Failed"] || 0) + 1;
            } else {
                storage.lastError = "";
                storage[direction + "Completed"] = (storage[direction + "Completed"] || 0) + 1;
            }
            Module._tf_web_persistent_sync_complete(err ? 1 : 0);
        });
    }, WEB_PERSISTENT_ROOT.data(), populate ? 1 : 0);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}
#endif

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE void tf_web_persistent_sync_complete(int result) {
    completePendingSync(result == 0);
}
#endif

void syncPersistentStorageFromBrowser(PersistentSyncCallback callback, void* user_data) {
#if defined(__EMSCRIPTEN__)
    startSync(true, callback, user_data);
#else
    if (callback != nullptr) {
        callback(true, user_data);
    }
#endif
}

void syncPersistentStorageToBrowser(PersistentSyncCallback callback, void* user_data) {
#if defined(__EMSCRIPTEN__)
    startSync(false, callback, user_data);
#else
    if (callback != nullptr) {
        callback(true, user_data);
    }
#endif
}

} // namespace engine::platform::web
