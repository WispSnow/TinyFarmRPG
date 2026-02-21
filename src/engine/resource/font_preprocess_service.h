#pragma once

#include "engine/resource/font_preprocess_data.h"

#include <optional>
#include <string_view>

namespace engine::resource {

class FontPreprocessService final {
public:
    [[nodiscard]] static std::optional<FontPreprocessData> rasterizeGlyphs(entt::id_type font_id,
                                                                            int pixel_size,
                                                                            std::string_view file_path,
                                                                            std::u32string_view codepoints);
};

} // namespace engine::resource

