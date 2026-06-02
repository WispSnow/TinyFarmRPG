#pragma once

namespace engine::platform {

[[nodiscard]] constexpr bool runtimeThreadingEnabled() {
#if defined(TF_ENABLE_RUNTIME_THREADS)
    return true;
#else
    return false;
#endif
}

[[nodiscard]] constexpr bool webPthreadsEnabled() {
#if defined(TF_WEB_ENABLE_PTHREADS)
    return true;
#else
    return false;
#endif
}

} // namespace engine::platform
