#include "engine/input/mouse_cursor_service.h"

#include "engine/resource/image_decode_service.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace engine::input {

namespace {

inline constexpr std::array<MouseCursorKind, 4> CUSTOM_CURSOR_KINDS{
    MouseCursorKind::Default,
    MouseCursorKind::Pointer,
    MouseCursorKind::Grab,
    MouseCursorKind::Dragging,
};

[[nodiscard]] std::size_t toIndex(MouseCursorKind kind) noexcept {
    return static_cast<std::size_t>(kind);
}

void setError(std::string* error_message, std::string message) {
    if (error_message) {
        *error_message = std::move(message);
    }
}

[[nodiscard]] std::optional<int> readInteger(const nlohmann::json& value) {
    if (const auto* signed_value = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        if (*signed_value < 0) {
            return std::nullopt;
        }
        return static_cast<int>(*signed_value);
    }
    if (const auto* unsigned_value = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        if (*unsigned_value > static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        return static_cast<int>(*unsigned_value);
    }
    return std::nullopt;
}

[[nodiscard]] int readPositiveField(const nlohmann::json& root,
                                    std::string_view key,
                                    int fallback,
                                    int max_value) {
    const auto it = root.find(std::string(key));
    if (it == root.end()) {
        return fallback;
    }
    const auto value = readInteger(*it);
    if (!value || *value <= 0) {
        return fallback;
    }
    return std::min(*value, max_value);
}

[[nodiscard]] bool parseIntArray(const nlohmann::json& node, std::span<int> out) {
    if (!node.is_array() || node.size() != out.size()) {
        return false;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        const auto value = readInteger(node[i]);
        if (!value) {
            return false;
        }
        out[i] = *value;
    }
    return true;
}

[[nodiscard]] bool parseImageSpec(const nlohmann::json& node,
                                  MouseCursorImageSpec& out,
                                  std::string_view state_name,
                                  std::string* error_message) {
    if (!node.is_object()) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' must be an object");
        return false;
    }

    const auto source_it = node.find("source");
    if (source_it == node.end()) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' is missing source");
        return false;
    }
    std::array<int, 4> source_values{};
    if (!parseIntArray(*source_it, source_values) ||
        source_values[2] <= 0 || source_values[3] <= 0) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' has invalid source");
        return false;
    }

    const auto hotspot_it = node.find("hotspot");
    if (hotspot_it == node.end()) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' is missing hotspot");
        return false;
    }
    std::array<int, 2> hotspot_values{};
    if (!parseIntArray(*hotspot_it, hotspot_values)) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' has invalid hotspot");
        return false;
    }
    if (hotspot_values[0] >= source_values[2] || hotspot_values[1] >= source_values[3]) {
        setError(error_message, "cursor state '" + std::string(state_name) + "' hotspot is outside source");
        return false;
    }

    out = MouseCursorImageSpec{
        .source = MouseCursorSourceRect{
            .x = source_values[0],
            .y = source_values[1],
            .width = source_values[2],
            .height = source_values[3],
        },
        .hotspot = MouseCursorHotspot{
            .x = hotspot_values[0],
            .y = hotspot_values[1],
        },
    };
    return true;
}

[[nodiscard]] SDL_SystemCursor systemCursorForKind(MouseCursorKind kind) noexcept {
    switch (kind) {
        case MouseCursorKind::Pointer:
            return SDL_SYSTEM_CURSOR_POINTER;
        case MouseCursorKind::Grab:
        case MouseCursorKind::Dragging:
            return SDL_SYSTEM_CURSOR_MOVE;
        case MouseCursorKind::Text:
            return SDL_SYSTEM_CURSOR_TEXT;
        case MouseCursorKind::Resize:
            return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
        case MouseCursorKind::Cross:
            return SDL_SYSTEM_CURSOR_CROSSHAIR;
        case MouseCursorKind::Default:
        case MouseCursorKind::Count:
            return SDL_SYSTEM_CURSOR_DEFAULT;
    }
    return SDL_SYSTEM_CURSOR_DEFAULT;
}

