#include "text_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/resource/font_manager.h"
#include "opengl/gl_renderer.h"
#include "engine/utils/defs.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cctype>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <hb.h>
#include <hb-ft.h>  // HarfBuzz-FreeType 桥接
#include <entt/signal/dispatcher.hpp>

namespace engine::render {

namespace {

using engine::render::TextRenderer;
using engine::utils::ColorOptions;
using engine::utils::FColor;
using engine::utils::LayoutOptions;
using engine::utils::ShadowOptions;
using engine::utils::TextRenderParams;

[[nodiscard]] std::string toLowerAscii(std::string_view value) {
    std::string result(value.begin(), value.end());
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

[[nodiscard]] float jsonToFloat(const nlohmann::json& value, float fallback) {
    if (value.is_number_float() || value.is_number_integer() || value.is_number_unsigned()) {
        return static_cast<float>(value.get<double>());
    }
    return fallback;
}

[[nodiscard]] std::size_t jsonToSizeT(const nlohmann::json& value, std::size_t fallback) {
    if (value.is_number_unsigned()) {
        return value.get<std::size_t>();
    }
    if (value.is_number_integer()) {
        const auto integer_value = value.get<long long>();
        if (integer_value >= 0) {
            return static_cast<std::size_t>(integer_value);
        }
    }
    return fallback;
}

[[nodiscard]] bool jsonToBool(const nlohmann::json& value, bool fallback) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<long long>() != 0;
    }
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() != 0;
    }
    if (value.is_string()) {
        const std::string lowered = toLowerAscii(value.get<std::string>());
        if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on") {
            return true;
        }
        if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off") {
            return false;
        }
    }
    return fallback;
}

[[nodiscard]] glm::vec2 jsonToVec2(const nlohmann::json& value, glm::vec2 fallback) {
    glm::vec2 result = fallback;
    if (value.is_array()) {
        if (!value.empty()) {
            result.x = jsonToFloat(value.at(0), result.x);
            if (value.size() > 1) {
                result.y = jsonToFloat(value.at(1), result.y);
            }
        }
    } else if (value.is_object()) {
        if (auto it = value.find("x"); it != value.end()) {
            result.x = jsonToFloat(*it, result.x);
        }
        if (auto it = value.find("y"); it != value.end()) {
            result.y = jsonToFloat(*it, result.y);
        }
    }
    return result;
}

[[nodiscard]] FColor jsonToColor(const nlohmann::json& value, const FColor& fallback) {
    auto clamp01 = [](float v) {
        return std::clamp(v, 0.0f, 1.0f);
    };

    FColor result = fallback;
    if (value.is_array()) {
        if (!value.empty()) {
            result.r = clamp01(jsonToFloat(value.at(0), result.r));
            if (value.size() > 1) {
                result.g = clamp01(jsonToFloat(value.at(1), result.g));
            }
            if (value.size() > 2) {
                result.b = clamp01(jsonToFloat(value.at(2), result.b));
            }
            if (value.size() > 3) {
                result.a = clamp01(jsonToFloat(value.at(3), result.a));
            }
        }
    } else if (value.is_object()) {
        if (auto it = value.find("r"); it != value.end()) {
            result.r = clamp01(jsonToFloat(*it, result.r));
        }
        if (auto it = value.find("g"); it != value.end()) {
            result.g = clamp01(jsonToFloat(*it, result.g));
        }
        if (auto it = value.find("b"); it != value.end()) {
            result.b = clamp01(jsonToFloat(*it, result.b));
        }
        if (auto it = value.find("a"); it != value.end()) {
            result.a = clamp01(jsonToFloat(*it, result.a));
        } else if (auto it_alpha = value.find("alpha"); it_alpha != value.end()) {
            result.a = clamp01(jsonToFloat(*it_alpha, result.a));
        }
    } else if (value.is_string()) {
        result = engine::utils::parseHexColor(value.get<std::string>());
    }

    result.r = clamp01(result.r);
    result.g = clamp01(result.g);
    result.b = clamp01(result.b);
    result.a = clamp01(result.a);
    return result;
}

[[nodiscard]] LayoutOptions sanitizeLayoutOptions(const LayoutOptions& options) {
    LayoutOptions sanitized = options;

    auto sanitizeFinite = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };

    sanitized.letter_spacing = sanitizeFinite(sanitized.letter_spacing, 0.0f);
    sanitized.line_spacing_scale = sanitizeFinite(sanitized.line_spacing_scale, 1.0f);
    sanitized.glyph_scale.x = sanitizeFinite(sanitized.glyph_scale.x, 1.0f);
    sanitized.glyph_scale.y = sanitizeFinite(sanitized.glyph_scale.y, 1.0f);

    sanitized.line_spacing_scale = std::max(sanitized.line_spacing_scale, 0.01f);
    sanitized.glyph_scale.x = std::max(sanitized.glyph_scale.x, 0.01f);
    sanitized.glyph_scale.y = std::max(sanitized.glyph_scale.y, 0.01f);
    return sanitized;
}

