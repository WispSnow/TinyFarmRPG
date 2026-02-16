#include "font_manager.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>
#include <glad/glad.h>
#include <cmath>
#include <string>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>

namespace engine::resource {

namespace {
// Unicode 替换字符 (U+FFFD)，用于显示无法映射的字符
constexpr char32_t REPLACEMENT_CHAR = U'\uFFFD';
} // namespace

std::unique_ptr<Font> Font::create(entt::id_type id,
                                   int pixel_size,
                                   FT_Face face,
                                   std::string_view source_path) {
    if (!face) {
        spdlog::error("创建 Font 失败：无效的 FT_Face。");
        return nullptr;
    }
    auto font = std::unique_ptr<Font>(new Font(id, pixel_size, face, source_path));
    if (!font->init()) {
        spdlog::error("创建 Font 失败：初始化失败 (id={}, {}px, path='{}')。", id, pixel_size, source_path);
        return nullptr;
    }
    return font;
}

/**
 * @brief Font 构造函数
 * 
 * 字体渲染流程 - 阶段3：字体对象创建
 * 
 * 1. 从 FreeType 的 FT_Face 中提取字体度量信息
 *    - ascender: 字体的上升高度（基线到顶部的距离）
 *    - descender: 字体的下降深度（基线到底部的距离，通常为负值）
 *    - line_height: 行高（用于多行文本的垂直间距）
 * 
 * 2. 创建 HarfBuzz 字体对象
 *    - hb_ft_font_create_referenced 创建一个引用 FT_Face 的 HarfBuzz 字体对象
 *    - 这个对象用于后续的文本整形（shaping）操作
 *    - HarfBuzz 负责处理复杂的文本布局，如连字、字符连接、双向文本等
 * 
 * 注意：FreeType 使用 26.6 定点数格式（1个单位 = 1/64像素），需要除以64.0f转换为浮点数
 */
Font::Font(entt::id_type id, int pixel_size, FT_Face face, std::string_view source_path)
    : id_(id),
      pixel_size_(pixel_size),
      face_(face),
      atlas_page_width_(calculateAtlasDimension(pixel_size)),
      atlas_page_height_(atlas_page_width_),
      source_path_(source_path) {
}

bool Font::init() {
    if (!face_) {
        spdlog::error("Font::init 失败：FT_Face 为空。");
        return false;
    }
    if (!face_->size) {
        spdlog::error("Font::init 失败：FT_Face 缺少 size 信息。");
        return false;
    }
    // FreeType 使用 26.6 定点数格式（26位整数 + 6位小数），需要除以64转换为像素值
    ascender_ = static_cast<float>(face_->size->metrics.ascender) / 64.0f;
    descender_ = static_cast<float>(std::abs(face_->size->metrics.descender)) / 64.0f;
    line_height_ = static_cast<float>(face_->size->metrics.height) / 64.0f;
    // 如果行高无效，使用上升+下降高度作为备用值
    if (line_height_ <= 0.0f) {
        line_height_ = ascender_ + descender_;
    }
    // 创建 HarfBuzz 字体对象，用于文本整形
    // hb_ft_font_create_referenced 会增加 FT_Face 的引用计数，确保在使用期间不会被释放
    hb_font_ = hb_ft_font_create_referenced(face_);
    if (!hb_font_) {
        spdlog::error("Font::init 失败：无法创建 HarfBuzz 字体对象。");
        return false;
    }
    return true;
}

Font::~Font() {
    release();
}

Font::Font(Font&& other) noexcept {
    *this = std::move(other);
}

Font& Font::operator=(Font&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    release();
    id_ = other.id_;
    pixel_size_ = other.pixel_size_;
    ascender_ = other.ascender_;
    descender_ = other.descender_;
    line_height_ = other.line_height_;
    face_ = other.face_;
    hb_font_ = other.hb_font_;
    glyph_cache_ = std::move(other.glyph_cache_);
    atlas_pages_ = std::move(other.atlas_pages_);
    atlas_page_width_ = other.atlas_page_width_;
    atlas_page_height_ = other.atlas_page_height_;
    source_path_ = std::move(other.source_path_);

    other.face_ = nullptr;
    other.hb_font_ = nullptr;
    other.glyph_cache_.clear();
    other.atlas_pages_.clear();
    other.id_ = 0;
    other.pixel_size_ = 0;
    other.ascender_ = 0.0f;
    other.descender_ = 0.0f;
    other.line_height_ = 0.0f;
    other.atlas_page_width_ = 0;
    other.atlas_page_height_ = 0;
    other.source_path_.clear();

    return *this;
}

/**
 * 字体渲染流程 - 阶段4：字形获取与缓存
 * 
 * 流程：
 * 1. 如果字形索引为0（无效），尝试使用替换字符（U+FFFD）或问号（'?'）作为后备
 * 2. 检查字形缓存，如果已存在则直接返回
 * 3. 如果未缓存，调用 loadGlyph 加载字形并缓存
 */
const FontGlyph* Font::getGlyphByIndex(uint32_t glyph_index) {
    // 如果字形索引无效（0），尝试使用替换字符
    if (glyph_index == 0) {
        glyph_index = FT_Get_Char_Index(face_, REPLACEMENT_CHAR);
    }
    // 如果替换字符也不存在，尝试使用问号
    if (glyph_index == 0) {
        glyph_index = FT_Get_Char_Index(face_, '?');
    }

    // 检查字形缓存，避免重复加载相同字形
    if (auto it = glyph_cache_.find(glyph_index); it != glyph_cache_.end()) {
        return &it->second;
    }
    // 缓存未命中，加载字形
    if (!loadGlyph(glyph_index)) {
        return nullptr;
    }
    // 加载成功后返回缓存中的字形
    return &glyph_cache_.find(glyph_index)->second;
}

/**
 * @brief 通过 Unicode 码点获取字形
 * 
 * 字体渲染流程 - 阶段4.1（辅助）：从码点到字形索引的转换
 * 
 * FreeType 使用字符映射表（charmap）将 Unicode 码点转换为字形索引。
 * 这个过程依赖于字体文件中的字符映射表（如 Unicode、Latin-1 等）。
 * 
 * @param codepoint Unicode 码点（例如：'A' = U+0041）
 * @return 字形指针，失败返回 nullptr
 */
const FontGlyph* Font::getGlyphForCodepoint(char32_t codepoint) {
    // 使用 FreeType 将 Unicode 码点转换为字形索引
    uint32_t glyph_index = FT_Get_Char_Index(face_, static_cast<FT_ULong>(codepoint));
    // 如果转换失败且不是替换字符本身，尝试使用替换字符
    if (glyph_index == 0 && codepoint != REPLACEMENT_CHAR) {
        glyph_index = FT_Get_Char_Index(face_, static_cast<FT_ULong>(REPLACEMENT_CHAR));
    }
    // 通过字形索引获取字形（会触发缓存机制）
    return getGlyphByIndex(glyph_index);
}

/**
 * 字体渲染流程 - 阶段4.2（核心）：字形加载与纹理创建
 * 
 * 完整流程：
 * 1. FreeType 加载字形：FT_Load_Glyph 从字体文件中加载字形数据
 *    - FT_LOAD_RENDER 标志会同时渲染字形到位图
 * 
 * 2. 提取字形度量信息：
 *    - size: 位图尺寸（宽度、高度）
 *    - bearing: 字形的偏移量（bitmap_left, bitmap_top），用于定位字形
 *    - advance: 字形的基础步进距离（26.6定点数格式）
 * 
 * 3. 位图到纹理转换：
 *    - FreeType 生成的位图是灰度图（单通道 alpha）
 *    - 需要转换为 RGBA 格式供 OpenGL 使用
 *    - 处理可能的负 pitch（位图从上到下存储）
 * 
 * 4. 创建 OpenGL 纹理：
 *    - 生成纹理对象
 *    - 设置纹理参数（过滤方式、环绕模式）
 *    - 上传像素数据
 * 
 * 5. 缓存字形对象，避免重复加载
 */
bool Font::loadGlyph(uint32_t glyph_index) {
    if (!face_) {
        spdlog::error("Font::loadGlyph 失败：FT_Face 无效。");
        return false;
    }

    // 步骤1：使用 FreeType 加载并渲染字形
    // FT_LOAD_RENDER 标志会同时将字形渲染到位图，避免后续单独调用 FT_Render_Glyph
    FT_Error error = FT_Load_Glyph(face_, glyph_index, FT_LOAD_RENDER);
    if (error != 0) {
        spdlog::warn("Font::loadGlyph 无法加载glyph {} (错误码 {})", glyph_index, error);
        return false;
    }

    // 步骤2：从 FreeType 的 glyph slot 中提取字形信息
    FT_GlyphSlot slot = face_->glyph;
    const FT_Bitmap& bitmap = slot->bitmap;

    FontGlyph glyph{};
    // 字形位图的尺寸（像素）
    glyph.size = {static_cast<int>(bitmap.width), static_cast<int>(bitmap.rows)};
    // bearing 是字形相对于原点的偏移量
    // bitmap_left: 字形左边缘到原点的水平距离
    // bitmap_top: 字形顶部到基线的垂直距离（通常为正，表示在基线上方）
    glyph.bearing = {slot->bitmap_left, slot->bitmap_top};
    // advance 是字形的基础步进距离（26.6定点数，除以64转换为像素）
    glyph.advance = static_cast<float>(slot->advance.x) / 64.0f;

    // 步骤3：如果位图有效，将数据写入字体图集
    if (bitmap.width > 0 && bitmap.rows > 0 && bitmap.buffer) {
        const int width = static_cast<int>(bitmap.width);
        const int height = static_cast<int>(bitmap.rows);
        constexpr int PADDING = 1;

        GlyphAtlasPage* target_page = nullptr;
        glm::ivec2 upload_pos{0};
        if (!tryAllocateRegion(width, height, PADDING, target_page, upload_pos)) {
            // 步骤4：必要时创建图集页并初始化 OpenGL 纹理
            if (!createAtlasPage(width + PADDING * 2, height + PADDING * 2) ||
                !tryAllocateRegion(width, height, PADDING, target_page, upload_pos)) {
                spdlog::error("Font::loadGlyph: 无法为 glyph {} 分配图集空间 ({}x{})", glyph_index, width, height);
                return false;
            }
        }

        const uint8_t* buffer = bitmap.buffer;
        int pitch = bitmap.pitch;
        if (pitch < 0) {
            const std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(pitch) * static_cast<std::ptrdiff_t>(height - 1);
            buffer = bitmap.buffer + offset;
            pitch = -pitch;
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width * height) * 4u, 255u);
        for (int y = 0; y < height; ++y) {
            const uint8_t* src = buffer + static_cast<std::ptrdiff_t>(y) * static_cast<std::ptrdiff_t>(pitch);
            for (int x = 0; x < width; ++x) {
                const uint8_t alpha = src[x];
                const size_t idx = static_cast<size_t>(y * width + x) * 4u;
                rgba[idx + 3] = alpha;
            }
        }

        GLint previous_texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        GLint previous_unpack = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack);

