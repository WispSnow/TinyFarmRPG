#pragma once

namespace engine::ui::rmlui {

// RmlUi MouseEvent.button uses DOM-style numbering: 0=primary, 1=secondary, 2=middle.
inline constexpr int kPrimaryMouseButton = 0;
inline constexpr int kSecondaryMouseButton = 1;
inline constexpr int kMiddleMouseButton = 2;

[[nodiscard]] constexpr bool isPrimaryMouseButton(int button) noexcept {
    return button == kPrimaryMouseButton;
}

[[nodiscard]] constexpr bool isSecondaryMouseButton(int button) noexcept {
    return button == kSecondaryMouseButton;
}

[[nodiscard]] constexpr bool isMiddleMouseButton(int button) noexcept {
    return button == kMiddleMouseButton;
}

} // namespace engine::ui::rmlui
