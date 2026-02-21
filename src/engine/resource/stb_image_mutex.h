#pragma once

#include <mutex>

namespace engine::resource::detail {

[[nodiscard]] std::mutex& stbImageMutex();

} // namespace engine::resource::detail
