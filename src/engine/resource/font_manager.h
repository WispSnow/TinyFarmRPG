#pragma once
#include <memory>
#include <unordered_map>
#include <utility>
#include <string>
#include <string_view>
#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "engine/utils/defs.h"
#include <ft2build.h>
#include FT_FREETYPE_H    // FreeType
#include <hb-ft.h>        // HarfBuzz-FreeType 桥接，用于将FreeType与HarfBuzz结合
#include <vector>
#include "resource_debug_info.h"

namespace engine::resource {

// 定义字体键类型（路径 + 大小）
using FontKey = std::pair<entt::id_type, int>;

/**
 * @brief FontKey 的自定义哈希函数，适用于 std::unordered_map。
 *        使用标准库推荐的哈希合并方式，避免简单异或带来的哈希冲突。
 */
struct FontKeyHash {
    std::size_t operator()(const FontKey& key) const noexcept {
        // 采用C++20标准库的hash_combine实现思路
        std::size_t h1 = std::hash<entt::id_type>{}(key.first);
        std::size_t h2 = std::hash<int>{}(key.second);
        // 推荐的哈希合并方式，参考boost::hash_combine
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

/**
 * @brief 字形结构体，用于存储字形纹理、大小、偏移和步进距离。
 */
struct FontGlyph {
    GLuint texture{0};                          ///< @brief 所在纹理页句柄
    glm::ivec2 size{0};                         ///< @brief 字形大小
    glm::ivec2 bearing{0};                      ///< @brief 字形偏移
    float advance{0.0f};                        ///< @brief 字形步进距离
    glm::vec4 uv_rect{0.0f};                    ///< @brief 字形在纹理页中的UV范围 [u0, v0, u1, v1]
};

/**
 * @brief 字体类，用于管理字体资源。
 * 
 * 包含字体度量信息、字形缓存、HarfBuzz 字体对象等。
 * 请通过 FontManager 创建实例，失败返回 nullptr。仅供 FontManager 内部使用。
 */
class Font {
    friend class FontManager;
private:
    entt::id_type id_{};
    int pixel_size_{0};
    float ascender_{0.0f};
    float descender_{0.0f};
    float line_height_{0.0f};
    FT_Face face_{nullptr};         ///< @brief FreeType 字体对象
    hb_font_t* hb_font_{nullptr};   ///< @brief HarfBuzz 字体对象
    std::unordered_map<uint32_t, FontGlyph> glyph_cache_;    ///< @brief 字形缓存
    struct GlyphAtlasPage {
        engine::utils::GL_Texture texture{};
        int cursor_x{0};
        int cursor_y{0};
        int current_row_height{0};
    };
    std::vector<GlyphAtlasPage> atlas_pages_;   ///< @brief 字形图集页
    int atlas_page_width_{0};
    int atlas_page_height_{0};
    std::string source_path_;

public:
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    /**
     * @brief 通过字形索引获取字形
     * @param glyph_index FreeType 字形索引（由 HarfBuzz 整形后提供）
     * @return 字形指针，失败返回 nullptr
     */
    [[nodiscard]] const FontGlyph* getGlyphByIndex(uint32_t glyph_index);

    /**
     * @brief 通过 Unicode 码点获取字形
     * @param codepoint Unicode 码点
     * @return 字形指针，失败返回 nullptr
     */
    [[nodiscard]] const FontGlyph* getGlyphForCodepoint(char32_t codepoint);

    [[nodiscard]] hb_font_t* getHBFont() const { return hb_font_; }     ///< @brief 获取 HarfBuzz 字体对象
    [[nodiscard]] float getLineHeight() const { return line_height_; }  ///< @brief 获取行高
    [[nodiscard]] float getAscender() const { return ascender_; }       ///< @brief 获取上升高度
    [[nodiscard]] float getDescender() const { return descender_; }     ///< @brief 获取下降深度
    [[nodiscard]] int getPixelSize() const { return pixel_size_; }      ///< @brief 获取像素大小
    [[nodiscard]] entt::id_type getId() const { return id_; }           ///< @brief 获取字体唯一标识符

private:
    [[nodiscard]] static std::unique_ptr<Font> create(entt::id_type id,
                                                      int pixel_size,
                                                      FT_Face face,
                                                      std::string_view source_path);
    Font(entt::id_type id, int pixel_size, FT_Face face, std::string_view source_path);
    [[nodiscard]] bool init();

    /**
     * @brief 加载字形
     * @param glyph_index FreeType 字形索引
     * @return 成功返回 true，失败返回 false
     */
    bool loadGlyph(uint32_t glyph_index);
    [[nodiscard]] static int calculateAtlasDimension(int pixel_size);
    bool tryAllocateRegion(int width, int height, int padding,
                           GlyphAtlasPage*& out_page,
                           glm::ivec2& out_position);
    bool createAtlasPage(int min_width, int min_height);
    void release();
    void fillDebugInfo(FontDebugInfo& info) const;
};

/**
 * @brief 管理 FreeType 字体资源（Font）。
 *
 * 提供字体的加载和缓存功能，通过文件路径和像素大小来标识。
 * 请通过 create() 创建实例，失败返回 nullptr。仅供 ResourceManager 内部使用。
 */
class FontManager final{
    friend class ResourceManager;

private:
    FT_Library ft_library_{nullptr};    ///< @brief FreeType 库对象
    std::unordered_map<FontKey, std::unique_ptr<Font>, FontKeyHash> fonts_;    ///< @brief 字体缓存

public:
    [[nodiscard]] static std::unique_ptr<FontManager> create();
    
    ~FontManager();            ///< @brief 析构函数，清理资源并关闭 FreeType。

    // 当前设计中，我们只需要一个FontManager，所有权不变，所以不需要拷贝、移动相关构造及赋值运算符
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

private: // 仅由 ResourceManager（和内部）访问的方法
    FontManager() = default;
    [[nodiscard]] bool init();

    /**
     * @brief 从文件路径加载指定像素大小的字体
     * @param id 字体的唯一标识符, 通过entt::hashed_string生成
     * @param pixel_size 字体的像素大小
     * @param file_path 字体文件的路径
     * @return 加载的字体的指针
     * @note 如果字体已经加载，则返回已加载字体的指针
     * @note 如果字体未加载，则从文件路径加载字体，并返回加载的字体的指针
     */
    Font* loadFont(entt::id_type id, int pixel_size, std::string_view file_path);

    /**
     * @brief 从字符串哈希值加载指定像素大小的字体
     * @param str_hs entt::hashed_string类型
     * @param pixel_size 字体的像素大小
     * @return 加载的字体的指针
     * @note 如果字体已经加载，则返回已加载字体的指针
     * @note 如果字体未加载，则从哈希字符串对应的文件路径加载字体，并返回加载的字体的指针
     */
    Font* loadFont(entt::hashed_string str_hs, int pixel_size);

    /**
     * @brief 尝试获取已加载字体的指针，如果未加载则尝试加载
     * @param id 字体的唯一标识符, 通过entt::hashed_string生成
     * @param pixel_size 字体的像素大小
     * @param file_path 字体文件的路径
     * @return 加载的字体的指针
     * @note 如果字体已经加载，则返回已加载字体的指针
     * @note 如果字体未加载，且提供了file_path，则尝试从文件路径加载字体，并返回加载的字体的指针
     */
    Font* getFont(entt::id_type id, int pixel_size, std::string_view file_path = "");

    /**
     * @brief 从字符串哈希值获取字体
     * @param str_hs entt::hashed_string类型
     * @param pixel_size 字体的像素大小
     * @return 加载的字体的指针
     * @note 如果字体已经加载，则返回已加载字体的指针
     * @note 如果字体未加载，则从哈希字符串对应的文件路径加载字体，并返回加载的字体的指针
     */
    Font* getFont(entt::hashed_string str_hs, int pixel_size);

    /**
     * @brief 卸载特定字体（通过路径哈希值和大小标识）
     * @param id 字体的唯一标识符, 通过entt::hashed_string生成
     * @param pixel_size 字体的像素大小
     */
    void unloadFont(entt::id_type id, int pixel_size);
    
    /**
     * @brief 清空所有缓存的字体
     */
    void clearFonts();

    /**
     * @brief 收集所有字体的调试信息
     * @param out 输出调试信息容器
     */
    void collectDebugInfo(std::vector<FontDebugInfo>& out) const;   
};

} // namespace engine::resource
