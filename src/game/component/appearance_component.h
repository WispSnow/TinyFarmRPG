#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::component {

struct AppearanceComponent {
    std::string profile_id_{};
    std::string gender_{"male"};
    std::unordered_map<std::string, std::string> slot_variants_{};
    bool dirty_{true};

    [[nodiscard]] std::string_view variantFor(std::string_view slot) const {
        if (const auto it = slot_variants_.find(std::string(slot)); it != slot_variants_.end()) {
            return it->second;
        }
        return {};
    }
};

} // namespace game::component
