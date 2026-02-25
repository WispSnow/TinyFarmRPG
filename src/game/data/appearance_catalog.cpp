#include "appearance_catalog.h"

#include "engine/utils/json_file_loader.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace game::data {

namespace {

[[nodiscard]] std::string toLower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

[[nodiscard]] std::string normalizeGender(std::string_view gender) {
    const std::string value = toLower(gender);
    if (value == "female") {
        return "female";
    }
    return "male";
}

[[nodiscard]] std::string normalizeDirection(std::string_view direction) {
    const std::string lowered = toLower(direction);
    if (lowered == "down" || lowered == "up" || lowered == "left" || lowered == "right") {
        return lowered;
    }
    return "down";
}

[[nodiscard]] bool isPngFile(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        return false;
    }
    const auto ext = path.extension().string();
    return ext == ".png" || ext == ".PNG";
}

[[nodiscard]] std::string resolveTextureRootPath(std::string_view configured_root, std::string_view catalog_path) {
    namespace fs = std::filesystem;

    fs::path root_path{std::string(configured_root)};
    if (root_path.is_absolute()) {
        return root_path.lexically_normal().string();
    }

    const fs::path catalog_abs = fs::absolute(fs::path(std::string(catalog_path)));
    const fs::path catalog_dir = catalog_abs.parent_path();

    std::vector<fs::path> base_candidates{
        fs::current_path(),
        catalog_dir,
        catalog_dir.parent_path(),
        catalog_dir.parent_path().parent_path()
    };
    for (const auto& base : base_candidates) {
        if (base.empty()) {
            continue;
        }
        const fs::path resolved = (base / root_path).lexically_normal();
        if (fs::exists(resolved) && fs::is_directory(resolved)) {
            return resolved.string();
        }
    }

    return root_path.lexically_normal().string();
}

} // namespace

