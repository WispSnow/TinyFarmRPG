#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "engine/utils/defs.h"
#include <hb.h>
#include <unordered_map>
#include <functional>
#include <entt/signal/fwd.hpp>
#include <nlohmann/json_fwd.hpp>
#include "engine/utils/events.h"

namespace engine::resource {
    class ResourceManager;
    class Font;
    struct FontGlyph;
}

namespace engine::render::opengl {
    class GLRenderer;
}

namespace engine::render {
class TextRenderer final {
public:
    static constexpr std::string_view DEFAULT_CONFIG_PATH{"config/text_render.json"};

    /**
     * @brief 文本方向，用于 HarfBuzz 排版。
     */
    enum class TextDirection {
        LeftToRight,
        RightToLeft,
        TopToBottom,
        BottomToTop
    };

    /**
     * @brief 字形放置信息
     */
    struct GlyphPlacement {
        const engine::resource::FontGlyph* glyph{nullptr};
        glm::vec4 dest_rect{0.0f, 0.0f, 0.0f, 0.0f};
    };

    /**
     * @brief 文本布局信息
     */
    struct TextLayout {
        engine::resource::Font* font{nullptr};
        glm::vec2 size{0.0f, 0.0f};
        std::vector<GlyphPlacement> glyphs;
        int line_count{0};
        uint64_t usage_frame{0};
    };

    /**
     * @brief 文本布局键
     */
    struct LayoutKey {
        entt::id_type font_id{};
        int font_size{};
        std::string text;
        engine::utils::LayoutOptions layout_options{};
        bool operator==(const LayoutKey& other) const noexcept {
            return font_id == other.font_id &&
                   font_size == other.font_size &&
                   text == other.text &&
                   layout_options == other.layout_options;
        }
    };

    /**
     * @brief 文本布局键哈希函数
     */
    struct LayoutKeyHash {
        std::size_t operator()(const LayoutKey& key) const noexcept {
            std::size_t h1 = std::hash<entt::id_type>{}(key.font_id);
            std::size_t h2 = std::hash<int>{}(key.font_size);
            std::size_t h3 = std::hash<std::string>{}(key.text);
            std::size_t h4 = std::hash<float>{}(key.layout_options.letter_spacing);
            std::size_t h5 = std::hash<float>{}(key.layout_options.line_spacing_scale);
            std::size_t h6 = std::hash<float>{}(key.layout_options.glyph_scale.x);
            std::size_t h7 = std::hash<float>{}(key.layout_options.glyph_scale.y);
            std::size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h5 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h6 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h7 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

private:
    struct TextStyleEntry {
        std::string key{};
        engine::utils::TextRenderParams params{};
    };

    engine::render::opengl::GLRenderer* gl_renderer_{nullptr};     ///< @brief 持有OpenGL渲染器的非拥有指针
    engine::resource::ResourceManager* resource_manager_{nullptr}; ///< @brief 持有资源管理器的非拥有指针
    entt::dispatcher* dispatcher_{nullptr};                        ///< @brief 事件分发器

    TextDirection default_direction_{TextDirection::LeftToRight};
    std::string default_language_tag_{"zh-Hans"};           ///< @brief 默认语言标签（BCP-47，例如 "en"、"zh-Hans"）
    std::vector<hb_feature_t> active_features_;             ///< @brief 激活的 HarfBuzz 特性

    mutable uint64_t layout_frame_counter_{0};    ///< @brief 文本布局帧计数器
    mutable std::unordered_map<LayoutKey, TextLayout, LayoutKeyHash> layout_cache_;    ///< @brief 文本布局缓存
    std::size_t layout_cache_capacity_{100};    ///< @brief 文本布局缓存容量

    std::unordered_map<entt::id_type, TextStyleEntry> text_styles_{};
    entt::id_type default_ui_style_id_{entt::null};
    entt::id_type default_world_style_id_{entt::null};
    std::uint64_t layout_revision_{0};

public:

    /**
     * @brief 创建并初始化 TextRenderer。
     *
     * @param gl_renderer 有效的 GLRenderer 指针（不能为空）。
     * @param resource_manager 有效的 ResourceManager 指针（不能为空，用于字体加载）。
     * @param dispatcher 有效的事件分发器指针（不能为空）。
     * @return 创建成功返回实例；失败返回 nullptr。
     */
    [[nodiscard]] static std::unique_ptr<TextRenderer> create(engine::render::opengl::GLRenderer* gl_renderer,
                                                              engine::resource::ResourceManager* resource_manager,
                                                              entt::dispatcher* dispatcher);
    ~TextRenderer();

    [[nodiscard]] bool loadConfig(std::string_view config_path);

    /**
     * @brief 绘制UI上的字符串。
     *        
     * @param text UTF-8 字符串内容。
     * @param font_id 字体 ID。
     * @param font_size 字体大小。
     * @param position 左上角屏幕位置。
     * @param color 文本颜色。(默认为白色)
     */
    void drawUIText(std::string_view text,
                    entt::id_type font_id,
                    int font_size,
                    const glm::vec2& position,
                    entt::id_type style_id,
                    const engine::utils::TextRenderOverrides* overrides = nullptr) const;

    void drawUIText(std::string_view text,
                    entt::id_type font_id,
                    int font_size,
                    const glm::vec2& position,
                    std::string_view style_key = {},
                    const engine::utils::TextRenderOverrides* overrides = nullptr) const;

    /**
     * @brief 绘制场景中的字符串（受相机投影影响）。
     *
     * @param text UTF-8 字符串内容。
     * @param font_id 字体 ID。
     * @param font_size 字体大小。
     * @param position 世界空间中的左上角位置。
     * @param color 文本颜色。
     */
    void drawText(std::string_view text,
                  entt::id_type font_id,
                  int font_size,
                  const glm::vec2& position,
                  entt::id_type style_id,
                  const engine::utils::TextRenderOverrides* overrides = nullptr) const;

    void drawText(std::string_view text,
                  entt::id_type font_id,
                  int font_size,
                  const glm::vec2& position,
                  std::string_view style_key = {},
                  const engine::utils::TextRenderOverrides* overrides = nullptr) const;

    /**
     * @brief 获取文本的尺寸。
     *
     * @param text 要测量的文本。
     * @param font_id 字体 ID。
     * @param font_size 字体大小。
     * @param font_path 字体路径（可选，用于首次加载）
     * @return 文本的尺寸。
     */
    [[nodiscard]] glm::vec2 getTextSize(std::string_view text,
                                        entt::id_type font_id,
                                        int font_size,
                                        std::string_view font_path = "",
                                        const engine::utils::LayoutOptions* layout_options = nullptr) const;

    /**
     * @brief 设置默认文字方向（影响 HarfBuzz 排版）。
     */
    void setDefaultDirection(TextDirection direction);

    /**
     * @brief 设置默认语言标签（BCP-47，例如 "en"、"zh"）。
     */
    void setDefaultLanguage(std::string_view language_tag);

    /**
     * @brief 设置 HarfBuzz 字体特性（例如 {"kern=0", "liga=1"}）。
     *        如果传入空列表，则使用 HarfBuzz 默认特性。
     */
    void setFeatures(const std::vector<std::string>& feature_specs);
    void clearLayoutCache();
    void setLayoutCacheCapacity(std::size_t capacity);
    [[nodiscard]] std::size_t getLayoutCacheCapacity() const { return layout_cache_capacity_; }

    [[nodiscard]] entt::id_type getDefaultUIStyleId() const { return default_ui_style_id_; }
    [[nodiscard]] entt::id_type getDefaultWorldStyleId() const { return default_world_style_id_; }
    [[nodiscard]] std::string_view getDefaultUIStyleKey() const;
    [[nodiscard]] std::string_view getDefaultWorldStyleKey() const;
    [[nodiscard]] std::uint64_t getLayoutRevision() const { return layout_revision_; }

    [[nodiscard]] bool hasTextStyle(entt::id_type style_id) const;
    [[nodiscard]] bool hasTextStyle(std::string_view key) const;
    [[nodiscard]] std::string_view getTextStyleKey(entt::id_type style_id) const;
    [[nodiscard]] const engine::utils::TextRenderParams& getTextStyle(entt::id_type style_id) const;
    [[nodiscard]] const engine::utils::TextRenderParams& getTextStyle(std::string_view key) const;
    bool setTextStyle(std::string_view key, const engine::utils::TextRenderParams& params);
    [[nodiscard]] std::vector<std::string> listTextStyleKeys(std::string_view prefix = {}) const;

    // 禁用拷贝和移动语义
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&) = delete;
    TextRenderer& operator=(TextRenderer&&) = delete;

private:
    TextRenderer(engine::render::opengl::GLRenderer* gl_renderer,
                 engine::resource::ResourceManager* resource_manager,
                 entt::dispatcher* dispatcher);

