#include "engine/platform/gl_platform.h"
#include "engine/platform/filesystem_paths.h"
#include "engine/platform/web_persistent_storage.h"
#include "web_shell_ui.h"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr std::string_view kMapPath = "/assets/maps/home_exterior.tmj";
constexpr std::string_view kPersistenceSmokePath = "/persistent/saves/web_persistence_smoke.json";

constexpr const char* kTileVertexShader = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

uniform vec2 u_canvas_size;
uniform vec2 u_origin_pixels;
uniform float u_scale;

out vec2 v_uv;

void main() {
    vec2 screen_position = a_position * u_scale + u_origin_pixels;
    vec2 ndc = vec2(
        (screen_position.x / u_canvas_size.x) * 2.0 - 1.0,
        1.0 - (screen_position.y / u_canvas_size.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
}
)";

constexpr const char* kTileFragmentShader = R"(#version 300 es
precision mediump float;

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;

void main() {
    frag_color = texture(u_tex, v_uv);
}
)";

struct TileVertex {
    float x{};
    float y{};
    float u{};
    float v{};
};

struct DrawBatch {
    GLuint texture{};
    GLsizei first_vertex{};
    GLsizei vertex_count{};
};

enum class StartupStage {
    WaitingPersistentSync,
    ReadyToInitialize,
    Failed,
    Running,
};

struct TileLayer {
    std::string name;
    int width{};
    int height{};
    std::vector<std::uint32_t> gids;
    bool visible{true};
};

struct Tileset {
    std::uint32_t first_gid{};
    std::uint32_t tile_count{};
    int columns{};
    int tile_width{};
    int tile_height{};
    int image_width{};
    int image_height{};
    std::string path;
    std::string image_path;
    GLuint texture{};
};

struct TileMap {
    int width{};
    int height{};
    int tile_width{};
    int tile_height{};
    std::vector<TileLayer> layers;
    std::vector<Tileset> tilesets;
};

struct WebApp {
    SDL_Window* window{};
    SDL_GLContext gl_context{};
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint canvas_size_uniform{-1};
    GLint origin_uniform{-1};
    GLint scale_uniform{-1};
    GLint texture_uniform{-1};
    int map_width_pixels{};
    int map_height_pixels{};
    std::vector<Tileset> tilesets;
    std::vector<DrawBatch> batches;
    StartupStage startup_stage{StartupStage::WaitingPersistentSync};
    bool running{true};
};

void logGlInfo() {
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::printf("TinyFarmRPG WebGL vendor: %s\n", vendor ? vendor : "(unknown)");
    std::printf("TinyFarmRPG WebGL renderer: %s\n", renderer ? renderer : "(unknown)");
    std::printf("TinyFarmRPG GL version: %s\n", version ? version : "(unknown)");
}

std::string readFileText(std::string_view path) {
    const std::string path_string{path};
    FILE* file = std::fopen(path_string.c_str(), "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "Failed to open file: %s\n", path_string.c_str());
        return {};
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return {};
    }
    std::fseek(file, 0, SEEK_SET);

    std::string text(static_cast<std::size_t>(size), '\0');
    const std::size_t bytes_read = std::fread(text.data(), 1, text.size(), file);
    std::fclose(file);
    text.resize(bytes_read);
    return text;
}