        glBindTexture(GL_TEXTURE_2D, target_page->texture.texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        upload_pos.x, upload_pos.y,
                        width, height,
                        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));

        glyph.texture = target_page->texture.texture;
        const float tex_w = static_cast<float>(target_page->texture.width);
        const float tex_h = static_cast<float>(target_page->texture.height);
        glyph.uv_rect = {
            static_cast<float>(upload_pos.x) / tex_w,
            static_cast<float>(upload_pos.y) / tex_h,
            static_cast<float>(upload_pos.x + width) / tex_w,
            static_cast<float>(upload_pos.y + height) / tex_h
        };
    }

    // 步骤4：将字形对象缓存，避免重复加载
    glyph_cache_.emplace(glyph_index, std::move(glyph));
    return true;
}

void Font::fillDebugInfo(FontDebugInfo& info) const {
    info.id = id_;
    info.pixel_size = pixel_size_;
    info.glyph_count = glyph_cache_.size();
    info.ascender = ascender_;
    info.descender = descender_;
    info.line_height = line_height_;
    info.source = source_path_;
    std::size_t atlas_memory = 0;
    info.atlas_pages.clear();
    info.atlas_pages.reserve(atlas_pages_.size());
    for (const auto& page : atlas_pages_) {
        FontAtlasPageDebugInfo page_info{};
        page_info.texture = page.texture.texture;
        page_info.width = page.texture.width;
        page_info.height = page.texture.height;
        page_info.cursor_x = page.cursor_x;
        page_info.cursor_y = page.cursor_y;
        page_info.current_row_height = page.current_row_height;
        info.atlas_pages.push_back(page_info);
        atlas_memory += static_cast<std::size_t>(page.texture.width) * static_cast<std::size_t>(page.texture.height) * 4u;
    }
    const std::size_t glyph_cache_memory = glyph_cache_.size() * sizeof(FontGlyph);
    info.memory_bytes = atlas_memory + glyph_cache_memory;
}