bool AppearanceCatalog::loadFromFile(std::string_view file_path) {
    nlohmann::json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "AppearanceCatalog", spdlog::level::err)) {
        return false;
    }

    texture_root_.clear();
    default_profile_id_.clear();
    layer_order_.clear();
    slot_dirs_.clear();
    action_dirs_.clear();
    action_layouts_.clear();
    action_available_slots_.clear();
    profiles_.clear();
    slot_variants_.clear();
    runtime_switchable_slots_.clear();
    weapon_action_variants_.clear();

    texture_root_ = root.value("texture_root", std::string{});
    if (texture_root_.empty()) {
        spdlog::error("AppearanceCatalog: 缺少 texture_root");
        return false;
    }
    texture_root_ = resolveTextureRootPath(texture_root_, file_path);

    default_profile_id_ = root.value("default_profile", std::string{});

    if (const auto it = root.find("layer_order"); it != root.end() && it->is_array()) {
        for (const auto& value : *it) {
            if (value.is_string()) {
                layer_order_.push_back(value.get<std::string>());
            }
        }
    }
    if (layer_order_.empty()) {
        layer_order_ = {"skin", "eyes", "clothes", "hair", "acc", "weapon"};
    }

    if (const auto it = root.find("slot_dirs"); it != root.end() && it->is_object()) {
        for (const auto& [slot, dir] : it->items()) {
            if (!dir.is_string()) {
                continue;
            }
            slot_dirs_.emplace(slot, dir.get<std::string>());
        }
    }
    if (slot_dirs_.empty()) {
        spdlog::error("AppearanceCatalog: 缺少 slot_dirs");
        return false;
    }

    if (const auto it = root.find("action_dirs"); it != root.end() && it->is_object()) {
        for (const auto& [action_key, dir] : it->items()) {
            if (!dir.is_string()) {
                continue;
            }
            action_dirs_.emplace(action_key, dir.get<std::string>());
        }
    }
    if (action_dirs_.empty()) {
        spdlog::error("AppearanceCatalog: 缺少 action_dirs");
        return false;
    }

    if (const auto it = root.find("action_layouts"); it != root.end()) {
        if (!it->is_object()) {
            spdlog::error("AppearanceCatalog: action_layouts 必须是 object");
            return false;
        }
        for (const auto& [action_key, layout_json] : it->items()) {
            if (!layout_json.is_object()) {
                spdlog::error("AppearanceCatalog: action_layouts.{} 必须是 object", action_key);
                return false;
            }

            ActionLayoutConfig layout{};
            const auto frames = layout_json.value("frames_per_direction", 0);
            if (frames <= 0) {
                spdlog::error("AppearanceCatalog: action_layouts.{}.frames_per_direction 必须 > 0", action_key);
                return false;
            }
            layout.frames_per_direction_ = static_cast<std::size_t>(frames);

            if (const auto order_it = layout_json.find("direction_block_order");
                order_it != layout_json.end() && order_it->is_array()) {
                for (const auto& value : *order_it) {
                    if (!value.is_string()) {
                        continue;
                    }
                    layout.direction_block_order_.push_back(normalizeDirection(value.get<std::string>()));
                }
            }
            if (layout.direction_block_order_.empty()) {
                layout.direction_block_order_ = {"down", "up", "right", "left"};
            }

            layout.left_fallback_ = toLower(layout_json.value("left_fallback", std::string{"mirror_right"}));
            if (layout.left_fallback_.empty()) {
                layout.left_fallback_ = "none";
            }
            action_layouts_.insert_or_assign(action_key, std::move(layout));
        }
    }

    for (const auto& [action_key, _] : action_dirs_) {
        if (!action_layouts_.contains(action_key)) {
            spdlog::error("AppearanceCatalog: action_layouts 缺少 action_key: {}", action_key);
            return false;
        }
    }

    if (const auto it = root.find("runtime_switchable_slots"); it != root.end() && it->is_array()) {
        for (const auto& slot : *it) {
            if (slot.is_string()) {
                runtime_switchable_slots_.insert(slot.get<std::string>());
            }
        }
    }

    if (const auto it = root.find("weapon_action_variants"); it != root.end() && it->is_object()) {
        for (const auto& [action_key, variant] : it->items()) {
            if (!variant.is_string()) {
                continue;
            }
            weapon_action_variants_.emplace(action_key, variant.get<std::string>());
        }
    }

    if (const auto it = root.find("slot_variants"); it != root.end() && it->is_object()) {
        for (const auto& [slot, values] : it->items()) {
            if (!values.is_array()) {
                continue;
            }
            auto& variants = slot_variants_[slot];
            for (const auto& value : values) {
                if (value.is_string()) {
                    variants.push_back(value.get<std::string>());
                }
            }
        }
    }

    if (const auto it = root.find("profiles"); it != root.end() && it->is_object()) {
        for (const auto& [profile_id, profile_json] : it->items()) {
            if (!profile_json.is_object()) {
                continue;
            }

            AppearanceProfile profile{};
            profile.id_ = profile_id;
            profile.gender_ = normalizeGender(profile_json.value("gender", std::string{"male"}));

            if (const auto slots_it = profile_json.find("slots");
                slots_it != profile_json.end() && slots_it->is_object()) {
                for (const auto& [slot, variant] : slots_it->items()) {
                    if (!variant.is_string()) {
                        continue;
                    }
                    profile.slots_.emplace(slot, variant.get<std::string>());
                }
            }

            profiles_.emplace(profile.id_, std::move(profile));
        }
    }

    if (profiles_.empty()) {
        spdlog::error("AppearanceCatalog: 缺少 profiles");
        return false;
    }

    if (default_profile_id_.empty() || !profiles_.contains(default_profile_id_)) {
        default_profile_id_ = profiles_.begin()->first;
    }

    if (!buildActionSlotAvailability()) {
        return false;
    }

    return true;
}

const AppearanceProfile* AppearanceCatalog::findProfile(std::string_view profile_id) const {
    if (const auto it = profiles_.find(std::string(profile_id)); it != profiles_.end()) {
        return &it->second;
    }
    return nullptr;
}

const AppearanceProfile* AppearanceCatalog::defaultProfile() const {
    return findProfile(default_profile_id_);
}

const std::vector<std::string>& AppearanceCatalog::variantsForSlot(std::string_view slot) const {
    static const std::vector<std::string> kEmptyVariants{};
    if (const auto it = slot_variants_.find(std::string(slot)); it != slot_variants_.end()) {
        return it->second;
    }
    return kEmptyVariants;
}

bool AppearanceCatalog::isRuntimeSwitchableSlot(std::string_view slot) const {
    return runtime_switchable_slots_.contains(std::string(slot));
}

