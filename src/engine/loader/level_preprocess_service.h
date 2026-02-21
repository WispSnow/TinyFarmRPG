#pragma once

#include "engine/loader/level_preprocess_data.h"

#include <string_view>

namespace engine::loader {

class LevelPreprocessService final {
public:
    [[nodiscard]] static LevelPreprocessResult preprocessLevel(std::string_view level_path);
};

} // namespace engine::loader