[[nodiscard]] ColorOptions jsonToColorOptions(const nlohmann::json& value, const ColorOptions& fallback) {
    if (!value.is_object()) {
        return fallback;
    }

    ColorOptions result = fallback;
    if (auto it = value.find("start_color"); it != value.end()) {
        result.start_color = jsonToColor(*it, result.start_color);
    } else if (auto it = value.find("start"); it != value.end()) {
        result.start_color = jsonToColor(*it, result.start_color);
    }

    if (auto it = value.find("end_color"); it != value.end()) {
        result.end_color = jsonToColor(*it, result.end_color);
    } else if (auto it = value.find("end"); it != value.end()) {
        result.end_color = jsonToColor(*it, result.end_color);
    }

    if (auto it = value.find("use_gradient"); it != value.end()) {
        result.use_gradient = jsonToBool(*it, result.use_gradient);
    }

    if (auto it = value.find("angle_radians"); it != value.end()) {
        result.angle_radians = jsonToFloat(*it, result.angle_radians);
    } else if (auto it = value.find("angle_degrees"); it != value.end()) {
        const float degrees = jsonToFloat(*it, result.angle_radians * 180.0f / glm::pi<float>());
        result.angle_radians = degrees * glm::pi<float>() / 180.0f;
    }

    return result;
}

[[nodiscard]] ShadowOptions jsonToShadowOptions(const nlohmann::json& value, const ShadowOptions& fallback) {
    if (!value.is_object()) {
        return fallback;
    }

    ShadowOptions result = fallback;
    if (auto it = value.find("enabled"); it != value.end()) {
        result.enabled = jsonToBool(*it, result.enabled);
    }
    if (auto it = value.find("offset"); it != value.end()) {
        result.offset = jsonToVec2(*it, result.offset);
    }
    if (auto it = value.find("color"); it != value.end()) {
        result.color = jsonToColor(*it, result.color);
    }
    return result;
}

[[nodiscard]] LayoutOptions jsonToLayoutOptions(const nlohmann::json& value, const LayoutOptions& fallback) {
    if (!value.is_object()) {
        return sanitizeLayoutOptions(fallback);
    }

    LayoutOptions result = fallback;
    if (auto it = value.find("letter_spacing"); it != value.end()) {
        result.letter_spacing = jsonToFloat(*it, result.letter_spacing);
    }
    if (auto it = value.find("line_spacing_scale"); it != value.end()) {
        result.line_spacing_scale = jsonToFloat(*it, result.line_spacing_scale);
    }
    if (auto it = value.find("glyph_scale"); it != value.end()) {
        result.glyph_scale = jsonToVec2(*it, result.glyph_scale);
    } else {
        if (auto it = value.find("glyph_scale_x"); it != value.end()) {
            result.glyph_scale.x = jsonToFloat(*it, result.glyph_scale.x);
        }
        if (auto it = value.find("glyph_scale_y"); it != value.end()) {
            result.glyph_scale.y = jsonToFloat(*it, result.glyph_scale.y);
        }
    }

    return sanitizeLayoutOptions(result);
}

/// 空样式常量，用于所有"找不到样式时"的返回引用
const TextRenderParams EMPTY_TEXT_PARAMS{};

[[nodiscard]] TextRenderParams jsonToTextRenderParams(const nlohmann::json& value, const TextRenderParams& fallback) {
    if (!value.is_object()) {
        return fallback;
    }

    TextRenderParams result = fallback;
    if (auto it = value.find("color"); it != value.end()) {
        result.color = jsonToColorOptions(*it, result.color);
    }
    if (auto it = value.find("shadow"); it != value.end()) {
        result.shadow = jsonToShadowOptions(*it, result.shadow);
    }
    if (auto it = value.find("layout"); it != value.end()) {
        result.layout = jsonToLayoutOptions(*it, result.layout);
    }
    return result;
}

[[nodiscard]] TextRenderer::TextDirection parseDirection(std::string_view value,
                                                         TextRenderer::TextDirection fallback) {
    const std::string lowered = toLowerAscii(value);
    if (lowered == "ltr" || lowered == "lefttoright" || lowered == "left_to_right") {
        return TextRenderer::TextDirection::LeftToRight;
    }
    if (lowered == "rtl" || lowered == "righttoleft" || lowered == "right_to_left") {
        return TextRenderer::TextDirection::RightToLeft;
    }
    if (lowered == "ttb" || lowered == "toptobottom" || lowered == "top_to_bottom") {
        return TextRenderer::TextDirection::TopToBottom;
    }
    if (lowered == "btt" || lowered == "bottomtotop" || lowered == "bottom_to_top") {
        return TextRenderer::TextDirection::BottomToTop;
    }
    return fallback;
}

} // namespace

