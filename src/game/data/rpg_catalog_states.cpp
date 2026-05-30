#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgStatesFile(const std::string_view file_path,
                       std::unordered_map<entt::id_type, StateData>& out_states) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogStates", spdlog::level::err)) {
        return false;
    }

    const auto states_it = root.find("states");
    if (states_it == root.end() || !states_it->is_array()) {
        spdlog::error("RpgCatalog: states 文件 '{}' 缺少 states 数组", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, StateData> parsed_states{};
    parsed_states.reserve(states_it->size());
    for (const auto& state_node : *states_it) {
        if (!state_node.is_object()) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        StateData state{};
        state.id_ = state_node.value("id", std::string{});
        if (state.id_.empty()) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        state.id_hash_ = RpgCatalog::hashId(state.id_);
        state.display_name_ = state_node.value("display_name", state.id_);
        state.description_ = state_node.value("description", std::string{});
        state.icon_key_ = state_node.value("icon_key", std::string{});
        state.priority_ = state_node.value("priority", 50);
        state.min_turns_ = state_node.value("min_turns", 1);
        state.max_turns_ = state_node.value("max_turns", 1);
        if (state.min_turns_ <= 0 || state.max_turns_ < state.min_turns_) {
            spdlog::error("RpgCatalog: state '{}' 回合范围非法", state.id_);
            return false;
        }

        const auto traits_it = state_node.find("traits");
        if (traits_it != state_node.end() && !parseRpgTraitList(*traits_it, state.traits_)) {
            spdlog::error("RpgCatalog: state '{}' traits 配置非法", state.id_);
            return false;
        }

        if (parsed_states.contains(state.id_hash_)) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在重复 id '{}'", file_path, state.id_);
            return false;
        }
        parsed_states.insert_or_assign(state.id_hash_, std::move(state));
    }

    out_states = std::move(parsed_states);
    return true;
}

} // namespace game::data
