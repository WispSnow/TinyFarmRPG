#pragma once

namespace engine::platform::web {

using PersistentSyncCallback = void (*)(bool success, void* user_data);

void syncPersistentStorageFromBrowser(PersistentSyncCallback callback, void* user_data);
void syncPersistentStorageToBrowser(PersistentSyncCallback callback, void* user_data);

} // namespace engine::platform::web