std::optional<std::uint32_t> parseUintAfter(std::string_view text,
                                            std::string_view key,
                                            std::size_t start = 0) {
    const std::size_t key_pos = text.find(key, start);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t value_pos = text.find_first_of("0123456789", key_pos + key.size());
    if (value_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::uint32_t value = 0;
    const char* begin = text.data() + value_pos;
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

std::uint32_t readPersistenceSmokeCounter() {
    std::error_code ec{};
    if (!std::filesystem::exists(std::filesystem::path{kPersistenceSmokePath}, ec) || ec) {
        return 0;
    }

    const std::string text = readFileText(kPersistenceSmokePath);
    if (text.empty()) {
        return 0;
    }
    return parseUintAfter(text, "\"boot_count\":").value_or(0);
}

bool writePersistenceSmokeCounter(std::uint32_t boot_count) {
    const std::filesystem::path smoke_path{kPersistenceSmokePath};
    if (const auto dir = smoke_path.parent_path(); !dir.empty()) {
        std::error_code ec{};
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            std::fprintf(stderr, "Failed to create persistent save directory: %s\n", ec.message().c_str());
            return false;
        }
    }

    std::ofstream file(smoke_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::fprintf(stderr, "Failed to write persistent smoke file: %s\n", smoke_path.string().c_str());
        return false;
    }
    file << "{\n"
         << "  \"schema\": 1,\n"
         << "  \"boot_count\": " << boot_count << ",\n"
         << "  \"note\": \"TinyFarmRPG Web IDBFS smoke\"\n"
         << "}\n";
    file.flush();
    return file.good();
}

std::optional<bool> parseBoolAfter(std::string_view text,
                                   std::string_view key,
                                   std::size_t start = 0) {
    const std::size_t key_pos = text.find(key, start);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t value_pos = text.find_first_not_of(" \t\r\n:", key_pos + key.size());
    if (value_pos == std::string_view::npos) {
        return std::nullopt;
    }
    if (text.substr(value_pos, 4) == "true") {
        return true;
    }
    if (text.substr(value_pos, 5) == "false") {
        return false;
    }
    return std::nullopt;
}

std::string unescapeJsonString(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            output.push_back(value[i + 1]);
            ++i;
            continue;
        }
        output.push_back(value[i]);
    }
    return output;
}

std::optional<std::string> parseStringAfter(std::string_view text,
                                            std::string_view key,
                                            std::size_t start = 0) {
    const std::size_t value_begin = text.find(key, start);
    if (value_begin == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t cursor = value_begin + key.size();
    std::string raw_value;
    while (cursor < text.size()) {
        const char ch = text[cursor];
        if (ch == '\\' && cursor + 1 < text.size()) {
            raw_value.push_back(ch);
            raw_value.push_back(text[cursor + 1]);
            cursor += 2;
            continue;
        }
        if (ch == '"') {
            return unescapeJsonString(raw_value);
        }
        raw_value.push_back(ch);
        ++cursor;
    }
    return std::nullopt;
}

std::vector<std::uint32_t> parseUintArrayAfter(std::string_view text,
                                               std::string_view key,
                                               std::size_t start,
                                               std::size_t& array_end) {
    std::vector<std::uint32_t> values;
    array_end = std::string_view::npos;

    const std::size_t key_pos = text.find(key, start);
    if (key_pos == std::string_view::npos) {
        return values;
    }
    const std::size_t begin = text.find('[', key_pos + key.size());
    if (begin == std::string_view::npos) {
        return values;
    }
    const std::size_t end = text.find(']', begin + 1);
    if (end == std::string_view::npos) {
        return values;
    }

    std::size_t cursor = begin + 1;
    while (cursor < end) {
        const std::size_t value_pos = text.find_first_of("0123456789", cursor);
        if (value_pos == std::string_view::npos || value_pos >= end) {
            break;
        }

        std::uint32_t value = 0;
        const char* parse_begin = text.data() + value_pos;
        const char* parse_end = text.data() + end;
        const auto result = std::from_chars(parse_begin, parse_end, value);
        if (result.ec != std::errc{}) {
            break;
        }
        values.push_back(value);
        cursor = static_cast<std::size_t>(result.ptr - text.data());
    }

    array_end = end + 1;
    return values;
}

std::string directoryOf(std::string_view path) {
    const std::size_t slash = path.rfind('/');
    if (slash == std::string_view::npos) {
        return {};
    }
    return std::string{path.substr(0, slash)};
}

std::string normalizePath(std::string_view base_dir, std::string_view relative_path) {
    std::string combined;
    if (!relative_path.empty() && relative_path.front() == '/') {
        combined = std::string{relative_path};
    } else if (base_dir.empty()) {
        combined = std::string{relative_path};
    } else {
        combined = std::string{base_dir} + "/" + std::string{relative_path};
    }

    const bool absolute = !combined.empty() && combined.front() == '/';
    std::vector<std::string> parts;
    std::size_t cursor = 0;
    while (cursor <= combined.size()) {
        const std::size_t slash = combined.find('/', cursor);
        const std::size_t end = slash == std::string::npos ? combined.size() : slash;
        const std::string_view part{combined.data() + cursor, end - cursor};
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.emplace_back(part);
        }
        if (slash == std::string::npos) {
            break;
        }
        cursor = slash + 1;
    }

    std::string normalized = absolute ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            normalized += "/";
        }
        normalized += parts[i];
    }
    return normalized;
}