bool AppearanceCatalog::isSlotAvailableForAction(std::string_view action_key, std::string_view slot) const {
    if (const auto it = action_available_slots_.find(std::string(action_key)); it != action_available_slots_.end()) {
        return it->second.contains(std::string(slot));
    }
    return false;
}

std::optional<std::string> AppearanceCatalog::actionKeyFromAnimationName(std::string_view animation_name) const {
    if (animation_name.empty()) {
        return std::nullopt;
    }
    std::string key(animation_name);
    if (const auto separator = key.find('_'); separator != std::string::npos) {
        key = key.substr(0, separator);
    }
    if (action_dirs_.contains(key)) {
        return key;
    }
    return std::nullopt;
}

std::optional<std::string> AppearanceCatalog::directionKeyFromAnimationName(std::string_view animation_name) const {
    if (animation_name.empty()) {
        return std::nullopt;
    }

    std::string name(animation_name);
    const auto separator = name.rfind('_');
    if (separator == std::string::npos || separator + 1 >= name.size()) {
        return std::nullopt;
    }

    const std::string direction = normalizeDirection(name.substr(separator + 1));
    return direction;
}

std::optional<AppearanceCatalog::LayerLayout> AppearanceCatalog::resolveLayerLayout(std::string_view action_key,
                                                                                     std::string_view direction_key) const {
    const auto action_it = action_layouts_.find(std::string(action_key));
    if (action_it == action_layouts_.end()) {
        return std::nullopt;
    }

    const auto direction = normalizeDirection(direction_key);
    const auto& layout = action_it->second;
    auto find_direction_index = [&layout](std::string_view direction_name) -> std::optional<std::size_t> {
        const auto it = std::find(layout.direction_block_order_.begin(),
                                  layout.direction_block_order_.end(),
                                  direction_name);
        if (it == layout.direction_block_order_.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(layout.direction_block_order_.begin(), it));
    };

    if (const auto index = find_direction_index(direction); index.has_value()) {
        return LayerLayout{*index, layout.frames_per_direction_, false};
    }

    if (direction == "left" && layout.left_fallback_ == "mirror_right") {
        if (const auto right_index = find_direction_index("right"); right_index.has_value()) {
            return LayerLayout{*right_index, layout.frames_per_direction_, true};
        }
    }

    if (const auto down_index = find_direction_index("down"); down_index.has_value()) {
        return LayerLayout{*down_index, layout.frames_per_direction_, false};
    }

    if (!layout.direction_block_order_.empty()) {
        return LayerLayout{0u, layout.frames_per_direction_, false};
    }
    return std::nullopt;
}

std::optional<AppearanceCatalog::LayerTexture> AppearanceCatalog::resolveLayerTexture(std::string_view action_key,
                                                                                       std::string_view slot,
                                                                                       std::string_view variant,
                                                                                       std::string_view gender) const {
    const auto action_it = action_dirs_.find(std::string(action_key));
    if (action_it == action_dirs_.end()) {
        return std::nullopt;
    }

    const auto slot_it = slot_dirs_.find(std::string(slot));
    if (slot_it == slot_dirs_.end()) {
        return std::nullopt;
    }

    if (!isSlotAvailableForAction(action_key, slot)) {
        return std::nullopt;
    }

    const auto normalized_variant = normalizeVariantForResolution(action_key, slot, variant);
    if (!normalized_variant) {
        return std::nullopt;
    }

    const auto texture_path = resolveTexturePath(action_it->second, slot_it->second, slot, *normalized_variant, gender);
    if (!texture_path) {
        return std::nullopt;
    }

    const entt::id_type texture_id = entt::hashed_string{texture_path->c_str()}.value();
    return LayerTexture{*texture_path, texture_id};
}

