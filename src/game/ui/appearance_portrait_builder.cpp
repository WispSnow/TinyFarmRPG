#include "game/ui/appearance_portrait_builder.h"

#include "engine/resource/image_decode_service.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace game::ui {
namespace {

struct PortraitCrop {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

constexpr PortraitCrop STANDARD_64_CROP{0, 0, 64, 64};
constexpr PortraitCrop BATTLE_48_CROP{8, 8, 48, 48};

[[nodiscard]] std::string variantOrDefault(const game::scene::AppearanceSelection& selection,
                                           std::string_view slot,
                                           std::string_view fallback = "none") {
    if (const auto it = selection.slot_variants.find(std::string(slot)); it != selection.slot_variants.end()) {
        return it->second;
    }
    return std::string(fallback);
}

[[nodiscard]] bool isElfEarVariant(std::string_view acc) {
    return acc.rfind("Elf/", 0) == 0;
}

[[nodiscard]] engine::resource::DecodedImage makeTransparentImage(const int width, const int height) {
    engine::resource::DecodedImage image{};
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);
    return image;
}

void blendPixel(std::uint8_t* dst, const std::uint8_t* src) {
    const auto src_a = static_cast<std::uint32_t>(src[3]);
    if (src_a == 0U) {
        return;
    }
    if (src_a == 255U) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    }

    const auto dst_a = static_cast<std::uint32_t>(dst[3]);
    const auto inv_src_a = 255U - src_a;
    const auto out_a = src_a + ((dst_a * inv_src_a + 127U) / 255U);
    if (out_a == 0U) {
        dst[0] = 0U;
        dst[1] = 0U;
        dst[2] = 0U;
        dst[3] = 0U;
        return;
    }

    for (std::size_t channel = 0; channel < 3U; ++channel) {
        const auto src_term = static_cast<std::uint32_t>(src[channel]) * src_a * 255U;
        const auto dst_term = static_cast<std::uint32_t>(dst[channel]) * dst_a * inv_src_a;
        dst[channel] = static_cast<std::uint8_t>((src_term + dst_term + (out_a * 127U)) / (out_a * 255U));
    }
    dst[3] = static_cast<std::uint8_t>(out_a);
}

void blendImage(engine::resource::DecodedImage& dst, const engine::resource::DecodedImage& src) {
    if (!dst.valid() || !src.valid() || dst.channels != 4 || src.channels != 4 ||
        dst.width != src.width || dst.height != src.height) {
        return;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(dst.width) * static_cast<std::size_t>(dst.height);
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        blendPixel(&dst.pixels[pixel * 4U], &src.pixels[pixel * 4U]);
    }
}

[[nodiscard]] engine::resource::DecodedImage cropImage(const engine::resource::DecodedImage& source,
                                                       const PortraitCrop& crop) {
    if (!source.valid() || source.channels != 4 || crop.x < 0 || crop.y < 0 ||
        crop.width <= 0 || crop.height <= 0 ||
        crop.x + crop.width > source.width || crop.y + crop.height > source.height) {
        return {};
    }

    engine::resource::DecodedImage output{};
    output.width = crop.width;
    output.height = crop.height;
    output.channels = 4;
    output.pixels.resize(static_cast<std::size_t>(crop.width) * static_cast<std::size_t>(crop.height) * 4U);

    for (int y = 0; y < crop.height; ++y) {
        const auto source_offset =
            (static_cast<std::size_t>(crop.y + y) * static_cast<std::size_t>(source.width) +
             static_cast<std::size_t>(crop.x)) * 4U;
        const auto output_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(crop.width) * 4U;
        std::copy_n(
            source.pixels.begin() + static_cast<std::ptrdiff_t>(source_offset),
            static_cast<std::size_t>(crop.width) * 4U,
            output.pixels.begin() + static_cast<std::ptrdiff_t>(output_offset));
    }

    return output;
}

[[nodiscard]] std::uint64_t fnv1a64(std::string_view value) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char c : value) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

AppearancePortraitImages AppearancePortraitBuilder::build(const game::data::AppearanceCatalog& catalog,
                                                          const game::scene::AppearanceSelection& selection) {
    const std::string skin = variantOrDefault(selection, "skin", "1");
    const std::string acc = variantOrDefault(selection, "acc", "none");

    engine::resource::DecodedImage sheet{};
    bool initialized = false;

    for (const auto& layer : catalog.portraitLayerOrder()) {
        std::string variant{};
        if (layer == "skin") {
            variant = skin;
        } else if (layer == "ears") {
            variant = isElfEarVariant(acc) ? acc : "Human/" + skin;
        } else if (layer == "clothes") {
            variant = variantOrDefault(selection, "clothes");
        } else if (layer == "eyes") {
            variant = variantOrDefault(selection, "eyes");
        } else if (layer == "hair") {
            variant = variantOrDefault(selection, "hair");
        } else if (layer == "acc") {
            if (acc == "none" || isElfEarVariant(acc)) {
                continue;
            }
            variant = acc;
        } else {
            continue;
        }

        const auto texture = catalog.resolvePortraitLayerTexture(layer, variant, selection.gender);
        if (!texture) {
            spdlog::warn("AppearancePortraitBuilder: portrait layer '{}' variant '{}' not found.", layer, variant);
            continue;
        }

        const auto* layer_image = decodeLayer(texture->path_);
        if (!layer_image) {
            spdlog::warn("AppearancePortraitBuilder: failed to decode portrait layer '{}'.", texture->path_);
            continue;
        }

        if (!initialized) {
            sheet = makeTransparentImage(layer_image->width, layer_image->height);
            initialized = true;
        }
        blendImage(sheet, *layer_image);
    }

    if (!sheet.valid()) {
        return {};
    }

    AppearancePortraitImages result{};
    result.selection_key = stableSelectionKey(catalog, selection);
    result.standard64 = cropImage(sheet, STANDARD_64_CROP);
    result.battle48 = cropImage(sheet, BATTLE_48_CROP);
    return result;
}

AppearancePortraitImages AppearancePortraitBuilder::build(const game::data::AppearanceCatalog& catalog,
                                                          const game::component::AppearanceComponent& appearance) {
    game::scene::AppearanceSelection selection{};
    selection.profile_id = appearance.profile_id_;
    selection.gender = appearance.gender_.empty() ? std::string{"male"} : appearance.gender_;
    selection.slot_variants = appearance.slot_variants_;
    return build(catalog, selection);
}

std::string AppearancePortraitBuilder::stableSelectionKey(const game::data::AppearanceCatalog& catalog,
                                                          const game::scene::AppearanceSelection& selection) {
    std::string canonical = "gender=" + selection.gender;
    const auto& order = catalog.selectionOrder().empty() ? catalog.layerOrder() : catalog.selectionOrder();
    for (const auto& slot : order) {
        if (slot == "gender") {
            continue;
        }
        canonical.push_back('|');
        canonical.append(slot);
        canonical.push_back('=');
        canonical.append(variantOrDefault(selection, slot));
    }
    return std::to_string(fnv1a64(canonical));
}

const engine::resource::DecodedImage* AppearancePortraitBuilder::decodeLayer(std::string_view path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::string key(path);
    if (const auto it = decoded_layer_cache_.find(key); it != decoded_layer_cache_.end()) {
        return &it->second;
    }

    auto decoded = engine::resource::ImageDecodeService::decodeRGBA(path);
    if (!decoded || !decoded->valid() || decoded->channels != 4) {
        return nullptr;
    }

    auto [it, _] = decoded_layer_cache_.emplace(key, std::move(*decoded));
    return &it->second;
}

} // namespace game::ui