void destroyCursorArray(std::array<SDL_Cursor*, MOUSE_CURSOR_KIND_COUNT>& cursors) {
    for (auto*& cursor : cursors) {
        if (cursor) {
            SDL_DestroyCursor(cursor);
            cursor = nullptr;
        }
    }
}

} // namespace

ScopedCursorOverride::ScopedCursorOverride(MouseCursorService& owner, const std::uint64_t override_id) noexcept
    : owner_(&owner), override_id_(override_id) {
}

ScopedCursorOverride::~ScopedCursorOverride() {
    reset();
}

ScopedCursorOverride::ScopedCursorOverride(ScopedCursorOverride&& other) noexcept
    : owner_(other.owner_), override_id_(other.override_id_) {
    other.owner_ = nullptr;
    other.override_id_ = 0;
}

ScopedCursorOverride& ScopedCursorOverride::operator=(ScopedCursorOverride&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    reset();
    owner_ = other.owner_;
    override_id_ = other.override_id_;
    other.owner_ = nullptr;
    other.override_id_ = 0;
    return *this;
}

void ScopedCursorOverride::reset() {
    if (!owner_) {
        return;
    }
    owner_->releaseOverride(override_id_);
    owner_ = nullptr;
    override_id_ = 0;
}

std::string_view toString(MouseCursorKind kind) noexcept {
    switch (kind) {
        case MouseCursorKind::Default:
            return "default";
        case MouseCursorKind::Pointer:
            return "pointer";
        case MouseCursorKind::Grab:
            return "grab";
        case MouseCursorKind::Dragging:
            return "dragging";
        case MouseCursorKind::Text:
            return "text";
        case MouseCursorKind::Resize:
            return "resize";
        case MouseCursorKind::Cross:
            return "cross";
        case MouseCursorKind::Count:
            return "default";
    }
    return "default";
}

std::optional<MouseCursorKind> mouseCursorKindFromString(const std::string_view value) noexcept {
    for (std::size_t i = 0; i < MOUSE_CURSOR_KIND_COUNT; ++i) {
        const auto kind = static_cast<MouseCursorKind>(i);
        if (toString(kind) == value) {
            return kind;
        }
    }
    return std::nullopt;
}

MouseCursorKind resolveMouseCursorKind(const std::optional<MouseCursorKind> override_kind,
                                       const MouseCursorKind ui_kind) noexcept {
    return override_kind.value_or(ui_kind);
}

bool parseMouseCursorThemeJson(const nlohmann::json& root,
                               MouseCursorTheme& out,
                               std::string* error_message) {
    if (!root.is_object()) {
        setError(error_message, "cursor theme root must be an object");
        return false;
    }

    const auto sheet_it = root.find("sheet");
    if (sheet_it == root.end() || !sheet_it->is_string()) {
        setError(error_message, "cursor theme is missing sheet");
        return false;
    }

    MouseCursorTheme parsed{};
    parsed.sheet = sheet_it->get<std::string>();
    if (parsed.sheet.empty()) {
        setError(error_message, "cursor theme sheet is empty");
        return false;
    }

    if (const auto theme_it = root.find("theme_id"); theme_it != root.end() && theme_it->is_string()) {
        parsed.theme_id = theme_it->get<std::string>();
    }
    parsed.tile_size = readPositiveField(root, "tile_size", parsed.tile_size, 1024);
    parsed.scale = readPositiveField(root, "scale", parsed.scale, 8);

    const auto states_it = root.find("states");
    if (states_it == root.end() || !states_it->is_object()) {
        setError(error_message, "cursor theme is missing states object");
        return false;
    }

    for (const auto kind : CUSTOM_CURSOR_KINDS) {
        const std::string state_name{toString(kind)};
        const auto state_it = states_it->find(state_name);
        if (state_it == states_it->end()) {
            setError(error_message, "cursor theme is missing state '" + state_name + "'");
            return false;
        }
        MouseCursorImageSpec spec{};
        if (!parseImageSpec(*state_it, spec, state_name, error_message)) {
            return false;
        }
        parsed.states[toIndex(kind)] = spec;
    }

    out = std::move(parsed);
    return true;
}

