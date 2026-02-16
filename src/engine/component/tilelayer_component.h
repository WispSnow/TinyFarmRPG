#pragma once
#include "animation_component.h"
#include "sprite_component.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

namespace engine::component {

/**
 * @brief 定义瓦片的类型，用于游戏逻辑（例如碰撞）。
 */
 enum class TileType {
    NORMAL      = 0,      // 正常图块，可通行（默认值）
    // === 物理碰撞层 (0-3位) ===
    // 表示该方向被阻挡，无法通行
    BLOCK_N     = 1 << 0, // (1)  上方不可行
    BLOCK_S     = 1 << 1, // (2)  下方不可行
    BLOCK_W     = 1 << 2, // (4)  左方不可行
    BLOCK_E     = 1 << 3, // (8)  右方不可行

    // === 逻辑/属性层 (4-7位) ===
    HAZARD      = 1 << 4, // (16) 危险区域（陷阱/扣血）
    WATER       = 1 << 5, // (32) 水域（可能需要游泳或船）
    INTERACT    = 1 << 6, // (64) 可交互（如告示牌）
    ARABLE      = 1 << 7, // (128) 可耕作（可种植农作物）
    OCCUPIED    = 1 << 8, // (256) 被占用（例如道路），不可耕作但非物理阻挡

    // === 常用组合 ===
    SOLID       = BLOCK_N | BLOCK_S | BLOCK_W | BLOCK_E, // 完全墙体
    _entt_enum_as_bitmask   ///< @brief 启用位掩码支持
};

[[nodiscard]] inline constexpr std::uint32_t tileTypeBits(TileType value) noexcept {
    return static_cast<std::uint32_t>(value);
}

inline constexpr TileType operator|(TileType a, TileType b) noexcept {
    return static_cast<TileType>(tileTypeBits(a) | tileTypeBits(b));
}
inline constexpr TileType& operator|=(TileType& a, TileType b) noexcept {
    return a = a | b;
}
inline constexpr TileType operator&(TileType a, TileType b) noexcept {
    return static_cast<TileType>(tileTypeBits(a) & tileTypeBits(b));
}
inline constexpr TileType& operator&=(TileType& a, TileType b) noexcept {
    return a = a & b;
}
inline constexpr TileType operator~(TileType a) noexcept {
    return static_cast<TileType>(~tileTypeBits(a));
}

[[nodiscard]] inline constexpr bool hasTileFlag(TileType value, TileType flag) noexcept {
    return (tileTypeBits(value) & tileTypeBits(flag)) != 0;
}

[[nodiscard]] inline constexpr bool hasAllTileFlags(TileType value, TileType mask) noexcept {
    const auto bits = tileTypeBits(mask);
    return (tileTypeBits(value) & bits) == bits;
}

/**
 * @brief 碰撞器类型
 */
enum class ColliderType {
    RECT,   /// @brief 矩形碰撞器
    CIRCLE, /// @brief 圆形碰撞器 (椭圆当成圆形处理)
};

/**
 * @brief 碰撞器信息
 */
struct ColliderInfo {
    ColliderType type_{ColliderType::RECT};
    engine::utils::Rect rect_{};  /// @brief 碰撞器外包围矩形尺寸
};

struct AutoTileData {
    entt::id_type rule_id_{entt::null};   ///< @brief 自动图块规则ID
    uint8_t mask_{0};                     ///< @brief 初始Mask
};

/**
 * @brief 瓦片信息，包含精灵、类型、动画、属性以及自动图块信息。
 * @note 它只是辅助LevelLoader解析的临时数据，不会保存在游戏中。
 */
struct TileInfo {
    engine::component::Sprite sprite_;                      ///< @brief 精灵
    engine::component::TileType type_;                      ///< @brief 类型
    std::optional<engine::component::Animation> animation_; ///< @brief 动画（支持Tiled动画图块）
    std::optional<engine::component::ColliderInfo> collider_; ///< @brief 碰撞器信息
    std::optional<nlohmann::json> properties_;              ///< @brief 属性（存放自定义属性，方便LevelLoader解析）
    std::optional<AutoTileData> auto_tile_;                 ///< @brief 自动图块数据

};

/**
 * @brief 瓦片层组件，包含瓦片大小、地图大小和瓦片实体列表。
 * @note 现在瓦片层更像一个容器，只是存储所有的“瓦片”，而每个瓦片就是一个实体。
 */
struct TileLayerComponent {
    glm::ivec2 tile_size_;              ///< @brief 瓦片大小
    glm::ivec2 map_size_;               ///< @brief 地图大小
    std::vector<entt::entity> tiles_;   ///< @brief 瓦片实体“稀疏列表”，仅包含 gid!=0 的瓦片实体（不保证长度等于 map_size.x*map_size.y）

    /**
     * @brief 构造函数
     * @param tile_size 瓦片大小
     * @param map_size 地图大小
     * @param tiles 瓦片实体列表
     */
    TileLayerComponent(glm::ivec2 tile_size, 
                       glm::ivec2 map_size, 
                       std::vector<entt::entity> tiles) :
                       tile_size_(tile_size),
                       map_size_(map_size),
                       tiles_(std::move(tiles)) {}
};

} // namespace engine::component