int Font::calculateAtlasDimension(int pixel_size) {
    if (pixel_size <= 24) {
        return 512;
    }
    if (pixel_size <= 60) {
        return 1024;
    }
    return 2048;
}

bool Font::tryAllocateRegion(int width, int height, int padding,
                             GlyphAtlasPage*& out_page,
                             glm::ivec2& out_position) {
    const int padded_width = width + padding * 2;
    const int padded_height = height + padding * 2;
    for (auto& page : atlas_pages_) {
        if (padded_width > page.texture.width || padded_height > page.texture.height) {
            continue;
        }

        int cursor_x = page.cursor_x;
        int cursor_y = page.cursor_y;
        int row_height = page.current_row_height;

        if (cursor_x + padded_width > page.texture.width) {
            cursor_x = 0;
            cursor_y += row_height;
            row_height = 0;
        }

        if (cursor_y + padded_height > page.texture.height) {
            continue;
        }

        out_page = &page;
        out_position = {cursor_x + padding, cursor_y + padding};

        page.cursor_x = cursor_x + padded_width;
        page.cursor_y = cursor_y;
        page.current_row_height = std::max(row_height, padded_height);
        return true;
    }
    return false;
}

bool Font::createAtlasPage(int min_width, int min_height) {
    const int width = std::max(atlas_page_width_, min_width);
    const int height = std::max(atlas_page_height_, min_height);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        spdlog::error("Font::createAtlasPage: glGenTextures 失败。");
        return false;
    }

    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    GLint previous_unpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack);

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));

    GlyphAtlasPage page{};
    page.texture = engine::utils::GL_Texture(texture, width, height);
    atlas_pages_.push_back(std::move(page));
    spdlog::debug("Font::createAtlasPage: 创建新的 {}x{} 图集页。", width, height);
    return true;
}