std::optional<MouseCursorBitmap> buildCursorBitmap(const engine::resource::DecodedImage& image,
                                                   const MouseCursorImageSpec& spec,
                                                   const int scale) {
    if (!image.valid() || image.channels != 4 || scale <= 0 ||
        spec.source.x < 0 || spec.source.y < 0 ||
        spec.source.width <= 0 || spec.source.height <= 0 ||
        spec.hotspot.x < 0 || spec.hotspot.y < 0 ||
        spec.hotspot.x >= spec.source.width || spec.hotspot.y >= spec.source.height ||
        spec.source.x > image.width - spec.source.width ||
        spec.source.y > image.height - spec.source.height) {
        return std::nullopt;
    }

    MouseCursorBitmap bitmap{};
    bitmap.width = spec.source.width * scale;
    bitmap.height = spec.source.height * scale;
    bitmap.pixels.resize(static_cast<std::size_t>(bitmap.width) * static_cast<std::size_t>(bitmap.height) * 4U);

    for (int y = 0; y < bitmap.height; ++y) {
        const int src_y = spec.source.y + (y / scale);
        for (int x = 0; x < bitmap.width; ++x) {
            const int src_x = spec.source.x + (x / scale);
            const auto src_offset =
                (static_cast<std::size_t>(src_y) * static_cast<std::size_t>(image.width) +
                 static_cast<std::size_t>(src_x)) * 4U;
            const auto dst_offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.width) +
                 static_cast<std::size_t>(x)) * 4U;
            std::copy_n(
                image.pixels.begin() + static_cast<std::ptrdiff_t>(src_offset),
                4,
                bitmap.pixels.begin() + static_cast<std::ptrdiff_t>(dst_offset));
        }
    }

    return bitmap;
}

MouseCursorService::MouseCursorService() {
    createSystemCursors();
}

MouseCursorService::~MouseCursorService() {
    destroyCustomCursors();
    destroySystemCursors();
}

bool MouseCursorService::loadTheme(const std::string_view config_path) {
    if (config_path.empty()) {
        spdlog::warn("MouseCursorService: cursor config path is empty; using system cursors.");
        return false;
    }

    const std::filesystem::path path{config_path};
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("MouseCursorService: unable to open cursor config '{}'; using system cursors.", path.string());
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::warn("MouseCursorService: failed to parse cursor config '{}'; using system cursors.", path.string());
        return false;
    }

    MouseCursorTheme theme{};
    std::string error_message;
    if (!parseMouseCursorThemeJson(root, theme, &error_message)) {
        spdlog::warn("MouseCursorService: invalid cursor config '{}': {}; using system cursors.",
                     path.string(),
                     error_message);
        return false;
    }

    const auto decoded = engine::resource::ImageDecodeService::decodeRGBA(theme.sheet);
    if (!decoded) {
        spdlog::warn("MouseCursorService: unable to decode cursor sheet '{}'; using system cursors.", theme.sheet);
        return false;
    }

    std::array<SDL_Cursor*, MOUSE_CURSOR_KIND_COUNT> next_custom{};
    for (const auto kind : CUSTOM_CURSOR_KINDS) {
        const auto& spec = theme.states[toIndex(kind)];
        if (!spec) {
            destroyCursorArray(next_custom);
            spdlog::warn("MouseCursorService: cursor state '{}' is missing; using system cursors.", toString(kind));
            return false;
        }

        next_custom[toIndex(kind)] = createColorCursor(*decoded, *spec, theme.scale);
        if (!next_custom[toIndex(kind)]) {
            destroyCursorArray(next_custom);
            spdlog::warn("MouseCursorService: failed to create cursor '{}'; using system cursors.", toString(kind));
            return false;
        }
    }

    destroyCustomCursors();
    custom_cursors_ = next_custom;
    spdlog::info("MouseCursorService: loaded cursor theme '{}' from '{}'.", theme.theme_id, path.string());
    resolveAndApply();
    return true;
}

