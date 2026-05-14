#include "game/ui/map_preview_builder.h"

#include "engine/loader/tiled_json_cache.h"
#include "engine/resource/image_decode_service.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::ui {
namespace {

constexpr std::uint32_t TILED_GID_MASK = 0x0FFFFFFFU;
constexpr std::size_t PREVIEW_CACHE_MAX_ENTRIES = 4U;
constexpr int CHANNELS_RGBA = 4;

struct TilesetPreviewData {
    struct TileImage {
        engine::resource::DecodedImage image{};
        int width{0};
        int height{0};
    };

    std::uint32_t first_gid{0};
    int tile_width{0};
    int tile_height{0};
    int columns{0};
    int tile_count{0};
    int margin{0};
    int spacing{0};
    engine::resource::DecodedImage image{};
    std::unordered_map<std::uint32_t, TileImage> tile_images_by_id{};
};

struct Rgba {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};
};

[[nodiscard]] std::filesystem::path resolveRelativePath(const std::filesystem::path& base_file,
                                                        std::string_view relative_path) {
    return (base_file.parent_path() / std::filesystem::path{std::string{relative_path}}).lexically_normal();
}

[[nodiscard]] int hexDigit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    }
    return -1;
}

[[nodiscard]] std::optional<std::uint8_t> parseHexByte(std::string_view hex, std::size_t offset) {
    if (offset + 1U >= hex.size()) {
        return std::nullopt;
    }
    const int high = hexDigit(hex[offset]);
    const int low = hexDigit(hex[offset + 1U]);
    if (high < 0 || low < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((high << 4) | low);
}

[[nodiscard]] Rgba parseBackgroundColor(const nlohmann::json& map_json) {
    const std::string color = map_json.value("backgroundcolor", "#00000000");
    if (color.empty() || color.front() != '#') {
        return {};
    }

    const std::string_view hex{color.data() + 1, color.size() - 1U};
    if (hex.size() == 6U) {
        const auto r = parseHexByte(hex, 0);
        const auto g = parseHexByte(hex, 2);
        const auto b = parseHexByte(hex, 4);
        if (r && g && b) {
            return Rgba{*r, *g, *b, 255};
        }
    }
    if (hex.size() == 8U) {
        const auto a = parseHexByte(hex, 0);
        const auto r = parseHexByte(hex, 2);
        const auto g = parseHexByte(hex, 4);
        const auto b = parseHexByte(hex, 6);
        if (a && r && g && b) {
            return Rgba{*r, *g, *b, *a};
        }
    }

    return {};
}

[[nodiscard]] bool boolPropertyIsTrue(const nlohmann::json& object, std::string_view name) {
    const auto properties_it = object.find("properties");
    if (properties_it == object.end() || !properties_it->is_array()) {
        return false;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object() || property.value("name", "") != name) {
            continue;
        }
        const auto value_it = property.find("value");
        return value_it != property.end() && value_it->is_boolean() && value_it->get<bool>();
    }
    return false;
}

[[nodiscard]] engine::resource::DecodedImage makeImage(int width, int height, Rgba color) {
    engine::resource::DecodedImage image{};
    if (width <= 0 || height <= 0) {
        return image;
    }

    image.width = width;
    image.height = height;
    image.channels = CHANNELS_RGBA;
    image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * CHANNELS_RGBA);
    for (std::size_t i = 0; i < image.pixels.size(); i += CHANNELS_RGBA) {
        image.pixels[i + 0U] = color.r;
        image.pixels[i + 1U] = color.g;
        image.pixels[i + 2U] = color.b;
        image.pixels[i + 3U] = color.a;
    }
    return image;
}