bool loadTilesetMetadata(Tileset& tileset) {
    const std::string text = readFileText(tileset.path);
    if (text.empty()) {
        return false;
    }

    const auto image = parseStringAfter(text, "\"image\":\"");
    const auto image_width = parseUintAfter(text, "\"imagewidth\":");
    const auto image_height = parseUintAfter(text, "\"imageheight\":");
    const auto tile_width = parseUintAfter(text, "\"tilewidth\":");
    const auto tile_height = parseUintAfter(text, "\"tileheight\":");
    const auto columns = parseUintAfter(text, "\"columns\":");
    const auto tile_count = parseUintAfter(text, "\"tilecount\":");
    if (!image || !image_width || !image_height || !tile_width || !tile_height) {
        std::printf("tileset skipped (no atlas image): %s\n", tileset.path.c_str());
        return false;
    }

    tileset.image_width = static_cast<int>(*image_width);
    tileset.image_height = static_cast<int>(*image_height);
    tileset.tile_width = static_cast<int>(*tile_width);
    tileset.tile_height = static_cast<int>(*tile_height);
    tileset.columns = columns ? static_cast<int>(*columns) : tileset.image_width / tileset.tile_width;
    tileset.tile_count = tile_count ? *tile_count : static_cast<std::uint32_t>(
        (tileset.image_width / tileset.tile_width) * (tileset.image_height / tileset.tile_height));
    tileset.image_path = normalizePath(directoryOf(tileset.path), *image);
    return tileset.columns > 0 && tileset.tile_count > 0;
}

