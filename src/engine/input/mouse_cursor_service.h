#pragma once

#include "engine/resource/decoded_image.h"

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Cursor;

namespace engine::input {

/// @brief Project-level semantic mouse cursor kinds.
///
/// Custom bitmap cursors are provided for the gameplay/UI states. Text, resize,
/// and cross use SDL system cursors but still pass through MouseCursorService so
/// scoped overrides keep priority.
enum class MouseCursorKind : std::uint8_t {
    Default,
    Pointer,
    Grab,
    Dragging,
    Text,
    Resize,
    Cross,
    Count,
};

inline constexpr std::size_t MOUSE_CURSOR_KIND_COUNT = static_cast<std::size_t>(MouseCursorKind::Count);

/// @brief Source rectangle inside the cursor atlas, in source pixels.
struct MouseCursorSourceRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/// @brief Hotspot inside the unscaled source rectangle, in source pixels.
struct MouseCursorHotspot {
    int x{0};
    int y{0};
};

/// @brief Single cursor image declaration loaded from cursor_config.json.
struct MouseCursorImageSpec {
    MouseCursorSourceRect source{};
    MouseCursorHotspot hotspot{};
};

/// @brief Data-driven cursor theme parsed from cursor_config.json.
struct MouseCursorTheme {
    std::string sheet{};
    std::string theme_id{"warm_light"};
    int tile_size{16};
    int scale{2};
    std::array<std::optional<MouseCursorImageSpec>, MOUSE_CURSOR_KIND_COUNT> states{};
};

/// @brief Straight-alpha RGBA bitmap ready for SDL cursor surface creation.
struct MouseCursorBitmap {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels{};

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

class MouseCursorService;

/// @brief RAII token that temporarily overrides the active mouse cursor.
///
/// The token is move-only. Destroying or resetting it releases only the
/// override id it owns, so stale tokens cannot clear a newer override.
class ScopedCursorOverride final {
public:
    ScopedCursorOverride() = default;
    ~ScopedCursorOverride();

    ScopedCursorOverride(const ScopedCursorOverride&) = delete;
    ScopedCursorOverride& operator=(const ScopedCursorOverride&) = delete;

    ScopedCursorOverride(ScopedCursorOverride&& other) noexcept;
    ScopedCursorOverride& operator=(ScopedCursorOverride&& other) noexcept;

    void reset();
    [[nodiscard]] bool active() const noexcept { return owner_ != nullptr; }

private:
    friend class MouseCursorService;

    ScopedCursorOverride(MouseCursorService& owner, std::uint64_t override_id) noexcept;

    MouseCursorService* owner_{nullptr};
    std::uint64_t override_id_{0};
};

/// @brief Returns the stable config/string key for a cursor kind.
[[nodiscard]] std::string_view toString(MouseCursorKind kind) noexcept;
/// @brief Parses a cursor kind from its stable config/string key.
[[nodiscard]] std::optional<MouseCursorKind> mouseCursorKindFromString(std::string_view value) noexcept;
/// @brief Resolves the effective cursor by applying override priority over UI state.
[[nodiscard]] MouseCursorKind resolveMouseCursorKind(std::optional<MouseCursorKind> override_kind,
                                                     MouseCursorKind ui_kind) noexcept;
/// @brief Parses cursor theme JSON without touching SDL or image files.
/// @param root JSON object to parse.
/// @param out Receives the parsed theme on success.
/// @param error_message Optional human-readable parse failure reason.
[[nodiscard]] bool parseMouseCursorThemeJson(const nlohmann::json& root,
                                             MouseCursorTheme& out,
                                             std::string* error_message = nullptr);
/// @brief Crops and nearest-neighbor scales a straight-alpha RGBA image region.
/// @return Scaled bitmap, or nullopt when the source rectangle/hotspot is invalid.
[[nodiscard]] std::optional<MouseCursorBitmap> buildCursorBitmap(const engine::resource::DecodedImage& image,
                                                                 const MouseCursorImageSpec& spec,
                                                                 int scale);

/// @brief Owns SDL cursor resources and applies the currently resolved cursor.
///
/// The service has two inputs: RmlUi's current cursor kind and an optional
/// scoped override used by drag operations. Setters apply immediately and avoid
/// repeated SDL_SetCursor calls by caching the active SDL cursor pointer.
class MouseCursorService final {
public:
    MouseCursorService();
    ~MouseCursorService();

    MouseCursorService(const MouseCursorService&) = delete;
    MouseCursorService& operator=(const MouseCursorService&) = delete;
    MouseCursorService(MouseCursorService&&) = delete;
    MouseCursorService& operator=(MouseCursorService&&) = delete;

    /// @brief Loads custom cursor bitmaps from a JSON config file.
    /// @return True when all custom cursors were created; false leaves system cursors active.
    [[nodiscard]] bool loadTheme(std::string_view config_path);
    /// @brief Sets the cursor requested by UI hover state and applies it if no override is active.
    void setUiCursor(MouseCursorKind kind);
    /// @brief Creates a move-only token that overrides the UI cursor until released.
    [[nodiscard]] ScopedCursorOverride scopedOverride(MouseCursorKind kind);

    [[nodiscard]] MouseCursorKind uiCursorForTest() const noexcept { return ui_cursor_; }
    [[nodiscard]] std::optional<MouseCursorKind> overrideCursorForTest() const noexcept;
    [[nodiscard]] MouseCursorKind resolvedCursorForTest() const noexcept;

private:
    friend class ScopedCursorOverride;

    struct OverrideState {
        std::uint64_t id{0};
        MouseCursorKind kind{MouseCursorKind::Default};
    };

    void releaseOverride(std::uint64_t override_id);
    void resolveAndApply();
    [[nodiscard]] SDL_Cursor* cursorForKind(MouseCursorKind kind) const noexcept;
    [[nodiscard]] SDL_Cursor* createColorCursor(const engine::resource::DecodedImage& image,
                                                const MouseCursorImageSpec& spec,
                                                int scale) const;
    void createSystemCursors();
    void destroyCustomCursors();
    void destroySystemCursors();

    std::array<SDL_Cursor*, MOUSE_CURSOR_KIND_COUNT> system_cursors_{};
    std::array<SDL_Cursor*, MOUSE_CURSOR_KIND_COUNT> custom_cursors_{};
    SDL_Cursor* active_cursor_{nullptr};
    MouseCursorKind ui_cursor_{MouseCursorKind::Default};
    std::optional<OverrideState> override_{};
    std::uint64_t next_override_id_{1};
};

} // namespace engine::input