void blendPixel(engine::resource::DecodedImage& target,
                int x,
                int y,
                const std::uint8_t* source_pixel,
                float layer_opacity) {
    if (!source_pixel || x < 0 || y < 0 || x >= target.width || y >= target.height) {
        return;
    }

    const float source_alpha = (static_cast<float>(source_pixel[3]) / 255.0F) * std::clamp(layer_opacity, 0.0F, 1.0F);
    if (source_alpha <= 0.0F) {
        return;
    }

    const std::size_t target_base =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(target.width) + static_cast<std::size_t>(x)) *
        CHANNELS_RGBA;
    const float target_alpha = static_cast<float>(target.pixels[target_base + 3U]) / 255.0F;
    const float out_alpha = source_alpha + target_alpha * (1.0F - source_alpha);
    if (out_alpha <= 0.0F) {
        target.pixels[target_base + 3U] = 0;
        return;
    }

    for (std::size_t channel = 0; channel < 3U; ++channel) {
        const float source_color = static_cast<float>(source_pixel[channel]) / 255.0F;
        const float target_color = static_cast<float>(target.pixels[target_base + channel]) / 255.0F;
        const float out_color =
            (source_color * source_alpha + target_color * target_alpha * (1.0F - source_alpha)) / out_alpha;
        target.pixels[target_base + channel] =
            static_cast<std::uint8_t>(std::clamp(std::lround(out_color * 255.0F), 0L, 255L));
    }
    target.pixels[target_base + 3U] =
        static_cast<std::uint8_t>(std::clamp(std::lround(out_alpha * 255.0F), 0L, 255L));
}

void drawImageRectScaled(engine::resource::DecodedImage& target,
                         const engine::resource::DecodedImage& source,
                         int source_x,
                         int source_y,
                         int source_width,
                         int source_height,
                         int dest_x,
                         int dest_y,
                         int dest_width,
                         int dest_height,
                         float opacity) {
    if (!source.valid() || source.channels != CHANNELS_RGBA || source_width <= 0 || source_height <= 0 ||
        dest_width <= 0 || dest_height <= 0) {
        return;
    }

    for (int y = 0; y < dest_height; ++y) {
        const int read_y = source_y + std::clamp((y * source_height) / dest_height, 0, source_height - 1);
        for (int x = 0; x < dest_width; ++x) {
            const int read_x = source_x + std::clamp((x * source_width) / dest_width, 0, source_width - 1);
            if (read_x < 0 || read_y < 0 || read_x >= source.width || read_y >= source.height) {
                continue;
            }
            const std::size_t source_base =
                (static_cast<std::size_t>(read_y) * static_cast<std::size_t>(source.width) +
                 static_cast<std::size_t>(read_x)) *
                CHANNELS_RGBA;
            blendPixel(target, dest_x + x, dest_y + y, &source.pixels[source_base], opacity);
        }
    }
}

[[nodiscard]] const TilesetPreviewData* findTilesetForGid(const std::vector<TilesetPreviewData>& tilesets,
                                                         std::uint32_t gid) {
    const TilesetPreviewData* selected = nullptr;
    for (const auto& tileset : tilesets) {
        if (gid >= tileset.first_gid && (!selected || tileset.first_gid > selected->first_gid)) {
            selected = &tileset;
        }
    }
    return selected;
}

void drawTile(engine::resource::DecodedImage& target,
              const TilesetPreviewData& tileset,
              std::uint32_t gid,
              int dest_x,
              int dest_y,
              float layer_opacity) {
    const std::uint32_t local_id = gid - tileset.first_gid;
    if (local_id >= static_cast<std::uint32_t>(tileset.tile_count)) {
        return;
    }

    if (const auto image_it = tileset.tile_images_by_id.find(local_id);
        image_it != tileset.tile_images_by_id.end()) {
        const auto& tile_image = image_it->second;
        const int width = tile_image.width > 0 ? tile_image.width : tile_image.image.width;
        const int height = tile_image.height > 0 ? tile_image.height : tile_image.image.height;
        drawImageRectScaled(
            target,
            tile_image.image,
            0,
            0,
            tile_image.image.width,
            tile_image.image.height,
            dest_x,
            dest_y,
            width,
            height,
            layer_opacity);
        return;
    }

    if (tileset.columns <= 0 || !tileset.image.valid()) {
        return;
    }

    const int tile_column = static_cast<int>(local_id % static_cast<std::uint32_t>(tileset.columns));
    const int tile_row = static_cast<int>(local_id / static_cast<std::uint32_t>(tileset.columns));
    const int source_x = tileset.margin + tile_column * (tileset.tile_width + tileset.spacing);
    const int source_y = tileset.margin + tile_row * (tileset.tile_height + tileset.spacing);

    drawImageRectScaled(
        target,
        tileset.image,
        source_x,
        source_y,
        tileset.tile_width,
        tileset.tile_height,
        dest_x,
        dest_y,
        tileset.tile_width,
        tileset.tile_height,
        layer_opacity);
}