bool loadTexture(Tileset& tileset) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(tileset.image_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        std::fprintf(stderr, "Failed to decode tileset image: %s (%s)\n",
                     tileset.image_path.c_str(),
                     stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        return false;
    }

    glGenTextures(1, &tileset.texture);
    glBindTexture(GL_TEXTURE_2D, tileset.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        engine::platform::gl::kTextureColorInternalFormat,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    if (tileset.texture == 0 || glGetError() != GL_NO_ERROR) {
        std::fprintf(stderr, "Failed to upload tileset texture: %s\n", tileset.image_path.c_str());
        return false;
    }
    return true;
}

std::vector<TileLayer> parseLayers(std::string_view map_text) {
    std::vector<TileLayer> layers;
    const std::size_t layers_begin = map_text.find("\"layers\"");
    const std::size_t tilesets_begin = map_text.find("\"tilesets\"", layers_begin);
    if (layers_begin == std::string_view::npos || tilesets_begin == std::string_view::npos) {
        return layers;
    }

    std::size_t cursor = layers_begin;
    while (cursor < tilesets_begin) {
        const std::size_t data_pos = map_text.find("\"data\"", cursor);
        if (data_pos == std::string_view::npos || data_pos >= tilesets_begin) {
            break;
        }

        std::size_t data_end = std::string_view::npos;
        TileLayer layer{};
        layer.gids = parseUintArrayAfter(map_text, "\"data\"", data_pos, data_end);
        if (layer.gids.empty() || data_end == std::string_view::npos) {
            cursor = data_pos + 6;
            continue;
        }

        const std::size_t name_pos = map_text.rfind("\"name\":\"", data_pos);
        if (name_pos != std::string_view::npos) {
            layer.name = parseStringAfter(map_text, "\"name\":\"", name_pos).value_or("tilelayer");
        }
        layer.width = static_cast<int>(parseUintAfter(map_text, "\"width\":", data_end).value_or(0));
        layer.height = static_cast<int>(parseUintAfter(map_text, "\"height\":", data_end).value_or(0));
        layer.visible = parseBoolAfter(map_text, "\"visible\":", data_end).value_or(true);

        if (layer.width > 0 && layer.height > 0 && layer.visible) {
            layers.push_back(std::move(layer));
        }
        cursor = data_end;
    }
    return layers;
}

std::vector<Tileset> parseTilesets(std::string_view map_text) {
    std::vector<Tileset> tilesets;
    const std::size_t tilesets_begin = map_text.find("\"tilesets\"");
    if (tilesets_begin == std::string_view::npos) {
        return tilesets;
    }

    const std::string map_dir = directoryOf(kMapPath);
    std::size_t cursor = tilesets_begin;
    while (true) {
        const std::size_t first_gid_pos = map_text.find("\"firstgid\"", cursor);
        if (first_gid_pos == std::string_view::npos) {
            break;
        }
        const auto first_gid = parseUintAfter(map_text, "\"firstgid\":", first_gid_pos);
        const auto source = parseStringAfter(map_text, "\"source\":\"", first_gid_pos);
        if (!first_gid || !source) {
            break;
        }

        Tileset tileset{};
        tileset.first_gid = *first_gid;
        tileset.path = normalizePath(map_dir, *source);
        if (loadTilesetMetadata(tileset) && loadTexture(tileset)) {
            tilesets.push_back(std::move(tileset));
        }
        cursor = first_gid_pos + 10;
    }

    std::sort(tilesets.begin(), tilesets.end(), [](const Tileset& lhs, const Tileset& rhs) {
        return lhs.first_gid < rhs.first_gid;
    });
    return tilesets;
}

TileMap loadTileMap(std::string_view path) {
    TileMap map{};
    const std::string text = readFileText(path);
    if (text.empty()) {
        return map;
    }

    map.width = static_cast<int>(parseUintAfter(text, "\"width\":").value_or(0));
    map.height = static_cast<int>(parseUintAfter(text, "\"height\":").value_or(0));
    map.tile_width = static_cast<int>(parseUintAfter(text, "\"tilewidth\":").value_or(16));
    map.tile_height = static_cast<int>(parseUintAfter(text, "\"tileheight\":").value_or(16));
    map.layers = parseLayers(text);
    map.tilesets = parseTilesets(text);
    return map;
}

const Tileset* findTilesetForGid(const std::vector<Tileset>& tilesets, std::uint32_t gid) {
    const Tileset* selected = nullptr;
    for (const Tileset& tileset : tilesets) {
        if (tileset.first_gid <= gid) {
            selected = &tileset;
        } else {
            break;
        }
    }
    if (selected == nullptr) {
        return nullptr;
    }

    const std::uint32_t local_id = gid - selected->first_gid;
    return local_id < selected->tile_count ? selected : nullptr;
}

void appendBatchVertex(std::vector<TileVertex>& vertices,
                       std::vector<DrawBatch>& batches,
                       GLuint texture,
                       const TileVertex& vertex) {
    if (batches.empty() || batches.back().texture != texture) {
        batches.push_back(DrawBatch{
            texture,
            static_cast<GLsizei>(vertices.size()),
            0,
        });
    }
    vertices.push_back(vertex);
    ++batches.back().vertex_count;
}

void appendTile(std::vector<TileVertex>& vertices,
                std::vector<DrawBatch>& batches,
                const Tileset& tileset,
                std::uint32_t gid,
                int tile_x,
                int tile_y,
                int map_tile_width,
                int map_tile_height) {
    const std::uint32_t local_id = gid - tileset.first_gid;
    const int atlas_x = static_cast<int>(local_id % static_cast<std::uint32_t>(tileset.columns)) * tileset.tile_width;
    const int atlas_y = static_cast<int>(local_id / static_cast<std::uint32_t>(tileset.columns)) * tileset.tile_height;
    const float inv_w = 1.0F / static_cast<float>(tileset.image_width);
    const float inv_h = 1.0F / static_cast<float>(tileset.image_height);

    const float u0 = static_cast<float>(atlas_x) * inv_w;
    const float v0 = static_cast<float>(atlas_y) * inv_h;
    const float u1 = static_cast<float>(atlas_x + tileset.tile_width) * inv_w;
    const float v1 = static_cast<float>(atlas_y + tileset.tile_height) * inv_h;

    const float x0 = static_cast<float>(tile_x * map_tile_width);
    const float y0 = static_cast<float>(tile_y * map_tile_height);
    const float x1 = x0 + static_cast<float>(map_tile_width);
    const float y1 = y0 + static_cast<float>(map_tile_height);

    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x0, y0, u0, v0});
    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x1, y0, u1, v0});
    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x0, y1, u0, v1});
    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x0, y1, u0, v1});
    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x1, y0, u1, v0});
    appendBatchVertex(vertices, batches, tileset.texture, TileVertex{x1, y1, u1, v1});
}