    [[nodiscard]] hb_direction_t toHbDirection(TextDirection direction) const;

    [[nodiscard]] const TextLayout* buildLayout(std::string_view text,
                                                entt::id_type font_id,
                                                int font_size,
                                                const engine::utils::LayoutOptions& layout_options,
                                                std::string_view font_path = "") const;

    /// @brief 对单行文本执行 HarfBuzz 整形并生成字形放置信息。
    /// @return 该行的像素宽度。
    [[nodiscard]] float shapeLine(std::string_view line_text,
                                  engine::resource::Font* font,
                                  const engine::utils::LayoutOptions& options,
                                  float pen_y_origin,
                                  std::vector<GlyphPlacement>& out_glyphs) const;
    void trimLayoutCache() const;
    void onFontUnloaded(const engine::utils::FontUnloadedEvent& event);
    void onFontsCleared(const engine::utils::FontsClearedEvent& event);

    void drawTextInternal(std::string_view text,
                          entt::id_type font_id,
                          int font_size,
                          const glm::vec2& position,
                          const engine::utils::TextRenderParams& params,
                          bool use_ui_pass) const;

    [[nodiscard]] static entt::id_type toTextStyleId(std::string_view key);

    /// @brief 从 JSON 配置中解析文本样式（styles + default_style_keys），更新 text_styles_ 及默认样式 ID。
    /// @return 是否有影响布局的变更。
    bool loadConfigStyles(const nlohmann::json& config);

    void ensureBuiltinStyles();
    [[nodiscard]] const engine::utils::TextRenderParams& getStyleOrFallback(entt::id_type style_id,
                                                                            entt::id_type fallback_id) const;
    [[nodiscard]] engine::utils::TextRenderParams resolveTextParams(entt::id_type style_id,
                                                                    entt::id_type fallback_id,
                                                                    const engine::utils::TextRenderOverrides* overrides) const;

}; // class TextRenderer

} // namespace engine::render