float TextRenderer::shapeLine(std::string_view line_text,
                              engine::resource::Font* font,
                              const engine::utils::LayoutOptions& options,
                              float pen_y_origin,
                              std::vector<GlyphPlacement>& out_glyphs) const {
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_set_direction(buffer, toHbDirection(default_direction_));
    hb_buffer_set_language(buffer, hb_language_from_string(default_language_tag_.c_str(), -1));
    hb_buffer_add_utf8(buffer,
                       line_text.data(),
                       static_cast<int>(line_text.size()),
                       0,
                       static_cast<int>(line_text.size()));
    hb_buffer_guess_segment_properties(buffer);

    const hb_feature_t* feature_data = active_features_.empty() ? nullptr : active_features_.data();
    unsigned int feature_count = static_cast<unsigned int>(active_features_.size());
    hb_shape(font->getHBFont(), buffer, feature_data, feature_count);

    unsigned int glyph_count = 0;
    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
    hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &glyph_count);

    float pen_x = 0.0f;
    float pen_y = pen_y_origin;

    float line_min_x = std::numeric_limits<float>::max();
    float line_max_x = std::numeric_limits<float>::lowest();

    for (unsigned int i = 0; i < glyph_count; ++i) {
        const engine::resource::FontGlyph* glyph = font->getGlyphByIndex(infos[i].codepoint);
        const float advance_x = positions[i].x_advance / 64.0f;
        const float advance_y = positions[i].y_advance / 64.0f;
        const float offset_x = positions[i].x_offset / 64.0f;
        const float offset_y = positions[i].y_offset / 64.0f;

        const float scaled_advance_x = advance_x * options.glyph_scale.x;
        const float scaled_advance_y = advance_y * options.glyph_scale.y;
        const float scaled_offset_x = offset_x * options.glyph_scale.x;
        const float scaled_offset_y = offset_y * options.glyph_scale.y;

        if (glyph) {
            const glm::vec2 bearing = glm::vec2(static_cast<float>(glyph->bearing.x),
                                                 static_cast<float>(glyph->bearing.y));
            const glm::vec2 scaled_bearing = bearing * options.glyph_scale;
            const glm::vec2 glyph_size = glm::vec2(static_cast<float>(glyph->size.x),
                                                   static_cast<float>(glyph->size.y));
            const glm::vec2 scaled_size = glyph_size * options.glyph_scale;

            if (glyph->texture != 0 && glyph->size.x > 0 && glyph->size.y > 0) {
                glm::vec4 dest_rect{
                    pen_x + scaled_offset_x + scaled_bearing.x,
                    pen_y - scaled_offset_y - scaled_bearing.y,
                    scaled_size.x,
                    scaled_size.y
                };
                out_glyphs.push_back({glyph, dest_rect});
            }

            const float dest_x = pen_x + scaled_offset_x + scaled_bearing.x;
            const float dest_right = dest_x + scaled_size.x;
            line_min_x = std::min(line_min_x, dest_x);
            line_max_x = std::max(line_max_x, dest_right);
        }

        pen_x += scaled_advance_x;
        pen_y -= scaled_advance_y;

        if (i + 1 < glyph_count) {
            pen_x += options.letter_spacing;
        }
    }

    hb_buffer_destroy(buffer);

    const float effective_min = (line_min_x == std::numeric_limits<float>::max())
                                    ? 0.0f
                                    : std::min(0.0f, line_min_x);
    const float effective_max = (line_max_x == std::numeric_limits<float>::lowest())
                                    ? std::max(0.0f, pen_x)
                                    : std::max({line_max_x, pen_x, 0.0f});
    return std::max(0.0f, effective_max - effective_min);
}

