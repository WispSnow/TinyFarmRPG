#pragma once

#include <entt/core/fwd.hpp>

#include <optional>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace game::data {

struct AppearanceProfile {
    std::string id_{};
    std::string gender_{"male"};
    std::unordered_map<std::string, std::string> slots_{};
};

class AppearanceCatalog final {
public:
    static constexpr std::size_t kPreloadAllRuntimeVariants = std::numeric_limits<std::size_t>::max();

    struct LayerTexture {
        std::string path_{};
        entt::id_type texture_id_{};
    };

    struct LayerLayout {
        std::size_t direction_block_index_{0};
        std::size_t frames_per_direction_{0};
        bool use_animation_flip_{false};
    };

    AppearanceCatalog() = default;
    ~AppearanceCatalog() = default;

    [[nodiscard]] bool loadFromFile(std::string_view file_path);

    [[nodiscard]] const AppearanceProfile* findProfile(std::string_view profile_id) const;
    [[nodiscard]] const AppearanceProfile* defaultProfile() const;

    [[nodiscard]] const std::vector<std::string>& layerOrder() const { return layer_order_; }
    [[nodiscard]] const std::vector<std::string>& selectionOrder() const { return selection_order_; }
    [[nodiscard]] const std::vector<std::string>& genderVariants() const { return gender_variants_; }
    [[nodiscard]] const std::vector<std::string>& portraitLayerOrder() const { return portrait_layer_order_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& actionDirs() const { return action_dirs_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& slotDirs() const { return slot_dirs_; }

    [[nodiscard]] const std::vector<std::string>& variantsForSlot(std::string_view slot) const;
    [[nodiscard]] bool isRuntimeSwitchableSlot(std::string_view slot) const;
    [[nodiscard]] bool isSlotAvailableForAction(std::string_view action_key, std::string_view slot) const;
    [[nodiscard]] std::optional<std::string> actionKeyFromAnimationName(std::string_view animation_name) const;
    [[nodiscard]] std::optional<std::string> directionKeyFromAnimationName(std::string_view animation_name) const;
    [[nodiscard]] std::optional<LayerLayout> resolveLayerLayout(std::string_view action_key,
                                                                 std::string_view direction_key) const;

    [[nodiscard]] std::optional<LayerTexture> resolveLayerTexture(std::string_view action_key,
                                                                  std::string_view slot,
                                                                  std::string_view variant,
                                                                  std::string_view gender) const;
    [[nodiscard]] std::optional<LayerTexture> resolvePortraitLayerTexture(std::string_view layer,
                                                                          std::string_view variant,
                                                                          std::string_view gender) const;

    [[nodiscard]] std::vector<std::string> collectPreloadTexturePaths(
        const AppearanceProfile& profile,
        std::size_t runtime_variant_limit_per_slot = kPreloadAllRuntimeVariants) const;

private:
    struct ActionLayoutConfig {
        std::size_t frames_per_direction_{0};
        std::vector<std::string> direction_block_order_{};
        std::string left_fallback_{"mirror_right"};
    };

    [[nodiscard]] bool buildActionSlotAvailability();

    [[nodiscard]] std::optional<std::string> normalizeVariantForResolution(std::string_view action_key,
                                                                            std::string_view slot,
                                                                            std::string_view variant) const;
    [[nodiscard]] std::optional<std::string> resolveTexturePath(std::string_view action_dir,
                                                                 std::string_view slot_dir,
                                                                 std::string_view slot,
                                                                 std::string_view variant,
                                                                 std::string_view gender) const;
    [[nodiscard]] std::optional<std::string> resolveTexturePathFromBase(const std::filesystem::path& base_path,
                                                                        std::string_view alias_scope,
                                                                        std::string_view slot,
                                                                        std::string_view variant) const;
    [[nodiscard]] std::vector<std::string> variantPathCandidates(std::string_view alias_scope,
                                                                  std::string_view slot,
                                                                  std::string_view variant) const;

    std::string texture_root_{};
    std::string portrait_texture_root_{};
    std::string default_profile_id_{};
    std::vector<std::string> layer_order_{};
    std::vector<std::string> selection_order_{};
    std::vector<std::string> gender_variants_{"male", "female"};
    std::vector<std::string> portrait_layer_order_{"skin", "ears", "clothes", "eyes", "hair", "acc"};
    std::unordered_map<std::string, std::string> slot_dirs_{};
    std::unordered_map<std::string, std::string> action_dirs_{};
    std::unordered_map<std::string, ActionLayoutConfig> action_layouts_{};
    std::unordered_map<std::string, std::unordered_set<std::string>> action_available_slots_{};
    std::unordered_map<std::string, AppearanceProfile> profiles_{};
    std::unordered_map<std::string, std::vector<std::string>> slot_variants_{};
    std::unordered_set<std::string> runtime_switchable_slots_{};
    std::unordered_map<std::string, std::string> weapon_action_variants_{};
    std::unordered_map<std::string,
                       std::unordered_map<std::string,
                                          std::unordered_map<std::string, std::vector<std::string>>>>
        variant_path_aliases_{};
};

} // namespace game::data