bool buildTileGeometry(WebApp& app, const TileMap& map, const std::vector<Tileset>& tilesets) {
    std::vector<TileVertex> vertices;
    std::vector<DrawBatch> batches;

    for (const TileLayer& layer : map.layers) {
        const int width = layer.width > 0 ? layer.width : map.width;
        const int height = layer.height > 0 ? layer.height : map.height;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t index = static_cast<std::size_t>(y * width + x);
                if (index >= layer.gids.size()) {
                    continue;
                }

                constexpr std::uint32_t tiled_gid_mask = 0x1FFFFFFFu;
                const std::uint32_t gid = layer.gids[index] & tiled_gid_mask;
                if (gid == 0) {
                    continue;
                }

                const Tileset* tileset = findTilesetForGid(tilesets, gid);
                if (tileset == nullptr || tileset->texture == 0) {
                    continue;
                }
                appendTile(vertices, batches, *tileset, gid, x, y, map.tile_width, map.tile_height);
            }
        }
    }

    if (vertices.empty()) {
        std::fprintf(stderr, "No renderable WebGL tile vertices were generated.\n");
        return false;
    }

    app.batches = std::move(batches);
    glGenVertexArrays(1, &app.vao);
    glBindVertexArray(app.vao);

    glGenBuffers(1, &app.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, app.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(TileVertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TileVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TileVertex),
        reinterpret_cast<const void*>(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    std::printf(
        "tile smoke: layers=%zu tilesets=%zu batches=%zu vertices=%zu\n",
        map.layers.size(),
        tilesets.size(),
        app.batches.size(),
        vertices.size());
    return glGetError() == GL_NO_ERROR;
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    std::vector<char> log(1024, '\0');
    GLsizei length = 0;
    glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &length, log.data());
    std::fprintf(stderr, "Shader compile failed: %.*s\n", length, log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint createProgram() {
    const GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, kTileVertexShader);
    if (vertex_shader == 0) {
        return 0;
    }

    const GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, kTileFragmentShader);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    std::vector<char> log(1024, '\0');
    GLsizei length = 0;
    glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &length, log.data());
    std::fprintf(stderr, "Program link failed: %.*s\n", length, log.data());
    glDeleteProgram(program);
    return 0;
}