const TextRenderer::TextLayout* TextRenderer::buildLayout(std::string_view text,
                                                          entt::id_type font_id,
                                                          int font_size,
                                                          const engine::utils::LayoutOptions& layout_options,
                                                          std::string_view font_path) const {
    if (!resource_manager_) {
        return nullptr;
    }

    ++layout_frame_counter_;

    const engine::utils::LayoutOptions sanitized_options = sanitizeLayoutOptions(layout_options);

    LayoutKey key{font_id, font_size, std::string(text), sanitized_options};
    if (auto it = layout_cache_.find(key); it != layout_cache_.end()) {
        it->second.usage_frame = layout_frame_counter_;
        return &it->second;
    }

    engine::resource::Font* font = resource_manager_->getFont(font_id, font_size, font_path);
    if (!font) {
        return nullptr;
    }

    TextLayout layout{};
    layout.font = font;
    layout.glyphs.reserve(text.size());

    const float ascender = font->getAscender();
    const float descender = font->getDescender();
    const float line_height = font->getLineHeight();

    const float ascender_scaled = ascender * sanitized_options.glyph_scale.y;
    const float descender_scaled = descender * sanitized_options.glyph_scale.y;
    const float effective_line_height = line_height * sanitized_options.line_spacing_scale * sanitized_options.glyph_scale.y;

    float max_line_width = 0.0f;
    int line_count = 0;

    size_t line_start = 0;
    while (line_start <= text.size()) {
        const size_t newline_pos = text.find('\n', line_start);
        const bool is_last_line = (newline_pos == std::string::npos);
        const size_t line_length = is_last_line ? (text.size() - line_start) : (newline_pos - line_start);
        std::string_view line_view = text.substr(line_start, line_length);

        const float pen_y_origin = ascender_scaled + static_cast<float>(line_count) * effective_line_height;
        const float line_width = shapeLine(line_view, font, sanitized_options, pen_y_origin, layout.glyphs);
        max_line_width = std::max(max_line_width, line_width);

        ++line_count;
        if (is_last_line) {
            break;
        }
        line_start = newline_pos + 1;
    }

    layout.line_count = line_count;
    if (line_count == 0) {
        layout.size = glm::vec2(0.0f);
        return nullptr;
    }

    float total_height = ascender_scaled + descender_scaled;
    if (line_count > 1) {
        total_height += (static_cast<float>(line_count - 1) * effective_line_height);
    }
    layout.size = {max_line_width, total_height};

    layout.usage_frame = layout_frame_counter_;

    auto [it, inserted] = layout_cache_.emplace(std::move(key), std::move(layout));
    (void)inserted;
    trimLayoutCache();
    return &(it->second);
}

std::unique_ptr<TextRenderer> TextRenderer::create(engine::render::opengl::GLRenderer* gl_renderer,
                                                   engine::resource::ResourceManager* resource_manager,
                                                   entt::dispatcher* dispatcher) {
    if (gl_renderer == nullptr) {
        spdlog::error("创建 TextRenderer 失败：提供的 GLRenderer 指针为空。");
        return nullptr;
    }
    if (resource_manager == nullptr) {
        spdlog::error("创建 TextRenderer 失败：提供的 ResourceManager 指针为空。");
        return nullptr;
    }
    if (dispatcher == nullptr) {
        spdlog::error("创建 TextRenderer 失败：提供的事件分发器指针为空。");
        return nullptr;
    }
    return std::unique_ptr<TextRenderer>(new TextRenderer(gl_renderer, resource_manager, dispatcher));
}

TextRenderer::TextRenderer(engine::render::opengl::GLRenderer* gl_renderer,
                           engine::resource::ResourceManager* resource_manager,
                           entt::dispatcher* dispatcher)
    : gl_renderer_(gl_renderer),
      resource_manager_(resource_manager),
      dispatcher_(dispatcher) {
    dispatcher_->sink<engine::utils::FontUnloadedEvent>().connect<&TextRenderer::onFontUnloaded>(this);
    dispatcher_->sink<engine::utils::FontsClearedEvent>().connect<&TextRenderer::onFontsCleared>(this);
    ensureBuiltinStyles();
    [[maybe_unused]] const bool config_loaded = loadConfig(DEFAULT_CONFIG_PATH);
    spdlog::trace("TextRenderer 初始化成功.");
}

TextRenderer::~TextRenderer() {
    if (dispatcher_) {
        dispatcher_->disconnect(this);
        dispatcher_ = nullptr;
    }
}