void MouseCursorService::setUiCursor(const MouseCursorKind kind) {
    if (kind == MouseCursorKind::Count || ui_cursor_ == kind) {
        return;
    }
    ui_cursor_ = kind;
    resolveAndApply();
}

ScopedCursorOverride MouseCursorService::scopedOverride(const MouseCursorKind kind) {
    const std::uint64_t id = next_override_id_++;
    override_ = OverrideState{.id = id, .kind = kind == MouseCursorKind::Count ? MouseCursorKind::Default : kind};
    resolveAndApply();
    return ScopedCursorOverride(*this, id);
}

std::optional<MouseCursorKind> MouseCursorService::overrideCursorForTest() const noexcept {
    if (!override_) {
        return std::nullopt;
    }
    return override_->kind;
}

MouseCursorKind MouseCursorService::resolvedCursorForTest() const noexcept {
    return resolveMouseCursorKind(overrideCursorForTest(), ui_cursor_);
}

void MouseCursorService::releaseOverride(const std::uint64_t override_id) {
    if (!override_ || override_->id != override_id) {
        return;
    }
    override_.reset();
    resolveAndApply();
}

void MouseCursorService::resolveAndApply() {
    const MouseCursorKind resolved = resolvedCursorForTest();
    SDL_Cursor* cursor = cursorForKind(resolved);
    if (!cursor || cursor == active_cursor_) {
        return;
    }

    SDL_SetCursor(cursor);
    active_cursor_ = cursor;
}

SDL_Cursor* MouseCursorService::cursorForKind(const MouseCursorKind kind) const noexcept {
    if (kind == MouseCursorKind::Count) {
        return system_cursors_[toIndex(MouseCursorKind::Default)];
    }

    const auto index = toIndex(kind);
    if (index >= MOUSE_CURSOR_KIND_COUNT) {
        return system_cursors_[toIndex(MouseCursorKind::Default)];
    }
    if (custom_cursors_[index]) {
        return custom_cursors_[index];
    }
    return system_cursors_[index] ? system_cursors_[index] : system_cursors_[toIndex(MouseCursorKind::Default)];
}

SDL_Cursor* MouseCursorService::createColorCursor(const engine::resource::DecodedImage& image,
                                                  const MouseCursorImageSpec& spec,
                                                  const int scale) const {
    auto bitmap = buildCursorBitmap(image, spec, scale);
    if (!bitmap || !bitmap->valid()) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        bitmap->width,
        bitmap->height,
        SDL_PIXELFORMAT_RGBA32,
        bitmap->pixels.data(),
        bitmap->width * 4);
    if (!surface) {
        return nullptr;
    }

    // SDL_CreateColorCursor copies the surface pixels, so bitmap can die after this call.
    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, spec.hotspot.x * scale, spec.hotspot.y * scale);
    SDL_DestroySurface(surface);
    return cursor;
}

void MouseCursorService::createSystemCursors() {
    for (std::size_t i = 0; i < MOUSE_CURSOR_KIND_COUNT; ++i) {
        const auto kind = static_cast<MouseCursorKind>(i);
        system_cursors_[i] = SDL_CreateSystemCursor(systemCursorForKind(kind));
        if (!system_cursors_[i]) {
            spdlog::warn("MouseCursorService: failed to create system cursor '{}'.", toString(kind));
        }
    }
}

void MouseCursorService::destroyCustomCursors() {
    destroyCursorArray(custom_cursors_);
    active_cursor_ = nullptr;
}

void MouseCursorService::destroySystemCursors() {
    destroyCursorArray(system_cursors_);
    active_cursor_ = nullptr;
}

} // namespace engine::input
