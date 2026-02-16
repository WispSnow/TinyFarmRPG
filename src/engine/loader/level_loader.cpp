#include "level_loader.h"
#include "engine/scene/scene.h"
#include "engine/core/context.h"
#include "engine/resource/resource_manager.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/component/tilelayer_component.h"
#include "engine/component/name_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/parallax_component.h"
#include "engine/component/render_component.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/render/renderer.h"
#include "engine/utils/math.h"
#include "engine/utils/scoped_timer.h"
#include "tiled_conventions.h"
#include "tiled_diagnostics.h"
#include "tiled_json_cache.h"
#include "tiled_json_helpers.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string_view>
#include <cstdint>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <SDL3/SDL_rect.h>
#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>

namespace {
using engine::loader::tiled::findMember;
using engine::loader::tiled::findStringMember;
using engine::loader::tiled::jsonBoolOr;
using engine::loader::tiled::jsonFloatOr;
using engine::loader::tiled::jsonGidOr;
using engine::loader::tiled::jsonGidValueOr;
using engine::loader::tiled::jsonIntOr;
using engine::loader::tiled::jsonIntVectorOr;
using engine::loader::tiled::jsonStringOr;
using engine::loader::tiled::jsonUInt64Or;

[[nodiscard]] bool isValidTilesetRef(const nlohmann::json& tileset_json) {
    return tileset_json.contains("source") && tileset_json["source"].is_string() &&
           tileset_json.contains("firstgid") && tileset_json["firstgid"].is_number_integer();
}
} // namespace