void shutdown(WebApp& app) {
    if (app.vbo != 0) {
        glDeleteBuffers(1, &app.vbo);
        app.vbo = 0;
    }
    if (app.vao != 0) {
        glDeleteVertexArrays(1, &app.vao);
        app.vao = 0;
    }
    for (Tileset& tileset : app.tilesets) {
        if (tileset.texture != 0) {
            glDeleteTextures(1, &tileset.texture);
            tileset.texture = 0;
        }
    }
    app.tilesets.clear();
    app.batches.clear();
    if (app.program != 0) {
        glDeleteProgram(app.program);
        app.program = 0;
    }
    if (app.gl_context != nullptr) {
        SDL_GL_DestroyContext(app.gl_context);
        app.gl_context = nullptr;
    }
    if (app.window != nullptr) {
        SDL_DestroyWindow(app.window);
        app.window = nullptr;
    }
    SDL_Quit();
}

void frame(void* user_data) {
    auto* app = static_cast<WebApp*>(user_data);
    if (app == nullptr || !app->running) {
        emscripten_cancel_main_loop();
        return;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            app->running = false;
            emscripten_cancel_main_loop();
            shutdown(*app);
            return;
        }
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.08F, 0.12F, 0.16F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    const float fit_scale = std::min(
        static_cast<float>(width) / static_cast<float>(app->map_width_pixels),
        static_cast<float>(height) / static_cast<float>(app->map_height_pixels));
    const float scale = fit_scale * 0.92F;
    const float origin_x = (static_cast<float>(width) - static_cast<float>(app->map_width_pixels) * scale) * 0.5F;
    const float origin_y = (static_cast<float>(height) - static_cast<float>(app->map_height_pixels) * scale) * 0.5F;

    glUseProgram(app->program);
    glUniform2f(app->canvas_size_uniform, static_cast<float>(width), static_cast<float>(height));
    glUniform2f(app->origin_uniform, origin_x, origin_y);
    glUniform1f(app->scale_uniform, scale);
    glUniform1i(app->texture_uniform, 0);
    glBindVertexArray(app->vao);

    glActiveTexture(GL_TEXTURE0);
    for (const DrawBatch& batch : app->batches) {
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glDrawArrays(GL_TRIANGLES, batch.first_vertex, batch.vertex_count);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    SDL_GL_SwapWindow(app->window);
}

bool initialize(WebApp& app) {
    tinyfarm::web::setShellStatus("Initializing WebGL");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        tinyfarm::web::setShellStatus("SDL init failed");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);

    app.window = SDL_CreateWindow(
        "TinyFarmRPG WebGL2 Tile Smoke",
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (app.window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        tinyfarm::web::setShellStatus("Window init failed");
        shutdown(app);
        return false;
    }

    app.gl_context = SDL_GL_CreateContext(app.window);
    if (app.gl_context == nullptr) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        tinyfarm::web::setShellStatus("WebGL context failed");
        shutdown(app);
        return false;
    }

    SDL_GL_MakeCurrent(app.window, app.gl_context);
    SDL_GL_SetSwapInterval(1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    logGlInfo();
    tinyfarm::web::reportShellWebGlFeatures();

    TileMap map = loadTileMap(kMapPath);
    if (map.width <= 0 || map.height <= 0 || map.layers.empty() || map.tilesets.empty()) {
        std::fprintf(stderr, "Failed to load Web tile map resources.\n");
        tinyfarm::web::setShellStatus("Map resources failed");
        shutdown(app);
        return false;
    }

    app.program = createProgram();
    if (app.program == 0) {
        tinyfarm::web::setShellStatus("Shader init failed");
        shutdown(app);
        return false;
    }
    app.canvas_size_uniform = glGetUniformLocation(app.program, "u_canvas_size");
    app.origin_uniform = glGetUniformLocation(app.program, "u_origin_pixels");
    app.scale_uniform = glGetUniformLocation(app.program, "u_scale");
    app.texture_uniform = glGetUniformLocation(app.program, "u_tex");

    app.map_width_pixels = map.width * map.tile_width;
    app.map_height_pixels = map.height * map.tile_height;
    app.tilesets = std::move(map.tilesets);

    if (!buildTileGeometry(app, map, app.tilesets)) {
        std::fprintf(stderr, "Failed to create WebGL tile geometry.\n");
        tinyfarm::web::setShellStatus("Tile geometry failed");
        shutdown(app);
        return false;
    }

    tinyfarm::web::setShellMapStats(
        static_cast<int>(map.layers.size()),
        static_cast<int>(app.tilesets.size()),
        static_cast<int>(app.batches.size()),
        app.map_width_pixels,
        app.map_height_pixels);
    tinyfarm::web::setShellStatus("Running");
    return true;
}

void mainLoop(void* user_data) {
    auto* app = static_cast<WebApp*>(user_data);
    if (app == nullptr || !app->running) {
        emscripten_cancel_main_loop();
        return;
    }

    switch (app->startup_stage) {
    case StartupStage::WaitingPersistentSync:
        return;
    case StartupStage::ReadyToInitialize:
        if (!initialize(*app)) {
            std::fprintf(stderr, "TinyFarmRPG Web failed to start after persistent FS setup.\n");
            app->startup_stage = StartupStage::Failed;
            app->running = false;
            emscripten_cancel_main_loop();
            return;
        }
        app->startup_stage = StartupStage::Running;
        break;
    case StartupStage::Failed:
        emscripten_cancel_main_loop();
        return;
    case StartupStage::Running:
        break;
    }

    frame(user_data);
}

void onPersistentSyncedToBrowser(bool success, void* user_data) {
    if (!success) {
        std::fprintf(stderr, "Persistent FS sync-to-browser failed; continuing with in-memory data.\n");
        tinyfarm::web::setShellStatus("Storage flush failed; continuing");
    } else {
        tinyfarm::web::setShellStatus("Storage ready");
    }

    auto* app = static_cast<WebApp*>(user_data);
    if (app != nullptr) {
        app->startup_stage = StartupStage::ReadyToInitialize;
    }
}

void onPersistentSyncedFromBrowser(bool success, void* user_data) {
    if (!success) {
        std::fprintf(stderr, "Persistent FS sync-from-browser failed; continuing with in-memory data.\n");
        tinyfarm::web::setShellStatus("Storage load failed; continuing");
        onPersistentSyncedToBrowser(false, user_data);
        return;
    }

    const std::uint32_t previous_boot_count = readPersistenceSmokeCounter();
    const std::uint32_t next_boot_count = previous_boot_count + 1;
    if (!writePersistenceSmokeCounter(next_boot_count)) {
        std::fprintf(stderr, "Persistent FS smoke write failed; continuing without IDBFS smoke.\n");
        tinyfarm::web::setShellStatus("Storage smoke failed; continuing");
        onPersistentSyncedToBrowser(false, user_data);
        return;
    }

    std::printf(
        "persistent smoke: root=%s path=%s previous_boot_count=%u next_boot_count=%u\n",
        engine::platform::WEB_PERSISTENT_ROOT.data(),
        kPersistenceSmokePath.data(),
        previous_boot_count,
        next_boot_count);
    engine::platform::web::syncPersistentStorageToBrowser(onPersistentSyncedToBrowser, user_data);
}

} // namespace

int main() {
    static WebApp app{};
    tinyfarm::web::installShellUi();
    tinyfarm::web::setShellStatus("Syncing storage");
    engine::platform::web::syncPersistentStorageFromBrowser(onPersistentSyncedFromBrowser, &app);
    emscripten_set_main_loop_arg(mainLoop, &app, 0, true);
    return 0;
}
