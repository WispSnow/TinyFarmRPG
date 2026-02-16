#include "save_service.h"

#include "game/world/map_snapshot_serializer.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"

#include "game/component/actor_component.h"
#include "game/component/crop_component.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/component/resource_node_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/defs/constants.h"
#include "game/defs/render_layers.h"
#include "game/defs/spatial_layers.h"
#include "game/defs/events.h"

#include "engine/component/auto_tile_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/resource/resource_manager.h"

#include "game/factory/blueprint_manager.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <unordered_set>

using namespace entt::literals;

namespace game::save {

namespace {

constexpr entt::id_type RULE_SOIL_TILLED = game::defs::auto_tile_rule::SOIL_TILLED;
constexpr entt::id_type RULE_SOIL_WET    = game::defs::auto_tile_rule::SOIL_WET;

constexpr std::array<Vec2i, 8> NEIGHBOR_OFFSETS{{
    {0, -1},  // up
    {1, -1},  // up-right
    {1, 0},   // right
    {1, 1},   // down-right
    {0, 1},   // down
    {-1, 1},  // down-left
    {-1, 0},  // left
    {-1, -1}  // up-left
}};

std::uint8_t computeAutoTileMask(const std::unordered_set<std::int64_t>& tiles, Vec2i tile) {
    std::uint8_t mask = 0;
    for (std::size_t i = 0; i < NEIGHBOR_OFFSETS.size(); ++i) {
        const auto& offset = NEIGHBOR_OFFSETS[i];
        const std::int64_t key = (static_cast<std::int64_t>(tile.x + offset.x) << 32) |
                                 static_cast<std::uint32_t>(tile.y + offset.y);
        if (tiles.contains(key)) {
            mask |= static_cast<std::uint8_t>(1u << i);
        }
    }
    return mask;
}

Vec2i worldToTile(glm::vec2 world_pos) {
    return Vec2i{
        static_cast<int>(std::floor(world_pos.x / game::defs::TILE_SIZE)),
        static_cast<int>(std::floor(world_pos.y / game::defs::TILE_SIZE)),
    };
}

glm::vec2 tileToWorld(Vec2i tile) {
    return glm::vec2{
        static_cast<float>(tile.x) * game::defs::TILE_SIZE,
        static_cast<float>(tile.y) * game::defs::TILE_SIZE,
    };
}

std::string resourceNodeTypeToString(game::component::ResourceNodeType type) {
    switch (type) {
        case game::component::ResourceNodeType::Tree: return "tree";
        case game::component::ResourceNodeType::Rock: return "rock";
        case game::component::ResourceNodeType::Unknown: return "unknown";
    }
    return "unknown";
}

game::component::ResourceNodeType resourceNodeTypeFromString(std::string_view str) {
    if (str == "tree") return game::component::ResourceNodeType::Tree;
    if (str == "rock") return game::component::ResourceNodeType::Rock;
    return game::component::ResourceNodeType::Unknown;
}

game::component::Action actionFromString(std::string_view str) {
    if (str == "idle") return game::component::Action::Idle;
    if (str == "walk") return game::component::Action::Walk;
    if (str == "hoe") return game::component::Action::Hoe;
    if (str == "watering") return game::component::Action::Watering;
    if (str == "planting") return game::component::Action::Planting;
    if (str == "sickle") return game::component::Action::Sickle;
    if (str == "pickaxe") return game::component::Action::Pickaxe;
    if (str == "axe") return game::component::Action::Axe;
    if (str == "sleep") return game::component::Action::Sleep;
    if (str == "eat") return game::component::Action::Eat;
    return game::component::Action::Idle;
}

game::component::Direction directionFromString(std::string_view str) {
    if (str == "up") return game::component::Direction::Up;
    if (str == "down") return game::component::Direction::Down;
    if (str == "left") return game::component::Direction::Left;
    if (str == "right") return game::component::Direction::Right;
    return game::component::Direction::Down;
}

bool buildAutoTileSprite(const engine::resource::AutoTileLibrary& library,
                         entt::id_type rule_id,
                         std::uint8_t mask,
                         engine::component::SpriteComponent& out) {
    const auto* rule = library.getRule(rule_id);
    if (!rule) {
        return false;
    }

    const engine::utils::Rect* rect = library.getSrcRect(rule_id, mask);
    if (!rect) {
        rect = library.getSrcRect(rule_id, 0);
    }
    engine::utils::Rect fallback_rect{glm::vec2(0.0f), glm::vec2(game::defs::TILE_SIZE, game::defs::TILE_SIZE)};
    const engine::utils::Rect chosen = rect ? *rect : fallback_rect;

    out = engine::component::SpriteComponent(engine::component::Sprite(rule->texture_id_, chosen));
    return true;
}

std::optional<std::vector<ResourceNodeSaveData>> convertPendingResourceNodes(
    const std::vector<game::world::ResourceNodeState>& pending) {
    std::vector<ResourceNodeSaveData> nodes;
    nodes.reserve(pending.size());
    for (const auto& node : pending) {
        nodes.push_back(ResourceNodeSaveData{
            Vec2i{node.tile_coord.x, node.tile_coord.y},
            resourceNodeTypeToString(node.type_),
            node.hit_count_,
            0,
            0,
        });
    }
    return nodes;
}

} // namespace

SaveService::SaveService(engine::core::Context& context,
                         entt::registry& registry,
                         game::world::WorldState& world_state,
                         game::world::MapManager& map_manager,
                         game::factory::BlueprintManager& blueprint_manager)
    : context_(context),
      registry_(registry),
      world_state_(world_state),
      map_manager_(map_manager),
      blueprint_manager_(blueprint_manager) {
}

std::filesystem::path SaveService::slotPath(int slot) {
    slot = std::clamp(slot, 0, 9);
    return std::filesystem::path("saves") / ("slot" + std::to_string(slot) + ".json");
}

bool SaveService::saveToFile(const std::filesystem::path& file_path, std::string& out_error) {
    out_error.clear();
    map_manager_.snapshotCurrentMap();

    SaveData data = capture(out_error);
    if (!out_error.empty()) {
        return false;
    }

    const nlohmann::json json = serialize(data);

    std::error_code ec;
    const auto dir = file_path.parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            out_error = "创建存档目录失败: " + ec.message();
            return false;
        }
    }

    auto tmp_path = file_path;
    tmp_path += ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            out_error = "无法写入临时存档文件: " + tmp_path.string();
            return false;
        }
        out << json.dump(2);
        out.flush();
        if (!out.good()) {
            out_error = "写入临时存档文件失败: " + tmp_path.string();
            return false;
        }
    }

    std::filesystem::rename(tmp_path, file_path, ec);
    if (ec) {
        std::error_code remove_ec;
        std::filesystem::remove(file_path, remove_ec);
        ec.clear();
        std::filesystem::rename(tmp_path, file_path, ec);
        if (ec) {
            out_error = "替换存档文件失败: " + ec.message();
            return false;
        }
    }

    return true;
}

