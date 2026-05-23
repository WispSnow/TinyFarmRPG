#include "engine/input/input_binding_config.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace engine::input {

std::optional<InputMappingTable> loadInputMappingsConfig(const std::string_view config_path) {
    if (config_path.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path path{config_path};
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("InputManager: 无法打开输入配置文件 '{}'，使用默认映射。", path.string());
        return std::nullopt;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    const nlohmann::json json = nlohmann::json::parse(file_content, nullptr, false);
    if (json.is_discarded()) {
        spdlog::warn("InputManager: 解析输入配置 '{}' 失败，使用默认映射。", path.string());
        return std::nullopt;
    }

    const nlohmann::json* mappings_node = &json;
    if (auto it = json.find("input_mappings"); it != json.end()) {
        mappings_node = &(*it);
    }

    if (!mappings_node->is_object()) {
        spdlog::warn("InputManager: 输入配置文件 '{}' 缺少 input_mappings 对象，使用默认映射。", path.string());
        return std::nullopt;
    }

    InputMappingTable mappings;
    for (const auto& [action_name, key_array] : mappings_node->items()) {
        if (!key_array.is_array()) {
            spdlog::warn("InputManager: 输入配置文件 '{}' 的映射 '{}' 不是数组，使用默认映射。", path.string(), action_name);
            return std::nullopt;
        }

        std::vector<std::string> key_names;
        key_names.reserve(key_array.size());
        for (const auto& item : key_array) {
            if (!item.is_string()) {
                spdlog::warn(
                    "InputManager: 输入配置文件 '{}' 的映射 '{}' 包含非字符串条目，使用默认映射。",
                    path.string(),
                    action_name);
                return std::nullopt;
            }
            key_names.push_back(item.get<std::string>());
        }

        mappings.emplace(action_name, std::move(key_names));
    }

    spdlog::info("InputManager: 成功加载输入配置 '{}'", path.string());
    return mappings;
}

bool persistInputBindingsConfig(
    const std::string_view config_path,
    const std::vector<entt::id_type>& action_dispatch_order,
    const std::unordered_map<entt::id_type, ActionEntry>& actions,
    const std::unordered_map<entt::id_type, std::vector<BindingDefinition>>& action_bindings) {
    if (config_path.empty()) {
        spdlog::error("InputManager: 无法持久化绑定，config_path 为空。");
        return false;
    }

    nlohmann::ordered_json mappings = nlohmann::ordered_json::object();
    for (auto action_id : action_dispatch_order) {
        const auto action_it = actions.find(action_id);
        if (action_it == actions.end()) {
            continue;
        }

        nlohmann::ordered_json binding_array = nlohmann::ordered_json::array();
        if (const auto bindings_it = action_bindings.find(action_id); bindings_it != action_bindings.end()) {
            for (const auto& binding : bindings_it->second) {
                binding_array.push_back(binding.token);
            }
        }

        mappings[action_it->second.name] = std::move(binding_array);
    }

    nlohmann::ordered_json root = nlohmann::ordered_json::object();
    root["input_mappings"] = std::move(mappings);

    const std::filesystem::path path{config_path};
    const std::filesystem::path temp_path = path.string() + ".tmp";
    const std::filesystem::path backup_path = path.string() + ".bak";
    {
        std::ofstream file(temp_path);
        if (!file.is_open()) {
            spdlog::error("InputManager: 无法写入临时输入配置文件 '{}'.", temp_path.string());
            return false;
        }
        file << root.dump(2);
    }

    std::error_code ec;
    std::filesystem::rename(temp_path, path, ec);
    if (!ec) {
        ec.clear();
        std::filesystem::remove(backup_path, ec);
        return true;
    }

    const std::string rename_error = ec.message();
    ec.clear();
    const bool path_exists = std::filesystem::exists(path, ec);
    if (ec) {
        spdlog::error("InputManager: 检查输入配置文件 '{}' 是否存在失败: {}", path.string(), ec.message());
        std::filesystem::remove(temp_path, ec);
        return false;
    }
    if (!path_exists) {
        spdlog::error("InputManager: 替换输入配置文件 '{}' 失败: {}", path.string(), rename_error);
        std::filesystem::remove(temp_path, ec);
        return false;
    }

    ec.clear();
    std::filesystem::remove(backup_path, ec);
    ec.clear();
    std::filesystem::rename(path, backup_path, ec);
    if (ec) {
        spdlog::error("InputManager: 备份输入配置文件 '{}' 失败: {}", path.string(), ec.message());
        std::filesystem::remove(temp_path, ec);
        return false;
    }

    ec.clear();
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::error_code restore_ec;
        std::filesystem::rename(backup_path, path, restore_ec);
        if (restore_ec) {
            spdlog::error("InputManager: 恢复输入配置文件 '{}' 失败: {}", path.string(), restore_ec.message());
        } else {
            std::filesystem::remove(temp_path, restore_ec);
        }
        spdlog::error("InputManager: 替换输入配置文件 '{}' 失败: {}", path.string(), ec.message());
        return false;
    }

    ec.clear();
    std::filesystem::remove(backup_path, ec);
    if (ec) {
        spdlog::warn("InputManager: 清理输入配置备份 '{}' 失败: {}", backup_path.string(), ec.message());
    }

    return true;
}

} // namespace engine::input