bool TextRenderer::loadConfig(std::string_view config_path) {
    if (config_path.empty()) {
        spdlog::warn("TextRenderer::loadConfig 收到空的配置路径，跳过加载。");
        return false;
    }

    const std::filesystem::path path{config_path};
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("TextRenderer::loadConfig 无法打开配置文件 '{}'，使用内建默认值。", path.string());
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::error("TextRenderer::loadConfig 解析 '{}' 失败: JSON 解析失败。", path.string());
        return false;
    }
    if (!root.is_object()) {
        spdlog::error("TextRenderer::loadConfig 解析 '{}' 失败: 根节点不是对象。", path.string());
        return false;
    }

    const nlohmann::json* config = &root;
    if (auto it = config->find("text_renderer"); it != config->end() && it->is_object()) {
        config = &(*it);
    }

    auto applyDirectionNode = [&](const nlohmann::json& node) {
        TextDirection parsed = default_direction_;
        if (const auto* v = node.get_ptr<const nlohmann::json::string_t*>()) {
            parsed = parseDirection(*v, default_direction_);
        } else if (node.is_number_integer() || node.is_number_unsigned()) {
            switch (node.get<int>()) {
                case 0: parsed = TextDirection::LeftToRight; break;
                case 1: parsed = TextDirection::RightToLeft; break;
                case 2: parsed = TextDirection::TopToBottom; break;
                case 3: parsed = TextDirection::BottomToTop; break;
                default: break;
            }
        }
        if (parsed != default_direction_) {
            setDefaultDirection(parsed);
        }
    };

    auto applyLanguageNode = [&](const nlohmann::json& node) {
        if (const auto* v = node.get_ptr<const nlohmann::json::string_t*>()) {
            if (*v != default_language_tag_) {
                setDefaultLanguage(*v);
            }
        }
    };

    if (auto it = config->find("direction"); it != config->end()) {
        applyDirectionNode(*it);
    } else if (auto it = config->find("default_direction"); it != config->end()) {
        applyDirectionNode(*it);
    }

    if (auto it = config->find("language"); it != config->end()) {
        applyLanguageNode(*it);
    } else if (auto it = config->find("language_tag"); it != config->end()) {
        applyLanguageNode(*it);
    }

    if (auto it = config->find("layout_cache_capacity"); it != config->end()) {
        const std::size_t capacity = jsonToSizeT(*it, layout_cache_capacity_);
        if (capacity != layout_cache_capacity_) {
            setLayoutCacheCapacity(capacity);
        }
    }

    if (auto it = config->find("features"); it != config->end()) {
        std::vector<std::string> features;
        if (it->is_array()) {
            features.reserve(it->size());
            for (const auto& entry : *it) {
                if (const auto* v = entry.get_ptr<const nlohmann::json::string_t*>()) {
                    features.emplace_back(*v);
                }
            }
        } else if (const auto* v = it->get_ptr<const nlohmann::json::string_t*>()) {
            features.emplace_back(*v);
        }
        setFeatures(features);
    }

    const bool layout_changed = loadConfigStyles(*config);
    if (layout_changed) {
        clearLayoutCache();
        ++layout_revision_;
    }

    spdlog::info("TextRenderer::loadConfig 成功从 '{}' 载入配置。", path.string());
    return true;
}

bool TextRenderer::loadConfigStyles(const nlohmann::json& config) {
    bool layout_changed = false;
    decltype(text_styles_) loaded_styles = text_styles_;

    if (auto it = config.find("styles"); it != config.end() && it->is_object()) {
        loaded_styles.clear();
        for (auto entry = it->begin(); entry != it->end(); ++entry) {
            if (!entry.value().is_object()) {
                continue;
            }

            const TextRenderParams parsed = jsonToTextRenderParams(entry.value(), TextRenderParams{});
            const std::string& style_key = entry.key();
            const entt::id_type style_id = toTextStyleId(style_key);
            if (style_id == entt::null) {
                continue;
            }

            auto it_existing = loaded_styles.find(style_id);
            if (it_existing != loaded_styles.end()) {
                if (it_existing->second.key != style_key) {
                    spdlog::error("TextRenderer::loadConfig 检测到文本样式 key 哈希冲突: '{}' vs '{}'。",
                                  it_existing->second.key,
                                  style_key);
                    continue;
                }
                it_existing->second.params = parsed;
            } else {
                loaded_styles.emplace(style_id, TextStyleEntry{style_key, parsed});
            }
        }
    }

    if (auto it = config.find("default_style_keys"); it != config.end() && it->is_object()) {
        if (auto it_ui = it->find("ui"); it_ui != it->end() && it_ui->is_string()) {
            const std::string key = it_ui->get<std::string>();
            const entt::id_type style_id = toTextStyleId(key);
            if (style_id != entt::null && loaded_styles.find(style_id) == loaded_styles.end()) {
                loaded_styles.emplace(style_id, TextStyleEntry{key, TextRenderParams{}});
                layout_changed = true;
            }
            default_ui_style_id_ = style_id;
        }
        if (auto it_world = it->find("world"); it_world != it->end() && it_world->is_string()) {
            const std::string key = it_world->get<std::string>();
            const entt::id_type style_id = toTextStyleId(key);
            if (style_id != entt::null && loaded_styles.find(style_id) == loaded_styles.end()) {
                loaded_styles.emplace(style_id, TextStyleEntry{key, TextRenderParams{}});
                layout_changed = true;
            }
            default_world_style_id_ = style_id;
        }
    }

    const entt::id_type builtin_ui_style = toTextStyleId("ui/default");
    const entt::id_type builtin_world_style = toTextStyleId("world/default");
    if (builtin_ui_style != entt::null && loaded_styles.find(builtin_ui_style) == loaded_styles.end()) {
        loaded_styles.emplace(builtin_ui_style, TextStyleEntry{"ui/default", TextRenderParams{}});
    }
    if (builtin_world_style != entt::null && loaded_styles.find(builtin_world_style) == loaded_styles.end()) {
        loaded_styles.emplace(builtin_world_style, TextStyleEntry{"world/default", TextRenderParams{}});
    }

    if (default_ui_style_id_ == entt::null || loaded_styles.find(default_ui_style_id_) == loaded_styles.end()) {
        default_ui_style_id_ = builtin_ui_style;
    }
    if (default_world_style_id_ == entt::null || loaded_styles.find(default_world_style_id_) == loaded_styles.end()) {
        default_world_style_id_ = builtin_world_style;
    }

    if (loaded_styles.size() != text_styles_.size()) {
        layout_changed = true;
    } else {
        for (const auto& [key, entry] : loaded_styles) {
            if (auto it = text_styles_.find(key); it == text_styles_.end()) {
                layout_changed = true;
                break;
            } else if (it->second.params.layout != entry.params.layout) {
                layout_changed = true;
                break;
            }
        }
    }

    text_styles_ = std::move(loaded_styles);
    ensureBuiltinStyles();
    return layout_changed;
}

