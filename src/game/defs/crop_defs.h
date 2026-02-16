#pragma once

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <string_view>

namespace game::defs {

/// @brief 作物类型枚举
enum class CropType {
    Strawberry,  ///< 草莓
    Potato,         ///< 土豆
    // 后续可扩展更多作物类型
    Unknown,
};

/// @brief 生长阶段枚举
enum class GrowthStage {
    Seed,       ///< 种子阶段
    Sprout,         ///< 发芽阶段
    Growing,  ///< 生长阶段
    Mature,   ///< 成熟阶段
    Unknown,
};

/// @brief 从 CropType 转换为字符串
[[nodiscard]] inline const char* cropTypeToString(CropType type) {
    switch (type) {
        case CropType::Strawberry: return "strawberry";
        case CropType::Potato: return "potato";
        case CropType::Unknown: return "unknown";
    }
    return "unknown";
}

/// @brief 从字符串转换为CropType
inline CropType cropTypeFromString(std::string_view str) {
    if (str == "strawberry") return CropType::Strawberry;
    if (str == "potato") return CropType::Potato;
    return CropType::Unknown;  // 默认值
}

/// @brief 从 CropType 转换为用于 Blueprint/配置查找的 ID（hash string）
[[nodiscard]] inline entt::id_type cropTypeToId(CropType crop_type) {
    switch (crop_type) {
        case CropType::Strawberry: return entt::hashed_string{"strawberry"}.value();
        case CropType::Potato: return entt::hashed_string{"potato"}.value();
        case CropType::Unknown: return entt::null;
    }
    return entt::null;
}

/// @brief 从 GrowthStage 转换为字符串
[[nodiscard]] inline const char* growthStageToString(GrowthStage stage) {
    switch (stage) {
        case GrowthStage::Seed: return "seed";
        case GrowthStage::Sprout: return "sprout";
        case GrowthStage::Growing: return "growing";
        case GrowthStage::Mature: return "mature";
        case GrowthStage::Unknown: return "unknown";
    }
    return "unknown";
}

/// @brief 从字符串转换为GrowthStage
inline GrowthStage growthStageFromString(std::string_view str) {
    if (str == "seed") return GrowthStage::Seed;
    if (str == "sprout") return GrowthStage::Sprout;
    if (str == "growing") return GrowthStage::Growing;
    if (str == "mature") return GrowthStage::Mature;
    return GrowthStage::Unknown;  // 默认值
}

} // namespace game::defs
