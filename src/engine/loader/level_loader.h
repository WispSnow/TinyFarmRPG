#pragma once
#include "engine/utils/math.h"
#include "basic_entity_builder.h"
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>
#include <entt/entity/fwd.hpp>
#include <SDL3/SDL_rect.h>
#include <map>
#include <unordered_map>
#include <cstdint>

namespace engine::component {
    enum class TileType;
    struct ColliderInfo;
    struct TileInfo;
}

namespace engine::scene {
    class Scene;
}

namespace engine::resource {
    class ResourceManager;
    class AutoTileLibrary;
}

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace engine::loader {

/**
 * 关卡加载器，负责加载关卡数据，并生成游戏实体
 */
class LevelLoader final {
    friend class BasicEntityBuilder;
private:
    engine::scene::Scene& scene_;       ///< @brief 场景引用
    // 缓存一些常用模块，避免重复获取
    entt::registry& registry_;       ///< @brief 注册表引用
    engine::spatial::SpatialIndexManager& spatial_index_manager_;  ///< @brief 空间索引管理器引用
    engine::resource::ResourceManager& resource_manager_;          ///< @brief 资源管理器引用
    engine::resource::AutoTileLibrary& auto_tile_library_;        ///< @brief 自动图块库引用

    std::string map_path_;              ///< @brief 地图路径（拼接路径时需要）
    glm::ivec2 map_size_;               ///< @brief 地图尺寸(瓦片数量)
    glm::ivec2 tile_size_;              ///< @brief 瓦片尺寸(像素)

    bool use_spatial_index_{true};     ///< @brief 是否使用空间索引
    bool use_auto_tile_{true};         ///< @brief 是否使用自动图块

    std::map<int, std::shared_ptr<nlohmann::json>> tileset_data_; ///< @brief firstgid -> 瓦片集数据(共享缓存)

    std::unique_ptr<BasicEntityBuilder> entity_builder_;    ///< @brief 实体生成器(生成器模式)

    int current_layer_index_ = 0;      ///< @brief 当前图层序号（用于RenderComponent，决定渲染顺序）
    std::string current_layer_name_;   ///< @brief 当前图层名称(用于某些检测判断，例如碰撞层)

    bool timing_enabled_{false};      ///< @brief 是否输出加载耗时日志

    struct TilesetTileIndex {
        bool built{false};
        std::unordered_map<int, std::size_t> id_to_index{};
    };

    std::unordered_map<int, TilesetTileIndex> tileset_tile_indices_{}; ///< @brief firstgid -> (local_id -> tiles[index])
    std::unordered_map<std::uint64_t, engine::component::TileInfo> tile_info_cache_{}; ///< @brief gid(+flip) -> TileInfo


public:
    explicit LevelLoader(engine::scene::Scene& scene);
    ~LevelLoader();

    /// @brief 设置实体生成器（如果不设置，则使用默认的BasicEntityBuilder）
    void setEntityBuilder(std::unique_ptr<BasicEntityBuilder> builder);

    /**
     * @brief 加载关卡数据，并生成游戏实体
     * @param level_path 关卡文件路径（.tmj）
     * @return true 加载成功，false 加载失败
     */
    [[nodiscard]] bool loadLevel(std::string_view level_path);
    /// @brief 预加载关卡相关 JSON/tileset/纹理/自动图块规则（不创建实体）
    [[nodiscard]] bool preloadLevelData(std::string_view level_path);

    // --- getters and setters ---
    void setUseSpatialIndex(bool use) { use_spatial_index_ = use; }
    void setUseAutoTile(bool use) { use_auto_tile_ = use; }
    void setTimingEnabled(bool enabled) { timing_enabled_ = enabled; }
    const glm::ivec2& getMapSize() const { return map_size_; }
    const glm::ivec2& getTileSize() const { return tile_size_; }
    int getCurrentLayer() const { return current_layer_index_; }
    const std::string& getCurrentLayerName() const { return current_layer_name_; }

private:

    void loadImageLayer(const nlohmann::json& layer_json);    ///< @brief 加载图片图层
    void loadTileLayer(const nlohmann::json& layer_json);     ///< @brief 加载瓦片图层
    void loadObjectLayer(const nlohmann::json& layer_json);   ///< @brief 加载对象图层

    /**
    * @brief 加载 Tiled tileset 文件 (.tsj)，数据保存到tileset_data_。
    * @param tileset_path Tileset 文件路径。
    * @param first_gid 此 tileset 的第一个全局 ID。
    */
    void loadTileset(std::string_view tileset_path, int first_gid);

    /**
     * @brief 加载 Tiled wangsets 数据，保存到auto_tile_library_中。
     * @param json tileset json数据。
     */
    void loadWangSets(nlohmann::json& json);

    /**
     * @brief 获取瓦片碰撞器矩形
     * @param tile_json 瓦片json数据
     * @return 碰撞器矩形，如果碰撞器不存在则返回 std::nullopt
     */
    std::optional<engine::component::ColliderInfo> getColliderInfo(const nlohmann::json& tile_json);

    /**
     * @brief 获取瓦片纹理矩形（只针对单一图片图块集）
     * @param tileset_json 图块集json数据
     * @param local_id 图块集中的id
     * @return 纹理矩形
     */
    engine::utils::Rect getTextureRect(const nlohmann::json& tileset_json, int local_id);

    /**
     * @brief 根据瓦片json对象获取瓦片类型
     * @param tile_json 瓦片json数据
     * @return 瓦片类型
     */
    engine::component::TileType getTileType(const nlohmann::json& tile_json,
                                            std::string_view tileset_file_path,
                                            int tile_local_id);

    /**
     * @brief 根据图块集中的id获取瓦片类型
     * @param tileset_json 图块集json数据
     * @param local_id 图块集中的id
     * @return 瓦片类型
     */
    engine::component::TileType getTileTypeById(const nlohmann::json& tileset_json, int first_gid, int local_id);

    /**
     * @brief 根据全局 ID 获取瓦片信息。
     * @param gid 全局 ID。
     * @return engine::component::TileInfo 瓦片信息。
     */
    const engine::component::TileInfo* getTileInfoByGid(int gid);

    /// @brief 解析单图 tileset 的精灵与类型信息
    [[nodiscard]] bool parseSingleImageSprite(const nlohmann::json& tileset, int first_gid, int local_id,
                                              bool is_flipped, std::string_view file_path,
                                              engine::component::TileInfo& out);
    /// @brief 解析多图 tileset 的精灵与类型信息
    [[nodiscard]] bool parseMultiImageSprite(const nlohmann::json* tile_json, int local_id,
                                             bool is_flipped, std::string_view file_path,
                                             engine::component::TileInfo& out);
    /// @brief 解析 auto-tile、动画、碰撞器、属性等补充信息
    void parseTileExtras(const nlohmann::json& tileset, const nlohmann::json* tile_json,
                         bool is_single_image, int local_id,
                         engine::component::TileInfo& out);

    /**
     * @brief 解析图片路径，合并地图路径和相对路径。例如：
     * 1. 文件路径："assets/maps/level1.tmj"
     * 2. 相对路径："../textures/Layers/back.png"
     * 3. 最终路径："assets/textures/Layers/back.png"
     * @param relative_path 相对路径（相对于文件）
     * @param file_path 文件路径
     * @return std::string 解析后的完整路径。
     */
    std::string resolvePath(std::string_view relative_path, std::string_view file_path);

    [[nodiscard]] const nlohmann::json* findTileJson(const nlohmann::json& tileset_json, int first_gid, int local_id);
};

} // namespace engine::loader