namespace engine::loader {

LevelLoader::LevelLoader(engine::scene::Scene& scene)
    : scene_(scene), 
    registry_(scene.getRegistry()), 
    spatial_index_manager_(scene.getContext().getSpatialIndexManager()), 
    resource_manager_(scene.getContext().getResourceManager()), 
    auto_tile_library_(resource_manager_.getAutoTileLibrary()) {}

LevelLoader::~LevelLoader() = default;

void LevelLoader::setEntityBuilder(std::unique_ptr<BasicEntityBuilder> builder) {
    entity_builder_ = std::move(builder);
}

bool LevelLoader::loadLevel(std::string_view level_path) {
    engine::utils::ScopedTimer total_timer(
        std::string("LevelLoader::loadLevel: ") + std::string(level_path),
        timing_enabled_,
        spdlog::level::info
    );

    // 确保重复调用时状态一致（尽管当前项目通常每次切图都会创建新的 LevelLoader）
    tileset_data_.clear();
    tileset_tile_indices_.clear();
    tile_info_cache_.clear();
    current_layer_index_ = 0;
    current_layer_name_.clear();

    if (!entity_builder_) {
        spdlog::info("设置默认的实体生成器");
        entity_builder_ = std::make_unique<BasicEntityBuilder>(*this, scene_.getContext(), registry_);
    }

    // 1. 获取/缓存关卡 JSON（避免切图时反复读盘与解析）
    const auto json_ptr = tiled::getOrLoadLevelJson(level_path);
    if (!json_ptr) {
        return false;
    }
    const auto& json_data = *json_ptr;

    // 2. 获取基本地图信息 (名称、地图尺寸、瓦片尺寸)，并设置背景颜色
    map_path_ = level_path;
    map_size_ = glm::ivec2(jsonIntOr(json_data, "width", 0), jsonIntOr(json_data, "height", 0));
    tile_size_ = glm::ivec2(jsonIntOr(json_data, "tilewidth", 0), jsonIntOr(json_data, "tileheight", 0));
    if (const auto* color_string = findStringMember(json_data, "backgroundcolor")) {
        auto color = engine::utils::parseHexColor(*color_string);
        scene_.getContext().getRenderer().setClearColorFloat(color);
    }

    // 3. 如果设置了空间索引标志，则初始化空间索引管理器
    if (use_spatial_index_) {
        glm::vec2 map_pixel_size = glm::vec2(map_size_) * glm::vec2(tile_size_);
        spatial_index_manager_.initialize(
            registry_,
            map_size_,
            tile_size_,
            glm::vec2(0.0f, 0.0f),  // 世界边界最小值
            map_pixel_size,         // 世界边界最大值
            glm::vec2(tile_size_.x * 4.0f, tile_size_.y * 4.0f) // 动态网格单元格大小 (通常是瓦片网格的4倍)
        );
        spdlog::info("LevelLoader: 已创建并初始化空间索引管理器");
    }

    // 4. 加载 tileset 数据
    if (json_data.contains("tilesets") && json_data["tilesets"].is_array()) {
        for (const auto& tileset_json : json_data["tilesets"]) {
            if (!isValidTilesetRef(tileset_json)) {
                spdlog::error("tilesets 对象中缺少有效 'source' 或 'firstgid' 字段。");
                continue;
            }
            auto tileset_path = resolvePath(tileset_json["source"].get<std::string>(), map_path_);
            const int first_gid = jsonIntOr(tileset_json, "firstgid", 0);
            loadTileset(tileset_path, first_gid);
        }
    }

    // 5. 加载图层数据
    if (!json_data.contains("layers") || !json_data["layers"].is_array()) {       // 地图文件中必须有 layers 数组
        spdlog::error("地图文件 '{}' 中缺少或无效的 'layers' 数组。", level_path);
        return false;
    }
    for (const auto& layer_json : json_data["layers"]) {
        // 获取各图层对象中的类型（type）字段
        const std::string layer_type = jsonStringOr(layer_json, "type", "none");
        if (!jsonBoolOr(layer_json, "visible", true)) {
            spdlog::info("图层 '{}' 不可见，跳过加载。", jsonStringOr(layer_json, "name", "Unnamed"));
            continue;
        }

        // 必须在真正加载图层前更新当前图层名，否则 Builder 会读取到上一层名称（例如 solid/collider 的渲染/精灵判断会错层）。
        current_layer_name_ = jsonStringOr(layer_json, "name", "Unnamed");

        // 可以指定当前图层的序号（默认从0开始，每载入一个图层，序号加1），这个序号用于决定渲染顺序
        if (const auto* properties = findMember(layer_json, "properties"); properties && properties->is_array()) {
            for (const auto& property : *properties) {
                const auto* name = findStringMember(property, "name");
                if (!name || *name != tiled::LAYER_PROPERTY_ORDER) {
                    continue;
                }
                current_layer_index_ = jsonIntOr(property, "value", current_layer_index_);
            }
        }

        // 根据图层类型决定加载方法
        if (layer_type == "imagelayer") {       
            loadImageLayer(layer_json);
        } else if (layer_type == "tilelayer") {
            loadTileLayer(layer_json);
        } else if (layer_type == "objectgroup") {
            loadObjectLayer(layer_json);
        } else {
            spdlog::warn("不支持的图层类型: {}", layer_type);
        }
        spdlog::info("当前图层: {}, 图层ID: {}", current_layer_name_, current_layer_index_);
        
        current_layer_index_++;   // 每加载一个图层，图层ID加1
    }

    spdlog::info("关卡加载完成: {}", level_path);
    return true;
}

bool LevelLoader::preloadLevelData(std::string_view level_path) {
    engine::utils::ScopedTimer timer(
        std::string("LevelLoader::preloadLevelData: ") + std::string(level_path),
        timing_enabled_,
        spdlog::level::info
    );

    // 预加载不创建实体，但仍会复用 resolvePath / tileset 加载逻辑
    tileset_data_.clear();
    tileset_tile_indices_.clear();
    tile_info_cache_.clear();

    const auto json_ptr = tiled::getOrLoadLevelJson(level_path);
    if (!json_ptr) {
        return false;
    }
    const auto& json_data = *json_ptr;

    map_path_ = level_path;

    // 预加载 tileset（会触发 tileset JSON 缓存 + wangsets 解析 + auto-tile 规则预热）
    if (json_data.contains("tilesets") && json_data["tilesets"].is_array()) {
        for (const auto& tileset_ref : json_data["tilesets"]) {
            if (!isValidTilesetRef(tileset_ref)) {
                spdlog::warn("preloadLevelData: tilesets 对象中缺少有效 'source' 或 'firstgid' 字段。");
                continue;
            }

            const auto tileset_path = resolvePath(tileset_ref["source"].get<std::string>(), map_path_);
            const int first_gid = jsonIntOr(tileset_ref, "firstgid", 0);
            loadTileset(tileset_path, first_gid);

            // 额外预热：tileset 单图纹理（部分 tileset 没有 wangsets 时也需要）
            if (auto ts_json_ptr = tiled::getOrLoadTilesetJson(tileset_path); ts_json_ptr) {
                const auto& ts_json = *ts_json_ptr;
                if (ts_json.contains("image") && ts_json["image"].is_string()) {
                    const auto texture_path = resolvePath(ts_json["image"].get<std::string>(), tileset_path);
                    resource_manager_.loadTexture(entt::hashed_string(texture_path.c_str()), texture_path);
                }
            }
        }
    }

    // 预热 imagelayer 使用的纹理（不创建实体）
    if (json_data.contains("layers") && json_data["layers"].is_array()) {
        for (const auto& layer_json : json_data["layers"]) {
            if (!jsonBoolOr(layer_json, "visible", true)) {
                continue;
            }
            if (jsonStringOr(layer_json, "type", "") != "imagelayer") {
                continue;
            }
            const std::string image_path = jsonStringOr(layer_json, "image", "");
            if (image_path.empty()) {
                continue;
            }
            const auto texture_path = resolvePath(image_path, map_path_);
            resource_manager_.loadTexture(entt::hashed_string(texture_path.c_str()), texture_path);
        }
    }

    return true;
}

void LevelLoader::loadImageLayer(const nlohmann::json& layer_json) {
    // 获取纹理相对路径 （会自动处理'\/\'符号）
    std::string image_path = jsonStringOr(layer_json, "image", "");
    if (image_path.empty()) {
        spdlog::error("图层 '{}' 缺少 'image' 属性。", jsonStringOr(layer_json, "name", "Unnamed"));
        return;
    }

    // 创建精灵 (在获取纹理大小时会确保纹理加载)
    auto texture_path = resolvePath(image_path, map_path_); 
    auto texture_size = resource_manager_.getTextureSize(entt::hashed_string(texture_path.c_str()), texture_path);
    auto sprite = engine::component::Sprite(texture_path, engine::utils::Rect{glm::vec2(0.0f), texture_size});
    
    // 获取图层偏移量（json中没有则代表未设置，给默认值即可）
    const glm::vec2 offset = glm::vec2(jsonFloatOr(layer_json, "offsetx", 0.0f), jsonFloatOr(layer_json, "offsety", 0.0f));
    
    // 获取视差因子及重复标志
    const glm::vec2 scroll_factor =
        glm::vec2(jsonFloatOr(layer_json, "parallaxx", 1.0f), jsonFloatOr(layer_json, "parallaxy", 1.0f));
    const glm::bvec2 repeat = glm::bvec2(jsonBoolOr(layer_json, "repeatx", false), jsonBoolOr(layer_json, "repeaty", false));
    
    // 获取图层名称
    std::string layer_name = jsonStringOr(layer_json, "name", "Unnamed");
    entt::id_type name_id = entt::hashed_string(layer_name.c_str());
    
    /*  可用类似方法获取其它各种属性，这里我们暂时用不上 */
    
    // 创建图层实体
    auto entity = registry_.create();

    // 添加组件
    registry_.emplace<engine::component::NameComponent>(entity, name_id, layer_name);
    registry_.emplace<engine::component::TransformComponent>(entity, offset);
    registry_.emplace<engine::component::ParallaxComponent>(entity, scroll_factor, repeat);
    registry_.emplace<engine::component::SpriteComponent>(entity, sprite);
    registry_.emplace<engine::component::RenderComponent>(entity, current_layer_index_);
    if (entity_builder_) {
        entity_builder_->decorateExternalEntity(entity);
    }
    /* 实体与组件创建完毕后即由registry自动管理，不需要“添加到场景”的步骤 */

    spdlog::info("加载图层: '{}' 完成", layer_name);
}

void LevelLoader::loadTileLayer(const nlohmann::json& layer_json) {
    if (!layer_json.contains("data") || !layer_json["data"].is_array()) {
        spdlog::error("图层 '{}' 缺少 'data' 属性。", jsonStringOr(layer_json, "name", "Unnamed"));
        return;
    }

    // 获取图层名称
    std::string layer_name = jsonStringOr(layer_json, "name", "Unnamed");
    entt::id_type name_id = entt::hashed_string(layer_name.c_str());
    engine::utils::ScopedTimer layer_timer(
        "LevelLoader::loadTileLayer: " + layer_name,
        timing_enabled_,
        spdlog::level::info
    );

    // 创建图层实体
    auto layer_entity = registry_.create();
    registry_.emplace<engine::component::NameComponent>(layer_entity, name_id, layer_name);
    if (entity_builder_) {
        entity_builder_->decorateExternalEntity(layer_entity);
    }

    // 准备瓦片实体vector (瓦片数量 = 地图宽度 * 地图高度)
    std::vector<entt::entity> tiles;
    tiles.reserve(map_size_.x * map_size_.y);

    // 获取图层数据 (瓦片 ID 列表)
    const auto& data = layer_json["data"];

    size_t index = 0;   // data数据的索引，它决定图块在地图中的位置
    // --- 每一个瓦片都是一个独立的entity ---
    for (const auto& gid_value : data) {
        const int gid = jsonGidValueOr(gid_value, 0);
        if (gid == 0) {
            index++;
            continue;
        }
        const auto* tile_info = getTileInfoByGid(gid);
        if (!tile_info) {
            spdlog::error("瓦片 ID 为 {} 的瓦片未找到图块集。", gid);
            index++;
            continue;
        }
        // 使用生成器创建瓦片实体
        auto tile_entity = entity_builder_->configure(index, tile_info)->build()->getEntityID();
        // 添加到vector中
        if (tile_entity != entt::null) {
            tiles.push_back(tile_entity);
            
            // 计算瓦片坐标
            int tile_x = index % map_size_.x;
            int tile_y = index / map_size_.x;
            glm::ivec2 tile_coord(tile_x, tile_y);
            
            // 添加到空间索引（如果标志为true）
            if (use_spatial_index_) {
                // 设置瓦片类型（包含碰撞信息，累加位掩码）
                spatial_index_manager_.setTileType(tile_coord, tile_info->type_);
                // 添加瓦片实体到空间索引（按图层名分桶）
                spatial_index_manager_.addTileEntity(tile_coord, tile_entity, name_id);
            }
        }
        index++;
    }

    // 最后将瓦片层组件添加到图层实体中
    registry_.emplace<engine::component::TileLayerComponent>(layer_entity, tile_size_, map_size_, tiles);

    spdlog::info("加载图层: '{}' 完成", layer_name);
}

void LevelLoader::loadObjectLayer(const nlohmann::json& layer_json) {
    if (!layer_json.contains("objects") || !layer_json["objects"].is_array()) {
        spdlog::error("对象图层 '{}' 缺少 'objects' 属性。", jsonStringOr(layer_json, "name", "Unnamed"));
        return;
    }
    // 获取对象数据
    const auto& objects = layer_json["objects"];
    // 遍历对象数据
    for (const auto& object : objects) {
        // 获取对象gid
        const int gid = jsonGidOr(object, "gid", 0);
        if (gid == 0) {     // 如果gid为0 (即不存在)，则代表自己绘制的形状
            // 配置生成器，并调用build，针对自定义形状
            (void)entity_builder_->configure(&object)->build();

        } else {        // 如果gid存在，则按照图片解析流程
            // 配置生成器，针对图片对象
            const auto* tile_info = getTileInfoByGid(gid);
            if (!tile_info) {
                spdlog::warn("对象图层 '{}' 中的对象缺少有效的 'gid' 或瓦片信息。", jsonStringOr(layer_json, "name", "Unnamed"));
                continue;
            }
            // 配置生成器，并调用build，针对图片对象
            (void)entity_builder_->configure(&object, tile_info)->build()->getEntityID();
        }
    }
}

void LevelLoader::loadTileset(std::string_view tileset_path, int first_gid) {
    engine::utils::ScopedTimer timer(
        std::string("LevelLoader::loadTileset: ") + std::string(tileset_path),
        timing_enabled_,
        spdlog::level::info
    );

    // 获取/缓存 tileset JSON（避免切图时反复读盘与解析）
    auto ts_json_ptr = tiled::getOrLoadTilesetJson(tileset_path);
    if (!ts_json_ptr) {
        return;
    }

    auto& ts_json = *ts_json_ptr;
    ts_json["file_path"] = std::string(tileset_path); // 将文件路径存储到json中，后续解析图片路径时需要

    // 如果开启了自动图块，则加载WangSets（同一 tileset 只处理一次）
    if (use_auto_tile_) {
        const bool processed = jsonBoolOr(ts_json, "_engine_auto_tile_processed", false);
        if (!processed) {
            loadWangSets(ts_json);
            ts_json["_engine_auto_tile_processed"] = true;
        }
    }

    // 将 tileset 数据保存到 tileset_data_ 中（共享缓存，避免深拷贝）
    tileset_data_.emplace(first_gid, std::move(ts_json_ptr));
    spdlog::info("Tileset 文件 '{}' 加载完成，firstgid: {}", tileset_path, first_gid);
}

void LevelLoader::loadWangSets(nlohmann::json& json) {
    if (!json.contains("wangsets") || !json["wangsets"].is_array()) {
        spdlog::info("{} 文件中没有wangsets属性。", jsonStringOr(json, "file_path", ""));
        return;
    }
    if (!json.contains("image")) {
        spdlog::error("{} 文件中缺少 'image' 属性，无法生成自动图块。", jsonStringOr(json, "file_path", ""));
        return;
    }
    // 计算纹理ID并载入纹理
    const auto* image_value = findStringMember(json, "image");
    if (!image_value || image_value->empty()) {
        spdlog::error("{} 文件中 'image' 字段格式无效，无法生成自动图块。", jsonStringOr(json, "file_path", ""));
        return;
    }
    const std::string image_path = std::string(*image_value);
    auto texture_path = resolvePath(image_path, jsonStringOr(json, "file_path", ""));
    entt::id_type texture_id = entt::hashed_string(texture_path.c_str());
    resource_manager_.loadTexture(texture_id, texture_path);
    // 创建auto_tile_lookup对象, 用于记录自动图块数据
    auto& auto_tile_lookup = json["auto_tile_lookup"];
    if (!auto_tile_lookup.is_object()) {
        auto_tile_lookup = nlohmann::json::object();
    }
    // 记录自动图块数据的函数，用于将自动图块数据记录到auto_tile_lookup中
    auto record_auto_tile = [&auto_tile_lookup](int tile_id, entt::id_type rule_id, uint8_t mask) {
        if (tile_id < 0 || rule_id == entt::null) {
            return;
        }
        nlohmann::json entry;
        entry["rule_id"] = entt::to_integral(rule_id);
        entry["mask"] = mask;
        auto_tile_lookup[std::to_string(tile_id)] = std::move(entry);
    };
    auto& wangsets = json["wangsets"];
    for (const auto& wangset : wangsets) {
        // 获取地形集名称和类型
        std::string terrains_name = jsonStringOr(wangset, "name", "");
        engine::resource::AutoTileTopology terrains_type = engine::resource::AutoTileTopology::UNKNOWN;
        const std::string wangset_type = jsonStringOr(wangset, "type", "");
        if (wangset_type == "corner") {
            terrains_type = engine::resource::AutoTileTopology::CORNER;
        } else if (wangset_type == "edge") {
            terrains_type = engine::resource::AutoTileTopology::EDGE;
        } else if (wangset_type == "mixed") {
            terrains_type = engine::resource::AutoTileTopology::MIXED;
        }
        // 获取地形名称和对应的标志数字
        const nlohmann::json* colors = findMember(wangset, "colors");
        if (!colors || !colors->is_array() || colors->empty()) {
            spdlog::error("{} 地形集 '{}' 缺少 'colors' 属性。", jsonStringOr(json, "file_path", ""), terrains_name);
            return;
        }
        std::vector<entt::id_type> terrains_ids;
        for (const auto& color : *colors) {
            // 让地形名称 = 地形集名称 + "_" + 颜色名称
            const std::string color_name = jsonStringOr(color, "name", "");
            std::string terrain_name = terrains_name + "_" + color_name;
            entt::hashed_string terrain_hash_str = entt::hashed_string(terrain_name.c_str());
            terrains_ids.push_back(terrain_hash_str.value());   // 将地形id按顺序保存到vector中
            // 插入到auto_tile_library_中
            auto_tile_library_.addRule(terrain_hash_str, engine::resource::AutoTileRule(texture_id, terrain_name));
            auto_tile_library_.setRuleTopology(terrain_hash_str.value(), terrains_type);
            // Tiled地形中缺少单独一个图块的定义（即四面八方都为空的情况），因此我们直接让设置的显示图片为对应图块
            const int default_id = jsonIntOr(color, "tile", -1);
            if (default_id >= 0) {
                auto texture_rect = getTextureRect(json, default_id);
                auto_tile_library_.setSrcRect(terrain_hash_str.value(), 0, texture_rect);
                record_auto_tile(default_id, terrain_hash_str.value(), 0);
            }
        }
        // 开始解析"wangtiles"
        const nlohmann::json* wangtiles = findMember(wangset, "wangtiles");
        if (!wangtiles || !wangtiles->is_array() || wangtiles->empty()) {
            spdlog::error("{} 地形集 '{}' 缺少 'wangtiles' 属性。", jsonStringOr(json, "file_path", ""), terrains_name);
            return;
        }
        for (const auto& wangtile : *wangtiles) {
            // 获取源矩形
            const int tileid = jsonIntOr(wangtile, "tileid", 0);
            auto texture_rect = getTextureRect(json, tileid);
            // 根据wangid确认所针对的地形，并计算Mask掩码
            /* Tiled中，wangid 的标记顺序是：上，右上，右，右下，下，左下，左，左上，我们就按此顺序规定掩码值 */
            uint8_t mask = 0;
            entt::id_type terrain_id = entt::null;
            const std::vector<int> wangid_vector = jsonIntVectorOr(wangtile, "wangid", {});
            if (wangid_vector.empty() || wangid_vector.size() != 8) {
                spdlog::error(
                    "{} 地形集 '{}' 中瓦片 {} 缺少有效的 'wangid' 属性。",
                    jsonStringOr(json, "file_path", ""),
                    terrains_name,
                    tileid
                );
                return;
            }
            for (int i = 0; i < 8; i++) {
                // 现在只考虑一种地形和“空”之间的情况，不考虑两种地形之间的过渡
                if (wangid_vector[i] != 0) {
                    mask |= 1 << i;
                    terrain_id = terrains_ids[wangid_vector[i] - 1];
                }
            }
            if (terrain_id != entt::null) {
                auto_tile_library_.setSrcRect(terrain_id, mask, texture_rect);
                record_auto_tile(tileid, terrain_id, mask);
            }
        }
        for (const auto terrain_id : terrains_ids) {
            auto_tile_library_.fillMissingMasks(terrain_id, terrains_type);
        }
    }
}

std::optional<engine::component::ColliderInfo> LevelLoader::getColliderInfo(const nlohmann::json& tile_json) {
    if (!tile_json.contains("objectgroup")) return std::nullopt;
    auto& objectgroup = tile_json["objectgroup"];
    if (!objectgroup.contains("objects")) return std::nullopt;
    auto& objects = objectgroup["objects"];
    for (const auto& object : objects) {    // 一个图片只支持一个碰撞器。如果有多个，则返回第一个不为空的
        auto rect = engine::utils::Rect(glm::vec2(jsonFloatOr(object, "x", 0.0f), jsonFloatOr(object, "y", 0.0f)),
                                        glm::vec2(jsonFloatOr(object, "width", 0.0f), jsonFloatOr(object, "height", 0.0f)));
        if (rect.size.x > 0 && rect.size.y > 0) {
            // 目前支持两种形状：圆形（椭圆当成圆形处理）和矩形
            if (jsonBoolOr(object, "ellipse", false)) {
                return engine::component::ColliderInfo(engine::component::ColliderType::CIRCLE, rect);
            } else {
                return engine::component::ColliderInfo(engine::component::ColliderType::RECT, rect);
            }
        }
    }
    return std::nullopt;    // 如果没找到碰撞器，则返回空
}

engine::utils::Rect LevelLoader::getTextureRect(const nlohmann::json& tileset_json, int local_id) {
    if (local_id < 0) {
        spdlog::error("Tileset 文件 '{}' 中瓦片 {} 的local_id小于0。", jsonStringOr(tileset_json, "file_path", ""), local_id);
        return engine::utils::Rect{};
    }
    const int columns = jsonIntOr(tileset_json, "columns", 1);
    const int tile_width = jsonIntOr(tileset_json, "tilewidth", 0);
    const int tile_height = jsonIntOr(tileset_json, "tileheight", 0);
    auto coordinate_x = local_id % columns;
    auto coordinate_y = local_id / columns;
    return engine::utils::Rect{glm::vec2(coordinate_x * tile_width, coordinate_y * tile_height), 
                               glm::vec2(tile_width, tile_height)};
}

engine::component::TileType LevelLoader::getTileType(const nlohmann::json& tile_json,
                                                     std::string_view tileset_file_path,
                                                     int tile_local_id) {
    engine::component::TileType result = engine::component::TileType::NORMAL;

    const auto* properties = findMember(tile_json, "properties");
    if (!properties || !properties->is_array()) {
        return result;
    }

    for (const auto& property : *properties) {
        const auto* name = findStringMember(property, "name");
        if (!name || *name != tiled::TILE_PROPERTY_TILE_FLAG) {
            continue;
        }

        const auto* value = findStringMember(property, "value");
        if (!value || value->empty()) {
            continue;
        }

        std::stringstream ss{std::string(*value)};
        std::string flag;
        while (std::getline(ss, flag, ',')) {
            flag.erase(flag.begin(), std::find_if(flag.begin(), flag.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            flag.erase(std::find_if(flag.rbegin(), flag.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), flag.end());

            if (flag.empty()) {
                continue;
            }

            using engine::component::TileType;
            constexpr std::pair<std::string_view, TileType> FLAG_TABLE[] = {
                {tiled::TILE_FLAG_BLOCK_NORTH, TileType::BLOCK_N},
                {tiled::TILE_FLAG_BLOCK_SOUTH, TileType::BLOCK_S},
                {tiled::TILE_FLAG_BLOCK_WEST,  TileType::BLOCK_W},
                {tiled::TILE_FLAG_BLOCK_EAST,  TileType::BLOCK_E},
                {tiled::TILE_FLAG_HAZARD,      TileType::HAZARD},
                {tiled::TILE_FLAG_WATER,       TileType::WATER},
                {tiled::TILE_FLAG_INTERACT,    TileType::INTERACT},
                {tiled::TILE_FLAG_ARABLE,      TileType::ARABLE},
                {tiled::TILE_FLAG_SOLID,       TileType::SOLID},
                {tiled::TILE_FLAG_OCCUPIED,    TileType::OCCUPIED},
            };

            bool matched = false;
            for (const auto& entry : FLAG_TABLE) {
                if (flag == entry.first) {
                    TileType tile_flag = entry.second;
                    result |= tile_flag;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                tiled::diagnostics::recordUnknownTileFlagToken(tileset_file_path, tile_local_id, flag);
                spdlog::warn("LevelLoader: tileset '{}' tile {} has unknown tile_flag token '{}'",
                             tileset_file_path,
                             tile_local_id,
                             flag);
            }
        }
        break;
    }

    return result;
}

const nlohmann::json* LevelLoader::findTileJson(const nlohmann::json& tileset_json, int first_gid, int local_id) {
    if (!tileset_json.contains("tiles") || !tileset_json["tiles"].is_array()) {
        return nullptr;
    }

    auto& index = tileset_tile_indices_[first_gid];
    if (!index.built) {
        const auto& tiles = tileset_json["tiles"];
        index.id_to_index.reserve(tiles.size());
        for (std::size_t i = 0; i < tiles.size(); ++i) {
            const auto& tile = tiles[i];
            if (!tile.contains("id") || !tile["id"].is_number_integer()) {
                continue;
            }
            const int id = jsonIntOr(tile, "id", -1);
            if (id < 0) {
                continue;
            }
            index.id_to_index.emplace(id, i);
        }
        index.built = true;
    }

    const auto it = index.id_to_index.find(local_id);
    if (it == index.id_to_index.end()) {
        return nullptr;
    }

    const auto& tiles = tileset_json["tiles"];
    if (it->second >= tiles.size()) {
        return nullptr;
    }
    return &tiles.at(it->second);
}

engine::component::TileType LevelLoader::getTileTypeById(const nlohmann::json& tileset_json, int first_gid, int local_id) {
    if (const auto* tile_json = findTileJson(tileset_json, first_gid, local_id)) {
        const std::string file_path = jsonStringOr(tileset_json, "file_path", "");
        return getTileType(*tile_json, file_path, local_id);
    }
    return engine::component::TileType::NORMAL;
}

bool LevelLoader::parseSingleImageSprite(const nlohmann::json& tileset, int first_gid, int local_id,
                                          bool is_flipped, std::string_view file_path,
                                          engine::component::TileInfo& out) {
    if (!tileset["image"].is_string()) {
        spdlog::error("Tileset '{}' 的 'image' 字段不是字符串。", file_path);
        return false;
    }

    const auto texture_rect = getTextureRect(tileset, local_id);
    const auto image_path = tileset["image"].get<std::string>();
    const auto texture_path = resolvePath(image_path, file_path);
    out.sprite_ = engine::component::Sprite(texture_path, texture_rect, is_flipped);
    out.type_ = getTileTypeById(tileset, first_gid, local_id);
    return true;
}

bool LevelLoader::parseMultiImageSprite(const nlohmann::json* tile_json, int local_id,
                                         bool is_flipped, std::string_view file_path,
                                         engine::component::TileInfo& out) {
    if (!tile_json) {
        spdlog::error("Tileset '{}' 缺少 tiles[id={}] 定义，无法解析多图片瓦片。", file_path, local_id);
        return false;
    }
    if (!tile_json->contains("image")) {
        spdlog::error("Tileset '{}' 中瓦片 {} 缺少 'image' 属性。", file_path, local_id);
        return false;
    }

    const auto* image_path = findStringMember(*tile_json, "image");
    if (!image_path || image_path->empty()) {
        spdlog::error("Tileset '{}' 中瓦片 {} 的 'image' 字段不是字符串。", file_path, local_id);
        return false;
    }

    const auto texture_path = resolvePath(*image_path, file_path);
    const int image_width = jsonIntOr(*tile_json, "imagewidth", 0);
    const int image_height = jsonIntOr(*tile_json, "imageheight", 0);
    const engine::utils::Rect texture_rect = {
        glm::vec2(jsonFloatOr(*tile_json, "x", 0.0f), jsonFloatOr(*tile_json, "y", 0.0f)),
        glm::vec2(jsonFloatOr(*tile_json, "width", static_cast<float>(image_width)),
                  jsonFloatOr(*tile_json, "height", static_cast<float>(image_height)))
    };

    out.sprite_ = engine::component::Sprite(texture_path, texture_rect, is_flipped);
    resource_manager_.loadTexture(entt::hashed_string(texture_path.c_str()), texture_path);
    out.type_ = getTileType(*tile_json, file_path, local_id);
    return true;
}

void LevelLoader::parseTileExtras(const nlohmann::json& tileset, const nlohmann::json* tile_json,
                                   bool is_single_image, int local_id,
                                   engine::component::TileInfo& out) {
    // 解析可能存在的自动图块数据
    if (auto lookup_it = tileset.find("auto_tile_lookup"); lookup_it != tileset.end() && lookup_it->is_object()) {
        const std::string local_id_key = std::to_string(local_id);
        if (auto entry_it = lookup_it->find(local_id_key); entry_it != lookup_it->end()) {
            const auto& entry = entry_it.value();
            const auto rule_id_raw = jsonUInt64Or(entry, "rule_id", 0ull);
            if (rule_id_raw != 0ull) {
                engine::component::AutoTileData auto_tile_data{};
                auto_tile_data.rule_id_ = static_cast<entt::id_type>(rule_id_raw);
                auto_tile_data.mask_ = static_cast<std::uint8_t>(jsonIntOr(entry, "mask", 0));
                out.auto_tile_ = auto_tile_data;
            }
        }
    }

    // 补充 tile_json 相关信息（动画/碰撞/属性）
    if (tile_json) {
        // 动画信息（瓦片动画为animation字段，且必须为数组，目前只考虑单一图片情况）
        if (is_single_image && tile_json->contains("animation") && (*tile_json)["animation"].is_array()) {
            std::vector<engine::component::AnimationFrame> animation_frames;
            auto& animation = (*tile_json)["animation"];
            animation_frames.reserve(animation.size());
            for (auto& frame : animation) {
                const float duration_ms = jsonFloatOr(frame, "duration", 100.0f);
                const int id = jsonIntOr(frame, "tileid", 0);
                const auto frame_rect = getTextureRect(tileset, id);
                animation_frames.emplace_back(frame_rect, duration_ms);
            }
            if (!animation_frames.empty()) {
                const glm::vec2 dst_size = animation_frames.front().src_rect_.size;
                out.animation_ = engine::component::Animation("default", dst_size, std::move(animation_frames));
            }
        }

        // 碰撞器信息
        if (tile_json->contains("objectgroup")) {
            if (auto collider_info = getColliderInfo(*tile_json)) {
                out.collider_ = collider_info;
            }
        }

        // 属性信息
        if (tile_json->contains("properties")) {
            out.properties_ = (*tile_json)["properties"];
        }
    }
}

const engine::component::TileInfo* LevelLoader::getTileInfoByGid(int gid) {
    if (gid == 0) {
        return nullptr;
    }

    const std::uint32_t gid_raw = static_cast<std::uint32_t>(gid);
    const bool is_flipped_horizontally = (gid_raw & 0x80000000u) != 0u;

    // 还原gid的实际值 (最高的三个标志位置为0，而其余位全为1。这个掩码的十六进制表示为 0x1FFFFFFF。)
    const int gid_value = static_cast<int>(gid_raw & 0x1FFFFFFFu);
    const std::uint64_t cache_key = (static_cast<std::uint64_t>(gid_value) << 1u) | (is_flipped_horizontally ? 1u : 0u);

    if (auto it = tile_info_cache_.find(cache_key); it != tile_info_cache_.end()) {
        return &it->second;
    }

    // upper_bound：查找tileset_data_中键大于 gid 的第一个元素，返回迭代器
    auto tileset_it = tileset_data_.upper_bound(gid_value);
    if (tileset_it == tileset_data_.begin()) {
        spdlog::error("gid为 {} 的瓦片未找到图块集。", gid_value);
        return nullptr;
    }
    --tileset_it; // 前移一个位置，这样就得到不大于gid的最近一个元素（我们需要的）

    const int first_gid = tileset_it->first;
    const auto& tileset = *tileset_it->second;
    const int local_id = gid_value - first_gid; // 计算瓦片在图块集中的局部ID
    const std::string file_path = jsonStringOr(tileset, "file_path", "");
    if (file_path.empty()) {
        spdlog::error("Tileset(firstgid={}) 缺少 'file_path' 属性。", first_gid);
        return nullptr;
    }

    engine::component::TileInfo tile_info{};
    const bool is_single_image = tileset.contains("image");
    const nlohmann::json* tile_json = findTileJson(tileset, first_gid, local_id);

    if (is_single_image) {
        if (!parseSingleImageSprite(tileset, first_gid, local_id, is_flipped_horizontally, file_path, tile_info)) {
            return nullptr;
        }
    } else {
        if (!parseMultiImageSprite(tile_json, local_id, is_flipped_horizontally, file_path, tile_info)) {
            return nullptr;
        }
    }

    parseTileExtras(tileset, tile_json, is_single_image, local_id, tile_info);

    auto [it, inserted] = tile_info_cache_.emplace(cache_key, std::move(tile_info));
    return &it->second;
}

std::string LevelLoader::resolvePath(std::string_view relative_path, std::string_view file_path) {
    // 获取地图文件的父目录（相对于可执行文件） "assets/maps/level1.tmj" -> "assets/maps"
    const auto map_dir = std::filesystem::path(file_path).parent_path();
    // 合并路径（相对于可执行文件）
    const auto combined_path = map_dir / relative_path;

    // 规范化路径：解析路径中的当前目录（.）和上级目录（..）导航符，得到相对于可执行文件的干净路径
    // 使用 lexically_normal 而不是 canonical，因为 canonical 需要文件存在且会返回绝对路径
    return combined_path.lexically_normal().string();
}

} // namespace engine::loader
