#pragma once

#include <entt/resource/cache.hpp>
#include <entt/resource/resource.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace engine::resource {

struct AudioBuffer {
    std::vector<float> samples;
    std::uint32_t channels{0};
    std::uint32_t sample_rate{0};
    std::uint64_t frame_count{0};

    [[nodiscard]] bool empty() const noexcept { return samples.empty() || channels == 0 || frame_count == 0; }
};

class AudioLoader final {
public:
    using result_type = std::shared_ptr<AudioBuffer>;

    [[nodiscard]] result_type operator()(std::string_view file_path) const;
};

using AudioBufferHandle = entt::resource<const AudioBuffer>;
using SoundCache = entt::resource_cache<AudioBuffer, AudioLoader>;
using MusicCache = entt::resource_cache<AudioBuffer, AudioLoader>;

} // namespace engine::resource