bool SaveService::loadFromFile(const std::filesystem::path& file_path, std::string& out_error) {
    out_error.clear();

    std::ifstream in(file_path);
    if (!in.is_open()) {
        out_error = "无法打开存档文件: " + file_path.string();
        return false;
    }

    nlohmann::json json;
    try {
        in >> json;
    } catch (const std::exception& e) {
        out_error = std::string("解析存档 JSON 失败: ") + e.what();
        return false;
    }

    SaveData data{};
    std::string parse_error;
    if (!deserialize(json, data, parse_error)) {
        out_error = std::move(parse_error);
        return false;
    }

    return apply(data, out_error);
}

SaveData SaveService::capture(std::string& out_error) const {
    SaveData out{};
    out.schema_version = SAVE_SCHEMA_VERSION;

    const std::time_t now = std::time(nullptr);
    if (now > 0) {
        out.timestamp = std::to_string(static_cast<std::uint64_t>(now));
    }

    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        out_error = "registry.ctx() 缺少 GameTime";
        return out;
    }
    out.game_time.day = game_time->day_;
    out.game_time.hour = game_time->hour_;
    out.game_time.minute = game_time->minute_;
    out.game_time.time_scale = game_time->time_scale_;
    out.game_time.paused = game_time->paused_;

    auto player_view = registry_.view<game::component::PlayerTag>();
    if (player_view.empty()) {
        out_error = "未找到玩家实体";
        return out;
    }
    const entt::entity player = *player_view.begin();

    const auto* transform = registry_.try_get<engine::component::TransformComponent>(player);
    if (!transform) {
        out_error = "玩家缺少 TransformComponent";
        return out;
    }

    const entt::id_type current_map = map_manager_.currentMapId();
    const auto* map_state = world_state_.getMapState(current_map);
    if (!map_state) {
        out_error = "无法获取当前地图信息";
        return out;
    }

    out.player.map_name = map_state->info.name;
    out.player.position = Vec2f{transform->position_.x, transform->position_.y};

    if (const auto* state = registry_.try_get<game::component::StateComponent>(player)) {
        out.player.state.action = std::string(game::component::actionToString(state->action_));
        out.player.state.direction = std::string(game::component::directionToString(state->direction_));
    }

    if (const auto* inv = registry_.try_get<game::component::InventoryComponent>(player)) {
        out.player.inventory.active_page = inv->active_page_;
        out.player.inventory.slots.clear();
        out.player.inventory.slots.reserve(inv->slots_.size());
        for (const auto& slot : inv->slots_) {
            out.player.inventory.slots.push_back(ItemStackSaveData{
                static_cast<std::uint32_t>(slot.item_id_),
                slot.count_,
            });
        }
    } else {
        out_error = "玩家缺少 InventoryComponent";
        return out;
    }

    if (const auto* hotbar = registry_.try_get<game::component::HotbarComponent>(player)) {
        out.player.hotbar.active_slot = hotbar->active_slot_index_;
        out.player.hotbar.inventory_slot_indices.clear();
        out.player.hotbar.inventory_slot_indices.reserve(hotbar->slots_.size());
        for (const auto& slot : hotbar->slots_) {
            out.player.hotbar.inventory_slot_indices.push_back(slot.inventory_slot_index_);
        }
    } else {
        out_error = "玩家缺少 HotbarComponent";
        return out;
    }

    out.maps.clear();
    out.maps.reserve(world_state_.maps().size());

    for (const auto& [id, state] : world_state_.maps()) {
        (void)id;
        MapSaveData map{};
        map.map_name = state.info.name;
        map.last_updated_day = state.persistent.last_updated_day;
        map.opened_chests.assign(state.persistent.opened_chests.begin(), state.persistent.opened_chests.end());
        std::sort(map.opened_chests.begin(), map.opened_chests.end());

        if (state.persistent.snapshot.valid) {
            entt::registry temp;
            auto snapshot_copy = state.persistent.snapshot;
            game::world::DynamicSnapshotFlags read_flags{};
            if (!game::world::readDynamicSnapshot(temp, snapshot_copy, &read_flags)) {
                out_error = "读取地图快照失败: " + state.info.name;
                return out;
            }

            auto auto_view = temp.view<engine::component::AutoTileComponent, engine::component::TransformComponent>();
            for (auto entity : auto_view) {
                const auto& auto_tile = auto_view.get<engine::component::AutoTileComponent>(entity);
                const auto& tr = auto_view.get<engine::component::TransformComponent>(entity);
                const auto tile = worldToTile(tr.position_);
                if (auto_tile.rule_id_ == RULE_SOIL_TILLED) {
                    map.tilled_tiles.push_back(tile);
                } else if (auto_tile.rule_id_ == RULE_SOIL_WET) {
                    map.wet_tiles.push_back(tile);
                }
            }

            auto crop_view = temp.view<game::component::CropComponent, engine::component::TransformComponent>();
            for (auto entity : crop_view) {
                const auto& crop = crop_view.get<game::component::CropComponent>(entity);
                const auto& tr = crop_view.get<engine::component::TransformComponent>(entity);
                map.crops.push_back(CropSaveData{
                    worldToTile(tr.position_),
                    game::defs::cropTypeToString(crop.type_),
                    game::defs::growthStageToString(crop.current_stage_),
                    crop.planted_day_,
                    crop.stage_countdown_,
                });
            }

            if (game::world::hasFlag(read_flags, game::world::DynamicSnapshotFlags::IncludeResources)) {
                std::vector<ResourceNodeSaveData> nodes;
                auto res_view = temp.view<game::component::ResourceNodeComponent, engine::component::TransformComponent>();
                for (auto entity : res_view) {
                    const auto& node = res_view.get<game::component::ResourceNodeComponent>(entity);
                    const auto& tr = res_view.get<engine::component::TransformComponent>(entity);
                    nodes.push_back(ResourceNodeSaveData{
                        worldToTile(tr.position_),
                        resourceNodeTypeToString(node.type_),
                        node.hit_count_,
                        node.hits_to_break_,
                        static_cast<std::uint32_t>(node.drop_item_id_),
                    });
                }
                map.resource_nodes = std::move(nodes);
            } else if (state.persistent.pending_resource_nodes) {
                map.resource_nodes = convertPendingResourceNodes(*state.persistent.pending_resource_nodes);
            }
        } else if (state.persistent.pending_resource_nodes) {
            map.resource_nodes = convertPendingResourceNodes(*state.persistent.pending_resource_nodes);
        }

        out.maps.push_back(std::move(map));
    }

    return out;
}