void Font::release() {
    for (auto& page : atlas_pages_) {
        if (page.texture.texture != 0) {
            GLuint tex = page.texture.texture;
            glDeleteTextures(1, &tex);
            page.texture.texture = 0;
        }
        page.cursor_x = 0;
        page.cursor_y = 0;
        page.current_row_height = 0;
    }
    atlas_pages_.clear();
    glyph_cache_.clear();
    source_path_.clear();

    if (hb_font_) {
        hb_font_destroy(hb_font_);
        hb_font_ = nullptr;
    }

    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
}

/**
 * @brief FontManager 构造函数
 * 
 * 字体渲染流程 - 阶段1：FreeType 库初始化
 * 
 * FreeType 是一个字体渲染库，提供以下核心功能：
 * - 加载字体文件（TTF、OTF 等）
 * - 解析字体数据（字形、度量信息等）
 * - 渲染字形到位图
 * 
 * FT_Init_FreeType 初始化 FreeType 库，这是所有字体操作的前提。
 * 必须在使用任何 FreeType 功能之前调用。
 */
std::unique_ptr<FontManager> FontManager::create() {
    auto manager = std::unique_ptr<FontManager>(new FontManager());
    if (!manager->init()) {
        return nullptr;
    }
    return manager;
}

bool FontManager::init() {
    // 初始化 FreeType 库，获取 FT_Library 对象
    // FT_Library 是 FreeType 的核心对象，用于管理字体库和资源
    FT_Error error = FT_Init_FreeType(&ft_library_);
    if (error != 0 || !ft_library_) {
        spdlog::error("FontManager 初始化失败: FT_Init_FreeType 失败，错误码 {}", error);
        ft_library_ = nullptr;
        return false;
    }
    spdlog::trace("FontManager 初始化成功。");
    return true;
}

FontManager::~FontManager() {
    if (!fonts_.empty()) {
        spdlog::debug("FontManager 不为空，调用 clearFonts 处理清理逻辑。");
        clearFonts();
    }
    if (ft_library_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }
    spdlog::trace("FontManager 析构成功。");
}

/**
 * @brief 从文件路径加载字体
 * 
 * 字体渲染流程 - 阶段2：字体文件加载
 * 
 * 完整流程：
 * 1. 检查缓存：如果字体已加载，直接返回缓存的字体对象
 * 2. 加载字体文件：使用 FreeType 打开字体文件，创建 FT_Face 对象
 *    - FT_Face 代表一个字体文件中的一个字体样式（face）
 *    - 一个字体文件可能包含多个 face（如粗体、斜体等）
 * 3. 设置字体大小：使用 FT_Set_Pixel_Sizes 设置像素大小
 *    - 这会重新计算字体的度量信息（ascender、descender 等）
 * 4. 创建 Font 对象：封装 FT_Face 和 HarfBuzz 字体对象
 * 5. 缓存字体对象：按 (id, pixel_size) 键值对缓存
 * 
 * @param id 字体唯一标识符
 * @param pixel_size 字体像素大小
 * @param file_path 字体文件路径
 * @return 字体指针，失败返回 nullptr
 */