void TextRenderer::drawUIText(std::string_view text,
                              entt::id_type font_id,
                              int font_size,
                              const glm::vec2& position,
                              entt::id_type style_id,
                              const engine::utils::TextRenderOverrides* overrides) const {
    const engine::utils::TextRenderParams resolved = resolveTextParams(style_id, default_ui_style_id_, overrides);
    drawTextInternal(text, font_id, font_size, position, resolved, true);
}

void TextRenderer::drawUIText(std::string_view text,
                              entt::id_type font_id,
                              int font_size,
                              const glm::vec2& position,
                              std::string_view style_key,
                              const engine::utils::TextRenderOverrides* overrides) const {
    drawUIText(text, font_id, font_size, position, toTextStyleId(style_key), overrides);
}

void TextRenderer::drawText(std::string_view text,
                            entt::id_type font_id,
                            int font_size,
                            const glm::vec2& position,
                            entt::id_type style_id,
                            const engine::utils::TextRenderOverrides* overrides) const {
    const engine::utils::TextRenderParams resolved = resolveTextParams(style_id, default_world_style_id_, overrides);
    drawTextInternal(text, font_id, font_size, position, resolved, false);
}

void TextRenderer::drawText(std::string_view text,
                            entt::id_type font_id,
                            int font_size,
                            const glm::vec2& position,
                            std::string_view style_key,
                            const engine::utils::TextRenderOverrides* overrides) const {
    drawText(text, font_id, font_size, position, toTextStyleId(style_key), overrides);
}

glm::vec2 TextRenderer::getTextSize(std::string_view text,
                                    entt::id_type font_id,
                                    int font_size,
                                    std::string_view font_path,
                                    const engine::utils::LayoutOptions* layout_options) const {
    if (!resource_manager_) {
        return glm::vec2(0.0f);
    }

    const engine::utils::LayoutOptions options = layout_options ? *layout_options : engine::utils::LayoutOptions{};
    const TextLayout* layout = buildLayout(text, font_id, font_size, options, font_path);
    if (!layout) {
        if (font_path.empty()) {
            spdlog::warn("getTextSize 获取字体失败: font_id={} size={} (未提供 font_path，且缓存未命中)",
                         font_id,
                         font_size);
        } else {
            spdlog::warn("getTextSize 获取字体失败: font_id={} size={} path='{}'",
                         font_id,
                         font_size,
                         font_path);
        }
        return glm::vec2(0.0f);
    }
    return layout->size;
}

void TextRenderer::drawTextInternal(std::string_view text,
                                    entt::id_type font_id,
                                    int font_size,
                                    const glm::vec2& position,
                                    const engine::utils::TextRenderParams& params,
                                    bool use_ui_pass) const {
    if (!gl_renderer_ || !resource_manager_) {
        return;
    }

    const TextLayout* layout = buildLayout(text, font_id, font_size, params.layout);
    if (!layout || !layout->font) {
        spdlog::debug("drawTextInternal 获取字体失败: font_id={} size={} (提示：先调用 getTextSize(..., font_path) 预加载字体)",
                      font_id,
                      font_size);
        return;
    }

    for (const auto& placement : layout->glyphs) {
        const auto* glyph = placement.glyph;
        if (!glyph || glyph->texture == 0 || placement.dest_rect.z <= 0.0f || placement.dest_rect.w <= 0.0f) {
            continue;
        }

        glm::vec4 dest_rect = placement.dest_rect;
        dest_rect.x += position.x;
        dest_rect.y += position.y;

        const glm::vec4& uv_rect = glyph->uv_rect;
        if (uv_rect.z <= uv_rect.x || uv_rect.w <= uv_rect.y) {
            continue;
        }

        const bool culled = !use_ui_pass && gl_renderer_->shouldCullRect(
            engine::utils::Rect(dest_rect.x,
                                dest_rect.y,
                                dest_rect.z,
                                dest_rect.w));
        if (culled) {
            continue;
        }

        if (params.shadow.enabled) {
            engine::utils::ColorOptions shadow_color_options{};
            shadow_color_options.start_color = params.shadow.color;
            glm::vec4 shadow_rect = dest_rect;
            shadow_rect.x += params.shadow.offset.x;
            shadow_rect.y += params.shadow.offset.y;
            if (use_ui_pass) {
                gl_renderer_->drawUITexture(glyph->texture, shadow_rect, uv_rect,
                                            &shadow_color_options);
            } else {
                gl_renderer_->drawTexture(glyph->texture, shadow_rect, uv_rect,
                                          &shadow_color_options);
            }
        }

        if (use_ui_pass) {
            gl_renderer_->drawUITexture(glyph->texture, dest_rect, uv_rect,
                                        &params.color);
        } else {
            gl_renderer_->drawTexture(glyph->texture, dest_rect, uv_rect,
                                      &params.color);
        }
    }
}

