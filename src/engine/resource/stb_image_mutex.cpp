#include "engine/resource/stb_image_mutex.h"

namespace engine::resource::detail {

std::mutex& stbImageMutex() {
    static std::mutex mutex{};
    return mutex;
}

} // namespace engine::resource::detail