void drawTileObject(engine::resource::DecodedImage& target,
                    const TilesetPreviewData& tileset,
                    std::uint32_t gid,
                    int dest_x,
                    int dest_y,
                    int dest_width,
                    int dest_height,
                    float layer_opacity) {
    const std::uint32_t local_id = gid - tileset.first_gid;
    if (local_id >= static_cast<std::uint32_t>(tileset.tile_count)) {
        return;
    }

    if (const auto image_it = tileset.tile_images_by_id.find(local_id);
        image_it != tileset.tile_images_by_id.end()) {
        const auto& tile_image = image_it->second;
        drawImageRectScaled(
            target,
            tile_image.image,
            0,
            0,
            tile_image.image.width,
            tile_image.image.height,
            dest_x,
            dest_y,
            dest_width > 0 ? dest_width : tile_image.image.width,
            dest_height > 0 ? dest_height : tile_image.image.height,
            layer_opacity);
        return;
    }

    if (tileset.columns <= 0 || !tileset.image.valid()) {
        return;
    }

    const int tile_column = static_cast<int>(local_id % static_cast<std::uint32_t>(tileset.columns));
    const int tile_row = static_cast<int>(local_id / static_cast<std::uint32_t>(tileset.columns));
    const int source_x = tileset.margin + tile_column * (tileset.tile_width + tileset.spacing);
    const int source_y = tileset.margin + tile_row * (tileset.tile_height + tileset.spacing);
    drawImageRectScaled(
        target,
        tileset.image,
        source_x,
        source_y,
        tileset.tile_width,
        tileset.tile_height,
        dest_x,
        dest_y,
        dest_width > 0 ? dest_width : tileset.tile_width,
        dest_height > 0 ? dest_height : tileset.tile_height,
        layer_opacity);
}

[[nodiscard]] std::vector<TilesetPreviewData> loadTilesets(const nlohmann::json& map_json,
                                                          const std::filesystem::path& map_path) {
    std::vector<TilesetPreviewData> tilesets{};
    const auto tilesets_it = map_json.find("tilesets");
    if (tilesets_it == map_json.end() || !tilesets_it->is_array()) {
        return tilesets;
    }

    for (const auto& tileset_ref : *tilesets_it) {
        if (!tileset_ref.is_object() || !tileset_ref.contains("source")) {
            continue;
        }

        const auto tileset_path = resolveRelativePath(map_path, tileset_ref.value("source", ""));
        auto tileset_json = engine::loader::tiled::getOrLoadTilesetJson(tileset_path.string());
        if (!tileset_json || !tileset_json->is_object()) {
            continue;
        }

        TilesetPreviewData data{};
        data.first_gid = tileset_ref.value("firstgid", 0U);
        data.tile_width = tileset_json->value("tilewidth", 0);
        data.tile_height = tileset_json->value("tileheight", 0);
        data.columns = tileset_json->value("columns", 0);
        data.tile_count = tileset_json->value("tilecount", 0);
        data.margin = tileset_json->value("margin", 0);
        data.spacing = tileset_json->value("spacing", 0);

        const std::string sheet_image_path = tileset_json->value("image", "");
        if (!sheet_image_path.empty()) {
            const auto image_path = resolveRelativePath(tileset_path, sheet_image_path);
            auto image = engine::resource::ImageDecodeService::decodeRGBA(image_path.string());
            if (!image) {
                spdlog::warn("MapPreviewBuilder: failed to decode tileset image '{}'.", image_path.string());
            } else {
                data.image = std::move(*image);
            }
        }

        if (const auto tiles_it = tileset_json->find("tiles"); tiles_it != tileset_json->end() && tiles_it->is_array()) {
            for (const auto& tile_json : *tiles_it) {
                if (!tile_json.is_object() || !tile_json.contains("image")) {
                    continue;
                }

                const std::string tile_image_path = tile_json.value("image", "");
                if (tile_image_path.empty()) {
                    continue;
                }

                const auto image_path = resolveRelativePath(tileset_path, tile_image_path);
                auto image = engine::resource::ImageDecodeService::decodeRGBA(image_path.string());
                if (!image) {
                    spdlog::warn("MapPreviewBuilder: failed to decode collection tile image '{}'.", image_path.string());
                    continue;
                }

                TilesetPreviewData::TileImage tile_image{};
                tile_image.width = tile_json.value("width", tile_json.value("imagewidth", image->width));
                tile_image.height = tile_json.value("height", tile_json.value("imageheight", image->height));
                tile_image.image = std::move(*image);
                data.tile_images_by_id.emplace(tile_json.value("id", 0U), std::move(tile_image));
            }
        }

        const bool has_sheet = data.image.valid() && data.columns > 0;
        const bool has_collection_images = !data.tile_images_by_id.empty();
        if (data.first_gid == 0U || data.tile_width <= 0 || data.tile_height <= 0 || data.tile_count <= 0 ||
            (!has_sheet && !has_collection_images)) {
            continue;
        }
        tilesets.push_back(std::move(data));
    }

    std::ranges::sort(tilesets, {}, &TilesetPreviewData::first_gid);
    return tilesets;
}