hb_direction_t TextRenderer::toHbDirection(TextDirection direction) const {
    switch (direction) {
        case TextDirection::LeftToRight: return HB_DIRECTION_LTR;
        case TextDirection::RightToLeft: return HB_DIRECTION_RTL;
        case TextDirection::TopToBottom: return HB_DIRECTION_TTB;
        case TextDirection::BottomToTop: return HB_DIRECTION_BTT;
        default: return HB_DIRECTION_LTR;
    }
}

void TextRenderer::setDefaultDirection(TextDirection direction) {
    default_direction_ = direction;
    clearLayoutCache();
    ++layout_revision_;
}

void TextRenderer::setDefaultLanguage(std::string_view language_tag) {
    if (language_tag.empty()) {
        default_language_tag_ = "zh-Hans";
    } else {
        default_language_tag_.assign(language_tag.begin(), language_tag.end());
    }
    clearLayoutCache();
    ++layout_revision_;
}

void TextRenderer::setFeatures(const std::vector<std::string>& feature_specs) {
    active_features_.clear();
    active_features_.reserve(feature_specs.size());
    for (const auto& spec : feature_specs) {
        hb_feature_t feature{};
        if (hb_feature_from_string(spec.c_str(), static_cast<int>(spec.size()), &feature)) {
            active_features_.push_back(feature);
        } else {
            spdlog::warn("无法解析 HarfBuzz 特性字符串: {}", spec);
        }
    }
    clearLayoutCache();
    ++layout_revision_;
}

void TextRenderer::onFontUnloaded(const engine::utils::FontUnloadedEvent& event) {
    if (layout_cache_.empty()) {
        return;
    }
    const auto removed_count = std::erase_if(layout_cache_, [&](const auto& entry) {
        return entry.first.font_id == event.font_id && entry.first.font_size == event.font_size;
    });
    if (layout_cache_.empty()) {
        layout_frame_counter_ = 0;
    }
    if (removed_count > 0) {
        ++layout_revision_;
    }
}

void TextRenderer::onFontsCleared(const engine::utils::FontsClearedEvent&) {
    clearLayoutCache();
    ++layout_revision_;
}

void TextRenderer::clearLayoutCache() {
    layout_cache_.clear();
    layout_frame_counter_ = 0;
}

void TextRenderer::setLayoutCacheCapacity(std::size_t capacity) {
    layout_cache_capacity_ = std::max<std::size_t>(1, capacity);
    trimLayoutCache();
}

entt::id_type TextRenderer::toTextStyleId(std::string_view key) {
    if (key.empty()) {
        return entt::null;
    }
    return entt::hashed_string{key.data(), key.size()};
}

std::string_view TextRenderer::getDefaultUIStyleKey() const {
    return getTextStyleKey(default_ui_style_id_);
}

std::string_view TextRenderer::getDefaultWorldStyleKey() const {
    return getTextStyleKey(default_world_style_id_);
}

bool TextRenderer::hasTextStyle(entt::id_type style_id) const {
    if (style_id == entt::null) {
        return false;
    }
    return text_styles_.find(style_id) != text_styles_.end();
}

bool TextRenderer::hasTextStyle(std::string_view key) const {
    if (key.empty()) {
        return false;
    }
    const entt::id_type style_id = toTextStyleId(key);
    if (style_id == entt::null) {
        return false;
    }
    if (auto it = text_styles_.find(style_id); it != text_styles_.end()) {
        return it->second.key == key;
    }
    return false;
}

std::string_view TextRenderer::getTextStyleKey(entt::id_type style_id) const {
    if (style_id == entt::null) {
        return {};
    }
    if (auto it = text_styles_.find(style_id); it != text_styles_.end()) {
        return it->second.key;
    }
    return {};
}

const engine::utils::TextRenderParams& TextRenderer::getTextStyle(entt::id_type style_id) const {
    if (style_id != entt::null) {
        if (auto it = text_styles_.find(style_id); it != text_styles_.end()) {
            return it->second.params;
        }
    }
    return EMPTY_TEXT_PARAMS;
}

const engine::utils::TextRenderParams& TextRenderer::getTextStyle(std::string_view key) const {
    if (key.empty()) {
        return EMPTY_TEXT_PARAMS;
    }
    const entt::id_type style_id = toTextStyleId(key);
    if (style_id == entt::null) {
        return EMPTY_TEXT_PARAMS;
    }
    if (auto it = text_styles_.find(style_id); it != text_styles_.end() && it->second.key == key) {
        return it->second.params;
    }
    return EMPTY_TEXT_PARAMS;
}

