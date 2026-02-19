#pragma once

#include "engine/utils/defs.h"

#include <entt/resource/cache.hpp>
#include <entt/resource/resource.hpp>

#include <memory>
#include <string_view>

namespace engine::resource {

class TextureLoader final {
public:
    using result_type = std::shared_ptr<engine::utils::GL_Texture>;

    [[nodiscard]] result_type operator()(std::string_view file_path) const;
};

using TextureHandle = entt::resource<engine::utils::GL_Texture>;
using TextureCache = entt::resource_cache<engine::utils::GL_Texture, TextureLoader>;

} // namespace engine::resource