void renderTileLayer(engine::resource::DecodedImage& target,
                     const nlohmann::json& layer,
                     const std::vector<TilesetPreviewData>& tilesets,
                     int map_tile_width,
                     int map_tile_height,
                     float inherited_opacity) {
    if (!layer.is_object() || layer.value("type", "") != "tilelayer" || layer.value("visible", true) == false ||
        boolPropertyIsTrue(layer, "invisible")) {
        return;
    }

    const float layer_opacity = inherited_opacity * static_cast<float>(layer.value("opacity", 1.0));
    if (layer_opacity <= 0.0F) {
        return;
    }

    const auto data_it = layer.find("data");
    if (data_it == layer.end() || !data_it->is_array()) {
        spdlog::warn("MapPreviewBuilder: tile layer '{}' has unsupported or missing data array.",
                     layer.value("name", ""));
        return;
    }

    const int layer_width = layer.value("width", target.width / std::max(map_tile_width, 1));
    const int layer_height = layer.value("height", target.height / std::max(map_tile_height, 1));
    const int offset_x_tiles = layer.value("x", 0);
    const int offset_y_tiles = layer.value("y", 0);

    const std::size_t expected_cells = static_cast<std::size_t>(std::max(layer_width, 0)) *
                                       static_cast<std::size_t>(std::max(layer_height, 0));
    if (data_it->size() < expected_cells) {
        spdlog::warn("MapPreviewBuilder: tile layer '{}' data is shorter than its declared size.",
                     layer.value("name", ""));
    }

    const std::size_t cell_count = std::min(data_it->size(), expected_cells);
    bool warned_tile_size_mismatch = false;
    for (std::size_t index = 0; index < cell_count; ++index) {
        const auto& raw_cell = (*data_it)[index];
        if (!raw_cell.is_number_unsigned() && !raw_cell.is_number_integer()) {
            continue;
        }

        const std::uint32_t gid = raw_cell.get<std::uint32_t>() & TILED_GID_MASK;
        if (gid == 0U) {
            continue;
        }

        const TilesetPreviewData* tileset = findTilesetForGid(tilesets, gid);
        if (!tileset || tileset->tile_width != map_tile_width || tileset->tile_height != map_tile_height) {
            if (tileset && !warned_tile_size_mismatch) {
                spdlog::warn(
                    "MapPreviewBuilder: tile layer '{}' uses tileset tile size {}x{}, expected {}x{}; skipping mismatched tiles.",
                    layer.value("name", ""),
                    tileset->tile_width,
                    tileset->tile_height,
                    map_tile_width,
                    map_tile_height);
                warned_tile_size_mismatch = true;
            }
            continue;
        }

        const int tile_x = static_cast<int>(index % static_cast<std::size_t>(layer_width)) + offset_x_tiles;
        const int tile_y = static_cast<int>(index / static_cast<std::size_t>(layer_width)) + offset_y_tiles;
        drawTile(
            target,
            *tileset,
            gid,
            tile_x * map_tile_width,
            tile_y * map_tile_height,
            layer_opacity);
    }
}

void renderObjectLayer(engine::resource::DecodedImage& target,
                       const nlohmann::json& layer,
                       const std::vector<TilesetPreviewData>& tilesets,
                       float inherited_opacity) {
    if (!layer.is_object() || layer.value("type", "") != "objectgroup" || layer.value("visible", true) == false ||
        boolPropertyIsTrue(layer, "invisible")) {
        return;
    }

    const float layer_opacity = inherited_opacity * static_cast<float>(layer.value("opacity", 1.0));
    if (layer_opacity <= 0.0F) {
        return;
    }

    const auto objects_it = layer.find("objects");
    if (objects_it == layer.end() || !objects_it->is_array()) {
        return;
    }

    for (const auto& object : *objects_it) {
        if (!object.is_object() || object.value("visible", true) == false || !object.contains("gid")) {
            continue;
        }

        const std::uint32_t gid = object.value("gid", 0U) & TILED_GID_MASK;
        if (gid == 0U) {
            continue;
        }

        const TilesetPreviewData* tileset = findTilesetForGid(tilesets, gid);
        if (!tileset) {
            continue;
        }

        const float width = static_cast<float>(object.value("width", static_cast<double>(tileset->tile_width)));
        const float height = static_cast<float>(object.value("height", static_cast<double>(tileset->tile_height)));
        const float x = static_cast<float>(object.value("x", 0.0));
        const float y = static_cast<float>(object.value("y", 0.0)) - height;
        drawTileObject(
            target,
            *tileset,
            gid,
            static_cast<int>(std::floor(x)),
            static_cast<int>(std::floor(y)),
            static_cast<int>(std::lround(width)),
            static_cast<int>(std::lround(height)),
            layer_opacity);
    }
}