std::vector<std::string> AppearanceCatalog::collectPreloadTexturePaths(const AppearanceProfile& profile,
                                                                       std::size_t runtime_variant_limit_per_slot) const {
    std::unordered_set<std::string> unique_paths{};

    for (const auto& [action_key, _] : action_dirs_) {
        for (const auto& slot : layer_order_) {
            if (!isSlotAvailableForAction(action_key, slot)) {
                continue;
            }

            std::string default_variant = "none";
            if (const auto it = profile.slots_.find(slot); it != profile.slots_.end()) {
                default_variant = it->second;
            }

            if (const auto resolved = resolveLayerTexture(action_key, slot, default_variant, profile.gender_); resolved) {
                unique_paths.insert(resolved->path_);
            }

            if (isRuntimeSwitchableSlot(slot)) {
                const auto& candidates = variantsForSlot(slot);
                std::size_t preload_count = candidates.size();
                if (runtime_variant_limit_per_slot != kPreloadAllRuntimeVariants) {
                    preload_count = std::min(preload_count, runtime_variant_limit_per_slot);
                }

                for (std::size_t i = 0; i < preload_count; ++i) {
                    const auto& variant = candidates[i];
                    if (const auto resolved = resolveLayerTexture(action_key, slot, variant, profile.gender_); resolved) {
                        unique_paths.insert(resolved->path_);
                    }
                }
            }
        }
    }

    std::vector<std::string> paths(unique_paths.begin(), unique_paths.end());
    std::sort(paths.begin(), paths.end());
    return paths;
}

bool AppearanceCatalog::buildActionSlotAvailability() {
    namespace fs = std::filesystem;

    action_available_slots_.clear();

    for (const auto& [action_key, action_dir] : action_dirs_) {
        const fs::path action_path = fs::path(texture_root_) / action_dir;
        if (!fs::exists(action_path) || !fs::is_directory(action_path)) {
            spdlog::error("AppearanceCatalog: action 目录不存在: {} -> {}", action_key, action_path.string());
            return false;
        }

        auto& available = action_available_slots_[action_key];
        for (const auto& [slot, slot_dir] : slot_dirs_) {
            const fs::path slot_path = action_path / slot_dir;
            if (fs::exists(slot_path) && fs::is_directory(slot_path)) {
                available.insert(slot);
            }
        }
    }

    for (const auto& [slot, slot_dir] : slot_dirs_) {
        bool has_any_action = false;
        for (const auto& [action_key, _] : action_dirs_) {
            if (isSlotAvailableForAction(action_key, slot)) {
                has_any_action = true;
                break;
            }
        }
        if (!has_any_action) {
            spdlog::error("AppearanceCatalog: slot 目录在所有 action 中都不可用: {} -> {}", slot, slot_dir);
            return false;
        }
    }

    return true;
}

std::optional<std::string> AppearanceCatalog::normalizeVariantForResolution(std::string_view action_key,
                                                                             std::string_view slot,
                                                                             std::string_view variant) const {
    if (variant.empty() || variant == "none") {
        return std::nullopt;
    }

    std::string normalized(variant);
    if (slot == "weapon" && normalized == "auto") {
        if (const auto it = weapon_action_variants_.find(std::string(action_key)); it != weapon_action_variants_.end()) {
            normalized = it->second;
        } else {
            return std::nullopt;
        }
    }

    if (normalized.empty() || normalized == "none") {
        return std::nullopt;
    }
    return normalized;
}

std::optional<std::string> AppearanceCatalog::resolveTexturePath(std::string_view action_dir,
                                                                 std::string_view slot_dir,
                                                                 std::string_view slot,
                                                                 std::string_view variant,
                                                                 std::string_view gender) const {
    namespace fs = std::filesystem;

    fs::path base_path = fs::path(texture_root_) / action_dir / slot_dir;
    if (slot == "eyes") {
        base_path /= (normalizeGender(gender) == "female") ? "Female" : "Male";
    }
    if (!fs::exists(base_path) || !fs::is_directory(base_path)) {
        return std::nullopt;
    }

    const fs::path variant_path = fs::path(std::string(variant));
    std::vector<fs::path> candidates{};
    if (variant_path.has_extension()) {
        candidates.push_back(base_path / variant_path);
    } else {
        candidates.push_back(base_path / variant_path);
        candidates.push_back(base_path / fs::path(std::string(variant) + ".png"));
    }

    for (const auto& candidate : candidates) {
        if (isPngFile(candidate)) {
            return candidate.lexically_normal().string();
        }
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            std::vector<fs::path> nested_files{};
            for (const auto& entry : fs::directory_iterator(candidate)) {
                if (isPngFile(entry.path())) {
                    nested_files.push_back(entry.path());
                }
            }
            if (!nested_files.empty()) {
                std::sort(nested_files.begin(), nested_files.end());
                return nested_files.front().lexically_normal().string();
            }
        }
    }

    return std::nullopt;
}

} // namespace game::data