bool TextRenderer::setTextStyle(std::string_view key, const engine::utils::TextRenderParams& params) {
    if (key.empty()) {
        return false;
    }

    const entt::id_type style_id = toTextStyleId(key);
    if (style_id == entt::null) {
        return false;
    }

    bool layout_changed = true;
    if (auto it = text_styles_.find(style_id); it != text_styles_.end()) {
        if (it->second.key != key) {
            spdlog::error("TextRenderer::setTextStyle 检测到文本样式 key 哈希冲突: '{}' vs '{}'。",
                          it->second.key,
                          key);
            return false;
        }

        layout_changed = (it->second.params.layout != params.layout);
        it->second.params = params;
    } else {
        text_styles_.emplace(style_id, TextStyleEntry{std::string(key), params});
    }

    if (layout_changed) {
        clearLayoutCache();
        ++layout_revision_;
    }
    return true;
}

std::vector<std::string> TextRenderer::listTextStyleKeys(std::string_view prefix) const {
    std::vector<std::string> keys;
    keys.reserve(text_styles_.size());
    for (const auto& [style_id, entry] : text_styles_) {
        (void)style_id;
        if (prefix.empty() || entry.key.starts_with(prefix)) {
            keys.push_back(entry.key);
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

void TextRenderer::ensureBuiltinStyles() {
    const entt::id_type ui_default_id = toTextStyleId("ui/default");
    const entt::id_type world_default_id = toTextStyleId("world/default");

    if (ui_default_id != entt::null && text_styles_.find(ui_default_id) == text_styles_.end()) {
        text_styles_.emplace(ui_default_id, TextStyleEntry{"ui/default", engine::utils::TextRenderParams{}});
    }
    if (world_default_id != entt::null && text_styles_.find(world_default_id) == text_styles_.end()) {
        text_styles_.emplace(world_default_id, TextStyleEntry{"world/default", engine::utils::TextRenderParams{}});
    }

    if (default_ui_style_id_ == entt::null) {
        default_ui_style_id_ = ui_default_id;
    }
    if (default_world_style_id_ == entt::null) {
        default_world_style_id_ = world_default_id;
    }

    if (default_ui_style_id_ != entt::null && text_styles_.find(default_ui_style_id_) == text_styles_.end()) {
        default_ui_style_id_ = ui_default_id;
    }
    if (default_world_style_id_ != entt::null && text_styles_.find(default_world_style_id_) == text_styles_.end()) {
        default_world_style_id_ = world_default_id;
    }
}

const engine::utils::TextRenderParams& TextRenderer::getStyleOrFallback(entt::id_type style_id,
                                                                        entt::id_type fallback_id) const {
    if (style_id != entt::null) {
        if (auto it = text_styles_.find(style_id); it != text_styles_.end()) {
            return it->second.params;
        }
    }
    if (fallback_id != entt::null) {
        if (auto it = text_styles_.find(fallback_id); it != text_styles_.end()) {
            return it->second.params;
        }
    }
    return EMPTY_TEXT_PARAMS;
}

engine::utils::TextRenderParams TextRenderer::resolveTextParams(entt::id_type style_id,
                                                                entt::id_type fallback_id,
                                                                const engine::utils::TextRenderOverrides* overrides) const {
    const entt::id_type resolved_id = style_id == entt::null ? fallback_id : style_id;
    const auto& base = getStyleOrFallback(resolved_id, fallback_id);
    engine::utils::TextRenderParams resolved = base;
    if (!overrides || overrides->isEmpty()) {
        return resolved;
    }

    if (overrides->color) {
        resolved.color.start_color = *overrides->color;
        resolved.color.end_color = *overrides->color;
        resolved.color.use_gradient = false;
    }
    if (overrides->shadow_enabled) {
        resolved.shadow.enabled = *overrides->shadow_enabled;
    }
    if (overrides->shadow_offset) {
        resolved.shadow.offset = *overrides->shadow_offset;
    }
    if (overrides->shadow_color) {
        resolved.shadow.color = *overrides->shadow_color;
    }
    if (overrides->glyph_scale) {
        resolved.layout.glyph_scale = *overrides->glyph_scale;
    }

    return resolved;
}

void TextRenderer::trimLayoutCache() const {
    if (layout_cache_.empty()) {
        return;
    }
    while (layout_cache_.size() > layout_cache_capacity_) {
        const auto oldest = std::min_element(
            layout_cache_.begin(),
            layout_cache_.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.second.usage_frame < rhs.second.usage_frame;
            });
        if (oldest == layout_cache_.end()) {
            break;
        }
        layout_cache_.erase(oldest);
    }
}

} // namespace engine::render