void renderLayers(engine::resource::DecodedImage& target,
                  const nlohmann::json& layers,
                  const std::vector<TilesetPreviewData>& tilesets,
                  int map_tile_width,
                  int map_tile_height,
                  float inherited_opacity = 1.0F) {
    if (!layers.is_array()) {
        return;
    }

    for (const auto& layer : layers) {
        if (!layer.is_object() || layer.value("visible", true) == false || boolPropertyIsTrue(layer, "invisible")) {
            continue;
        }

        const float layer_opacity = inherited_opacity * static_cast<float>(layer.value("opacity", 1.0));
        if (layer.value("type", "") == "group") {
            const auto child_layers = layer.find("layers");
            if (child_layers != layer.end()) {
                renderLayers(target, *child_layers, tilesets, map_tile_width, map_tile_height, layer_opacity);
            }
            continue;
        }

        if (layer.value("type", "") == "objectgroup") {
            renderObjectLayer(target, layer, tilesets, inherited_opacity);
        } else {
            renderTileLayer(target, layer, tilesets, map_tile_width, map_tile_height, inherited_opacity);
        }
    }
}

} // namespace

MapPreviewBuildResult MapPreviewBuilder::buildPreview(std::string_view map_name, std::string_view tmj_path) {
    const std::string cache_key = std::string{map_name} + "|" + std::string{tmj_path};
    if (auto it = preview_cache_.find(cache_key); it != preview_cache_.end()) {
        MapPreviewBuildResult result = it->second;
        result.from_cache = true;
        return result;
    }

    MapPreviewBuildResult result{};
    if (tmj_path.empty()) {
        return result;
    }

    const std::filesystem::path map_path = std::filesystem::path{std::string{tmj_path}}.lexically_normal();
    auto map_json = engine::loader::tiled::getOrLoadLevelJson(map_path.string());
    if (!map_json || !map_json->is_object()) {
        return result;
    }

    const int map_width_tiles = map_json->value("width", 0);
    const int map_height_tiles = map_json->value("height", 0);
    const int map_tile_width = map_json->value("tilewidth", 0);
    const int map_tile_height = map_json->value("tileheight", 0);
    if (map_width_tiles <= 0 || map_height_tiles <= 0 || map_tile_width <= 0 || map_tile_height <= 0) {
        spdlog::warn("MapPreviewBuilder: map '{}' has invalid dimensions.", tmj_path);
        return result;
    }

    result.map_pixel_size = {map_width_tiles * map_tile_width, map_height_tiles * map_tile_height};
    result.image = makeImage(result.map_pixel_size.x, result.map_pixel_size.y, parseBackgroundColor(*map_json));
    if (!result.image.valid()) {
        return result;
    }

    const std::vector<TilesetPreviewData> tilesets = loadTilesets(*map_json, map_path);
    const auto layers_it = map_json->find("layers");
    if (layers_it != map_json->end()) {
        renderLayers(result.image, *layers_it, tilesets, map_tile_width, map_tile_height);
    }

    preview_cache_[cache_key] = result;
    preview_cache_order_.push_back(cache_key);
    enforceCacheCapacity();
    return result;
}

void MapPreviewBuilder::clearCache() {
    preview_cache_.clear();
    preview_cache_order_.clear();
}

void MapPreviewBuilder::enforceCacheCapacity() {
    while (preview_cache_.size() > PREVIEW_CACHE_MAX_ENTRIES && !preview_cache_order_.empty()) {
        preview_cache_.erase(preview_cache_order_.front());
        preview_cache_order_.pop_front();
    }
}

} // namespace game::ui
