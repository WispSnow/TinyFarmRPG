#pragma once

#include <entt/entity/fwd.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::data {

class VfxCatalog final {
public:
    [[nodiscard]] bool loadFromFile(std::string_view file_path);
    [[nodiscard]] const std::string* findEffectPath(entt::id_type effect_id) const;

private:
    std::unordered_map<entt::id_type, std::string> effect_paths_{};
};

} // namespace game::data