bool SaveService::apply(const SaveData& data, std::string& out_error) {
    out_error.clear();

    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        out_error = "registry.ctx() 缺少 GameTime";
        return false;
    }
    game_time->day_ = data.game_time.day;
    game_time->hour_ = data.game_time.hour;
    game_time->minute_ = data.game_time.minute;
    game_time->time_scale_ = data.game_time.time_scale;
    game_time->paused_ = data.game_time.paused;
    game_time->time_of_day_ = game_time->calculateTimeOfDay(game_time->hour_);

    map_manager_.unloadCurrentMap();

    for (auto& [id, state] : world_state_.mapsMutable()) {
        (void)id;
        state.persistent.snapshot.clear();
        state.persistent.last_updated_day = 0;
        state.persistent.pending_resource_nodes.reset();
        state.persistent.opened_chests.clear();
    }

    const auto& auto_tile_library = context_.getResourceManager().getAutoTileLibrary();

    for (const auto& map_save : data.maps) {
        if (map_save.map_name.empty()) {
            continue;
        }

        const entt::id_type map_id = world_state_.ensureExternalMap(map_save.map_name);
        auto* map_state = world_state_.getMapStateMutable(map_id);
        if (!map_state) {
            continue;
        }

        map_state->persistent.last_updated_day = map_save.last_updated_day;
        map_state->persistent.opened_chests.clear();
        for (const int chest_id : map_save.opened_chests) {
            map_state->persistent.opened_chests.insert(chest_id);
        }

        if (map_save.resource_nodes) {
            std::vector<game::world::ResourceNodeState> nodes;
            nodes.reserve(map_save.resource_nodes->size());
            for (const auto& node : *map_save.resource_nodes) {
                nodes.push_back(game::world::ResourceNodeState{
                    glm::ivec2{node.tile.x, node.tile.y},
                    resourceNodeTypeFromString(node.node_type),
                    node.hit_count,
                });
            }
            map_state->persistent.pending_resource_nodes = std::move(nodes);
        }

        if (map_save.tilled_tiles.empty() && map_save.wet_tiles.empty() && map_save.crops.empty()) {
            continue;
        }

        entt::registry temp;

        std::unordered_set<std::int64_t> tilled_set;
        tilled_set.reserve(map_save.tilled_tiles.size());
        for (const auto& tile : map_save.tilled_tiles) {
            tilled_set.insert((static_cast<std::int64_t>(tile.x) << 32) | static_cast<std::uint32_t>(tile.y));
        }

        std::unordered_set<std::int64_t> wet_set;
        wet_set.reserve(map_save.wet_tiles.size());
        for (const auto& tile : map_save.wet_tiles) {
            wet_set.insert((static_cast<std::int64_t>(tile.x) << 32) | static_cast<std::uint32_t>(tile.y));
        }

        constexpr int SOIL_LAYER = game::defs::render_layer::SOIL;
        constexpr int WET_LAYER = game::defs::render_layer::SOIL_WET;

        for (const auto& tile : map_save.tilled_tiles) {
            const auto mask = computeAutoTileMask(tilled_set, tile);
            engine::component::SpriteComponent sprite{};
            if (!buildAutoTileSprite(auto_tile_library, RULE_SOIL_TILLED, mask, sprite)) {
                out_error = "AutoTileLibrary 缺少 soil_tilled 规则";
                return false;
            }

            const auto entity = temp.create();
            temp.emplace<game::component::MapId>(entity, map_id);
            temp.emplace<engine::component::TransformComponent>(entity, tileToWorld(tile));
            temp.emplace<engine::component::SpriteComponent>(entity, sprite);
            temp.emplace<engine::component::AutoTileComponent>(entity, RULE_SOIL_TILLED, mask);
            temp.emplace<engine::component::RenderComponent>(entity, SOIL_LAYER);
        }

        for (const auto& tile : map_save.wet_tiles) {
            const auto mask = computeAutoTileMask(wet_set, tile);
            engine::component::SpriteComponent sprite{};
            if (!buildAutoTileSprite(auto_tile_library, RULE_SOIL_WET, mask, sprite)) {
                out_error = "AutoTileLibrary 缺少 soil_wet 规则";
                return false;
            }

            const auto entity = temp.create();
            temp.emplace<game::component::MapId>(entity, map_id);
            temp.emplace<engine::component::TransformComponent>(entity, tileToWorld(tile));
            temp.emplace<engine::component::SpriteComponent>(entity, sprite);
            temp.emplace<engine::component::AutoTileComponent>(entity, RULE_SOIL_WET, mask);
            temp.emplace<engine::component::RenderComponent>(entity, WET_LAYER);
        }

        for (const auto& crop : map_save.crops) {
            const auto crop_type = game::defs::cropTypeFromString(crop.crop_type);
            const auto crop_id = game::defs::cropTypeToId(crop_type);
            if (crop_id == entt::null || !blueprint_manager_.hasCropBlueprint(crop_id)) {
                continue;
            }

            const auto stage = game::defs::growthStageFromString(crop.growth_stage);
            const auto& blueprint = blueprint_manager_.getCropBlueprint(crop_id);
            const game::factory::SpriteBlueprint* sprite_bp = nullptr;
            for (const auto& entry : blueprint.stages_) {
                if (entry.stage_ == stage) {
                    sprite_bp = &entry.sprite_;
                    break;
                }
            }
            if (!sprite_bp && !blueprint.stages_.empty()) {
                sprite_bp = &blueprint.stages_.front().sprite_;
            }
            if (!sprite_bp) {
                continue;
            }

            const auto entity = temp.create();
            temp.emplace<game::component::MapId>(entity, map_id);
            temp.emplace<engine::component::TransformComponent>(entity, tileToWorld(crop.tile) + game::defs::CROP_OFFSET);

            engine::component::SpriteComponent sprite_comp{
                engine::component::Sprite(sprite_bp->path_, sprite_bp->src_rect_, sprite_bp->flip_horizontal_),
                sprite_bp->dst_size_,
                sprite_bp->pivot_
            };
            temp.emplace<engine::component::SpriteComponent>(entity, std::move(sprite_comp));

            const int crop_layer = stage == game::defs::GrowthStage::Seed
                                       ? game::defs::render_layer::CROP_SEED
                                       : game::defs::render_layer::ACTOR;
            temp.emplace<engine::component::RenderComponent>(entity, crop_layer);

            auto& crop_comp = temp.emplace<game::component::CropComponent>(entity, crop_type, crop.planted_day, crop.stage_countdown);
            crop_comp.current_stage_ = stage;
            crop_comp.planted_day_ = crop.planted_day;
            crop_comp.stage_countdown_ = crop.stage_countdown;
        }

        game::world::writeDynamicSnapshot(temp, map_id, map_state->persistent.snapshot, game::world::DynamicSnapshotFlags::None);
    }

    if (!map_manager_.loadMap(data.player.map_name)) {
        out_error = "加载存档地图失败: " + data.player.map_name;
        return false;
    }

    auto player_view = registry_.view<game::component::PlayerTag>();
    if (player_view.empty()) {
        out_error = "加载地图后未找到玩家实体";
        return false;
    }
    const entt::entity player = *player_view.begin();

    // 读档可能发生在玩家“动作中/持物中/被锁定中”的任意一帧：
    // 这些状态不应该跨存档持久化，否则会出现“读档后卡动作/卡手持”的问题。
    if (auto* actor = registry_.try_get<game::component::ActorComponent>(player)) {
        actor->tool_ = game::defs::Tool::None;
        actor->hold_seed_ = game::defs::CropType::Unknown;
        actor->hold_seed_item_id_ = entt::null;
        actor->hold_seed_inventory_slot_ = -1;
    }

    // 位置恢复后标记 TransformDirty，避免依赖 Transform 的系统（渲染/相机/空间索引）
    // 继续使用“旧位置”的缓存状态。
    if (auto* tr = registry_.try_get<engine::component::TransformComponent>(player)) {
        tr->position_ = glm::vec2{data.player.position.x, data.player.position.y};
        registry_.emplace_or_replace<engine::component::TransformDirtyTag>(player);
    }

    // 清除 ActionLock：避免读档后仍处于“禁止输入/禁止动作”的锁定态。
    registry_.remove<game::component::ActionLockedTag>(player);

    // 方向/动作是渲染与动画的“外显状态”，恢复后标记 StateDirty 让表现立刻同步。
    if (auto* state = registry_.try_get<game::component::StateComponent>(player)) {
        state->direction_ = directionFromString(data.player.state.direction);
        state->action_ = actionFromString(data.player.state.action);
        registry_.emplace_or_replace<game::component::StateDirtyTag>(player);
    }

    auto* inv = registry_.try_get<game::component::InventoryComponent>(player);
    auto* hotbar = registry_.try_get<game::component::HotbarComponent>(player);
    if (!inv || !hotbar) {
        out_error = "玩家缺少 InventoryComponent/HotbarComponent";
        return false;
    }

    inv->active_page_ = data.player.inventory.active_page;
    inv->clearAll();
    for (std::size_t i = 0; i < inv->slots_.size() && i < data.player.inventory.slots.size(); ++i) {
        const auto& slot = data.player.inventory.slots[i];
        if (slot.item_id == 0u || slot.count <= 0) {
            continue;
        }
        inv->slots_[i].item_id_ = static_cast<entt::id_type>(slot.item_id);
        inv->slots_[i].count_ = slot.count;
    }

    hotbar->active_slot_index_ = std::clamp(data.player.hotbar.active_slot, 0, game::component::HotbarComponent::SLOT_COUNT - 1);
    hotbar->clearAll();
    for (std::size_t i = 0; i < hotbar->slots_.size() && i < data.player.hotbar.inventory_slot_indices.size(); ++i) {
        hotbar->slots_[i].inventory_slot_index_ = data.player.hotbar.inventory_slot_indices[i];
    }

    auto& dispatcher = context_.getDispatcher();
    dispatcher.trigger(game::defs::InventorySyncRequest{player});
    dispatcher.trigger(game::defs::HotbarSyncRequest{player, true});
    dispatcher.trigger(game::defs::HotbarActivateRequest{player, hotbar->active_slot_index_});

    spdlog::info("SaveService: 已载入存档 '{}'", data.player.map_name);
    return true;
}

} // namespace game::save