Font* FontManager::loadFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    if (!ft_library_) {
        spdlog::error("无法加载字体 (id = {})：FontManager 未初始化 FreeType。", id);
        return nullptr;
    }
    if (pixel_size <= 0) {
        spdlog::error("无法加载字体 (id = {})：无效的像素大小 {}。", id, pixel_size);
        return nullptr;
    }
    if (file_path.empty()) {
        spdlog::error("无法加载字体 (id = {})：未提供文件路径。", id);
        return nullptr;
    }

    // 步骤1：检查字体缓存，避免重复加载
    FontKey key = {id, pixel_size};
    if (auto it = fonts_.find(key); it != fonts_.end()) {
        return it->second.get();
    }

    // 步骤2：使用 FreeType 打开字体文件
    // FT_New_Face 从文件路径创建 FT_Face 对象
    // 参数3是 face_index，0 表示使用第一个字体样式
    FT_Face face = nullptr;
    FT_Error error = FT_New_Face(ft_library_, file_path.data(), 0, &face);
    if (error != 0 || !face) {
        spdlog::error("加载字体 '{}' (id = {}, {}px) 失败：错误码 {}。", file_path, id, pixel_size, error);
        return nullptr;
    }

    // 步骤3：设置字体的像素大小
    // FT_Set_Pixel_Sizes 设置字体的水平和垂直像素大小
    // 参数2为宽度，0 表示自动计算（保持宽高比）
    // 参数3为高度，即像素大小
    error = FT_Set_Pixel_Sizes(face, 0, pixel_size);
    if (error != 0) {
        spdlog::error("设置字体 '{}' (id = {}, {}px) 像素大小失败：错误码 {}。", file_path, id, pixel_size, error);
        FT_Done_Face(face);  // 失败时需要释放 FT_Face
        return nullptr;
    }

    // 步骤4：创建 Font 对象（内部会创建 HarfBuzz 字体对象）
    auto font = Font::create(id, pixel_size, face, file_path);
    if (!font) {
        spdlog::error("构造 Font 对象失败：{} (id = {}, {}px)", file_path, id, pixel_size);
        return nullptr;
    }
    Font* font_ptr = font.get();
    // 步骤5：缓存字体对象
    fonts_.emplace(key, std::move(font));
    spdlog::debug("成功加载并缓存字体：{} (id = {}, {}px)", file_path, id, pixel_size);
    return font_ptr;
}

Font* FontManager::loadFont(entt::hashed_string str_hs, int pixel_size) {
    return loadFont(str_hs.value(), pixel_size, str_hs.data());
}

Font* FontManager::getFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    FontKey key = {id, pixel_size};
    if (auto it = fonts_.find(key); it != fonts_.end()) {
        return it->second.get();
    }
    if (file_path.empty()) {
        spdlog::debug("字体 (id = {}, {}px) 不在缓存中，且未提供文件路径，返回 nullptr。", id, pixel_size);
        return nullptr;
    }

    spdlog::info("字体 '{}' (id = {}, {}px) 不在缓存中，尝试加载。", file_path, id, pixel_size);
    return loadFont(id, pixel_size, file_path);
}

Font* FontManager::getFont(entt::hashed_string str_hs, int pixel_size) {
    return getFont(str_hs.value(), pixel_size, str_hs.data());
}

void FontManager::unloadFont(entt::id_type id, int pixel_size) {
    FontKey key = {id, pixel_size};
    if (auto it = fonts_.find(key); it != fonts_.end()) {
        spdlog::debug("卸载字体：{} ({}px)", id, pixel_size);
        fonts_.erase(it);
    } else {
        spdlog::warn("尝试卸载不存在的字体：{} ({}px)", id, pixel_size);
    }
}

void FontManager::clearFonts() {
    if (!fonts_.empty()) {
        spdlog::debug("正在清理所有 {} 个缓存的字体。", fonts_.size());
        fonts_.clear();
    }
}

void FontManager::collectDebugInfo(std::vector<FontDebugInfo>& out) const {
    out.clear();
    out.reserve(fonts_.size());
    for (const auto& [key, font_ptr] : fonts_) {
        if (!font_ptr) {
            continue;
        }
        FontDebugInfo info{};
        font_ptr->fillDebugInfo(info);
        out.push_back(std::move(info));
    }
}

} // namespace engine::resource
