#include "entity_builder.h"
#include "tiled_conventions.h"
#include "game/factory/entity_factory.h"
#include "game/component/tags.h"
#include "game/component/map_component.h"
#include "game/component/resource_node_component.h"
#include "game/component/chest_component.h"
#include "game/component/enemy_encounter_component.h"
#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/party_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/recruitable_component.h"
#include "game/data/battle_background_id.h"
#include "game/defs/spatial_layers.h"
#include "game/world/world_state.h"
#include "game/data/game_time.h"
#include "engine/core/context.h"
#include "engine/component/tilelayer_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/light_component.h"
#include "engine/component/name_component.h"
#include "engine/component/tags.h"
#include "engine/loader/level_loader.h"
#include "engine/loader/tiled_diagnostics.h"
#include "engine/spatial/collider_shape.h"
#include "engine/utils/math.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

using namespace entt::literals;

namespace {
[[nodiscard]] bool isDarkForEmissivesNow(entt::registry& registry) {
    if (auto* game_time = registry.ctx().find<game::data::GameTime>()) {
        return game_time->isDarkForEmissives();
    }
    return false;
}

struct TimeVisibilityFlags {
    bool night_only = false;
    bool day_only   = false;
};

[[nodiscard]] TimeVisibilityFlags parseTimeVisibilityFlags(const nlohmann::json& properties) {
    TimeVisibilityFlags flags;
    for (const auto& property : properties) {
        const auto name = property.value("name", "");
        if (property.value("type", "") != "bool") continue;
        if (name == "night_only") flags.night_only = property.value("value", false);
        else if (name == "day_only") flags.day_only = property.value("value", false);
    }
    return flags;
}

[[nodiscard]] std::optional<std::string> findObjectStringProperty(const nlohmann::json* object_json,
                                                                  const std::string_view property_name) {
    if (!object_json) {
        return std::nullopt;
    }

    const auto properties_it = object_json->find("properties");
    if (properties_it == object_json->end() || !properties_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object()) {
            continue;
        }

        const auto name_it = property.find("name");
        if (name_it == property.end() || !name_it->is_string() || name_it->get_ref<const std::string&>() != property_name) {
            continue;
        }

        const auto type_it = property.find("type");
        const auto value_it = property.find("value");
        if (type_it == property.end() || !type_it->is_string() || type_it->get_ref<const std::string&>() != "string" ||
            value_it == property.end() || !value_it->is_string()) {
            return std::nullopt;
        }

        const std::string& value = value_it->get_ref<const std::string&>();
        if (value.empty()) {
            return std::nullopt;
        }
        return value;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<int> findObjectIntProperty(const nlohmann::json* object_json,
                                                       const std::string_view property_name) {
    if (!object_json) {
        return std::nullopt;
    }

    const auto properties_it = object_json->find("properties");
    if (properties_it == object_json->end() || !properties_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object()) {
            continue;
        }

        const auto name_it = property.find("name");
        if (name_it == property.end() || !name_it->is_string() || name_it->get_ref<const std::string&>() != property_name) {
            continue;
        }

        const auto value_it = property.find("value");
        if (value_it == property.end() || !value_it->is_number()) {
            return std::nullopt;
        }
        return value_it->get<int>();
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<bool> findObjectBoolProperty(const nlohmann::json* object_json,
                                                        const std::string_view property_name) {
    if (!object_json) {
        return std::nullopt;
    }

    const auto properties_it = object_json->find("properties");
    if (properties_it == object_json->end() || !properties_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object()) {
            continue;
        }

        const auto name_it = property.find("name");
        if (name_it == property.end() || !name_it->is_string() || name_it->get_ref<const std::string&>() != property_name) {
            continue;
        }

        const auto value_it = property.find("value");
        if (value_it == property.end() || !value_it->is_boolean()) {
            return std::nullopt;
        }
        return value_it->get<bool>();
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<float> findObjectFloatProperty(const nlohmann::json* object_json,
                                                          const std::string_view property_name) {
    if (!object_json) {
        return std::nullopt;
    }

    const auto properties_it = object_json->find("properties");
    if (properties_it == object_json->end() || !properties_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object()) {
            continue;
        }

        const auto name_it = property.find("name");
        if (name_it == property.end() || !name_it->is_string() || name_it->get_ref<const std::string&>() != property_name) {
            continue;
        }

        const auto value_it = property.find("value");
        if (value_it == property.end() || !value_it->is_number()) {
            return std::nullopt;
        }
        return value_it->get<float>();
    }

    return std::nullopt;
}

[[nodiscard]] bool isActorAlreadyRecruited(entt::registry& registry, const std::string_view actor_id) {
    auto players = registry.view<game::component::PlayerTag, game::component::PartyComponent>();
    for (const auto player : players) {
        const auto& party = players.get<game::component::PartyComponent>(player);
        if (std::any_of(party.recruited_actor_ids_.begin(), party.recruited_actor_ids_.end(),
                        [actor_id](const std::string& current) {
                            return current == actor_id;
                        })) {
            return true;
        }
    }
    return false;
}

void applyTimeVisibilityTags(entt::registry& registry, entt::entity entity, bool night_only, bool day_only, const char* label) {
    if (entity == entt::null || (!night_only && !day_only)) {
        return;
    }

    if (!label) {
        label = "Light";
    }

    if (night_only && day_only) {
        spdlog::warn("{}: 同时设置了 night_only 与 day_only，光源将始终隐藏", label);
        registry.emplace<game::component::NightOnlyTag>(entity);
        registry.emplace<game::component::DayOnlyTag>(entity);
        registry.emplace_or_replace<engine::component::LightDisabledTag>(entity);
        return;
    }

    if (night_only) {
        registry.emplace<game::component::NightOnlyTag>(entity);
        if (!isDarkForEmissivesNow(registry)) {
            registry.emplace_or_replace<engine::component::LightDisabledTag>(entity);
        }
        return;
    }

    registry.emplace<game::component::DayOnlyTag>(entity);
    if (isDarkForEmissivesNow(registry)) {
        registry.emplace_or_replace<engine::component::LightDisabledTag>(entity);
    }
}

void registerRectToStaticGrid(engine::spatial::SpatialIndexManager& spatial_index,
                              glm::ivec2 map_size,
                              glm::ivec2 tile_size,
                              const engine::utils::Rect& rect,
                              entt::entity entity,
                              entt::id_type layer_id,
                              engine::component::TileType tile_type,
                              const char* label) {
    if (entity == entt::null) {
        return;
    }

    if (!label) {
        label = "Rect";
    }

    if (!spatial_index.isInitialized()) {
        spdlog::warn("{}: SpatialIndexManager 未初始化，跳过静态网格注册", label);
        return;
    }

    if (map_size.x <= 0 || map_size.y <= 0 || tile_size.x <= 0 || tile_size.y <= 0) {
        spdlog::warn("{}: 地图尺寸或瓦片尺寸无效，跳过静态网格注册", label);
        return;
    }

    const glm::vec2 tile_size_px{
        static_cast<float>(tile_size.x),
        static_cast<float>(tile_size.y),
    };

    const glm::vec2 epsilon{0.001f, 0.001f};
    glm::vec2 max_for_coord = rect.pos + rect.size - epsilon;
    max_for_coord = glm::max(max_for_coord, rect.pos);

    glm::ivec2 min_tile = spatial_index.getTileCoordAtWorldPos(rect.pos);
    glm::ivec2 max_tile = spatial_index.getTileCoordAtWorldPos(max_for_coord);
    min_tile = glm::clamp(min_tile, glm::ivec2(0), map_size - glm::ivec2(1));
    max_tile = glm::clamp(max_tile, glm::ivec2(0), map_size - glm::ivec2(1));

    for (int y = min_tile.y; y <= max_tile.y; ++y) {
        for (int x = min_tile.x; x <= max_tile.x; ++x) {
            const engine::utils::Rect tile_rect{
                glm::vec2{static_cast<float>(x) * tile_size_px.x, static_cast<float>(y) * tile_size_px.y},
                tile_size_px,
            };
            if (!engine::spatial::rectRectOverlap(rect, tile_rect)) {
                continue;
            }
            spatial_index.addTileEntity(glm::ivec2{x, y}, entity, layer_id);
            spatial_index.setTileType(glm::ivec2{x, y}, tile_type);
        }
    }
}
} // namespace

namespace game::loader {

EntityBuilder::EntityBuilder(engine::loader::LevelLoader& level_loader, 
    engine::core::Context& context, 
    entt::registry& registry,
    game::factory::EntityFactory& entity_factory)
    : BasicEntityBuilder(level_loader, context, registry), entity_factory_(entity_factory) {
}

EntityBuilder* EntityBuilder::build() {
    if (object_json_ && !tile_info_) {  // 代表自己绘制的形状
        const bool is_point = object_json_->value("point", false) == true;
        const std::string type = object_json_->value("type", "");
        const std::string name = object_json_->value("name", "");

        if (is_point && type == tiled::OBJECT_TYPE_ACTOR) {
            if (!name.empty()) {
                buildActor(entt::hashed_string(name.c_str()));
            } else {
                spdlog::warn("EntityBuilder: actor point 对象缺少 name，回退为默认构建");
                BasicEntityBuilder::build();
            }
        } else if (is_point && type == tiled::OBJECT_TYPE_ANIMAL) {
            if (!name.empty()) {
                buildAnimal(entt::hashed_string(name.c_str()));
            } else {
                spdlog::warn("EntityBuilder: animal point 对象缺少 name，回退为默认构建");
                BasicEntityBuilder::build();
            }
        } else if (type == tiled::OBJECT_TYPE_MAP_TRIGGER) {
            buildMapTrigger();
        } else if (type == tiled::OBJECT_TYPE_REST) {
            buildRestArea();
        } else if (type == tiled::OBJECT_TYPE_CLOSET) {
            buildClosetArea();
        } else if (type == tiled::OBJECT_TYPE_LIGHT) {
            if (is_point && name == tiled::LIGHT_NAME_POINT) {
                buildPointLight();
            } else if (is_point && name == tiled::LIGHT_NAME_SPOT) {
                buildSpotLight();
            } else if (name == tiled::LIGHT_NAME_EMISSIVE) {
                buildEmissiveRect();
            } else {
                engine::loader::tiled::diagnostics::recordUnknownLightName(name, is_point);
                spdlog::warn("EntityBuilder: 未知 light name='{}' (point={})，回退为默认构建", name, is_point);
                BasicEntityBuilder::build();
            }
        } else {
            engine::loader::tiled::diagnostics::recordUnknownObjectType(type, name, is_point);
            if (!type.empty()) {
                spdlog::warn("EntityBuilder: 未知 object type='{}' (name='{}', point={})，回退为默认构建", type, name, is_point);
            }
            BasicEntityBuilder::build();
        }
    } else {  // 非自己绘制的形状，按默认方式构建
        BasicEntityBuilder::build();
        if (tile_info_) {
            buildFarmTag();
        }
    }
    attachMapId();
    return this;
}

void EntityBuilder::decorateExternalEntity(entt::entity entity) {
    if (map_id_ == entt::null || entity == entt::null) {
        return;
    }
    registry_.emplace_or_replace<game::component::MapId>(entity, map_id_);
}

void EntityBuilder::buildActor(entt::id_type name_id) {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));

    const auto shop_id = findObjectStringProperty(object_json_, tiled::ACTOR_PROP_SHOP_ID);
    const auto quest_offer_id = findObjectStringProperty(object_json_, tiled::ACTOR_PROP_QUEST_OFFER_ID);
    const auto recruit_actor_id = findObjectStringProperty(object_json_, tiled::ACTOR_PROP_RECRUIT_ACTOR_ID);
    const auto battle_troop_id = findObjectStringProperty(object_json_, tiled::ACTOR_PROP_BATTLE_TROOP_ID);
    auto battle_background_id = findObjectStringProperty(object_json_, tiled::ACTOR_PROP_BATTLE_BACKGROUND_ID);
    const auto encounter_id = findObjectIntProperty(object_json_, tiled::ACTOR_PROP_ENCOUNTER_ID);
    const bool encounter_once = findObjectBoolProperty(object_json_, tiled::ACTOR_PROP_ENCOUNTER_ONCE).value_or(false);
    const auto wander_radius_override = findObjectFloatProperty(object_json_, tiled::ACTOR_PROP_WANDER_RADIUS_OVERRIDE);

    if (battle_background_id && !game::data::isValidBattleBackgroundId(*battle_background_id)) {
        spdlog::warn("EntityBuilder: actor 声明 battle_background_id='{}' 非法，已忽略。", *battle_background_id);
        battle_background_id.reset();
    }

    if (reuse_player_if_exists_ && name_id == "player"_hs) {
        auto view = registry_.view<game::component::PlayerTag, engine::component::TransformComponent>();
        if (view.begin() != view.end()) {
            entity_id_ = *view.begin();
            auto& transform = registry_.get<engine::component::TransformComponent>(entity_id_);
            transform.position_ = position;
            registry_.emplace_or_replace<engine::component::TransformDirtyTag>(entity_id_);
            return;
        }
    }

    bool should_attach_encounter = false;
    if (battle_troop_id) {
        if (shop_id || quest_offer_id || recruit_actor_id) {
            spdlog::warn("EntityBuilder: actor 同时声明 battle_troop_id='{}' 与 shop_id/quest_offer_id/recruit_actor_id，本阶段忽略战斗入口。",
                         *battle_troop_id);
        } else if (!encounter_id || *encounter_id <= 0) {
            spdlog::warn("EntityBuilder: actor 声明 battle_troop_id='{}' 但缺少合法 encounter_id，忽略战斗入口。",
                         *battle_troop_id);
        } else if (encounter_once && isEncounterDefeated(*encounter_id)) {
            entity_id_ = entt::null;
            return;
        } else if (!seen_encounter_ids_.insert(*encounter_id).second) {
            spdlog::warn("EntityBuilder: map_id={} 内重复声明 encounter_id={}，后续重复项将作为普通 actor 生成。",
                         static_cast<std::uint64_t>(map_id_),
                         *encounter_id);
        } else {
            should_attach_encounter = true;
        }
    }

    bool should_attach_recruitable = false;
    if (recruit_actor_id) {
        if (shop_id || quest_offer_id || battle_troop_id) {
            spdlog::warn("EntityBuilder: actor 同时声明 recruit_actor_id='{}' 与 shop_id/quest_offer_id/battle_troop_id，本阶段忽略招募入口。",
                         *recruit_actor_id);
        } else {
            should_attach_recruitable = true;
        }
    }
    if (should_attach_recruitable && recruit_actor_id && isActorAlreadyRecruited(registry_, *recruit_actor_id)) {
        entity_id_ = entt::null;
        return;
    }

    entity_id_ = entity_factory_.createActor(name_id, position);
    if (entity_id_ == entt::null) {
        return;
    }

    if (shop_id) {
        if (quest_offer_id) {
            spdlog::warn("EntityBuilder: actor 同时声明 shop_id='{}' 与 quest_offer_id='{}'，本阶段按 merchant 优先处理。",
                         *shop_id,
                         *quest_offer_id);
        }
        registry_.emplace_or_replace<game::component::MerchantComponent>(
            entity_id_,
            game::component::MerchantComponent{
                .shop_id_ = *shop_id,
                .shop_id_hash_ = entt::hashed_string{shop_id->c_str()}.value()});
    } else if (quest_offer_id) {
        registry_.emplace_or_replace<game::component::QuestGiverComponent>(
            entity_id_,
            game::component::QuestGiverComponent{
                .quest_id_ = *quest_offer_id,
                .quest_id_hash_ = entt::hashed_string{quest_offer_id->c_str()}.value()});
    } else if (should_attach_recruitable && recruit_actor_id) {
        registry_.emplace_or_replace<game::component::RecruitableComponent>(
            entity_id_,
            game::component::RecruitableComponent{
                .actor_id_ = *recruit_actor_id,
                .actor_id_hash_ = entt::hashed_string{recruit_actor_id->c_str()}.value()});
    }

    if (wander_radius_override) {
        auto& wander = registry_.get_or_emplace<game::component::WanderComponent>(entity_id_);
        wander.home_position_ = position;
        wander.target_ = position;
        wander.radius_ = std::max(0.0F, *wander_radius_override);
        wander.phase_ = game::component::WanderPhase::Waiting;
        wander.wait_timer_ = 0.0F;
    }

    if (should_attach_encounter && battle_troop_id && encounter_id) {
        registry_.emplace_or_replace<game::component::EnemyEncounterComponent>(
            entity_id_,
            game::component::EnemyEncounterComponent{
                .troop_id_ = *battle_troop_id,
                .troop_id_hash_ = entt::hashed_string{battle_troop_id->c_str()}.value(),
                .battle_background_id_ = battle_background_id.value_or(std::string{}),
                .encounter_id_ = *encounter_id,
                .once_ = encounter_once,
                .defeated_ = false,
                .home_position_ = position});
    }
}

bool EntityBuilder::isEncounterDefeated(int encounter_id) const {
    if (map_id_ == entt::null || encounter_id <= 0) {
        return false;
    }

    auto** world_state_ptr = registry_.ctx().find<game::world::WorldState*>();
    if (!world_state_ptr || !*world_state_ptr) {
        return false;
    }

    const auto* map_state = (*world_state_ptr)->getMapState(map_id_);
    if (!map_state) {
        return false;
    }

    return map_state->persistent.defeated_encounters.contains(encounter_id);
}

void EntityBuilder::buildAnimal(entt::id_type name_id) {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));
    entity_id_ = entity_factory_.createAnimal(name_id, position);
}

void EntityBuilder::buildFarmTag() {
    auto property_json = tile_info_->properties_;
    if (!property_json) return;

    auto tag_str = getStringProperty(tiled::TILE_PROPERTY_OBJ_TYPE);
    auto anim_str = getStringProperty(tiled::TILE_PROPERTY_ANIM_ID);

    if (tag_str && tag_str.value() == tiled::TILE_OBJ_TYPE_CHEST) {
        game::component::ChestComponent chest{};
        chest.opened_ = false;
        if (object_json_) {
            chest.chest_id_ = object_json_->value("id", 0);
            const auto& props = object_json_->value("properties", nlohmann::json::array_t());
            for (const auto& prop : props) {
                if (!prop.is_object()) continue;
                if (prop.value("type", "") == "int") {
                    const std::string name = prop.value("name", "");
                    const int value = prop.value("value", 0);
                    if (name.empty() || value <= 0) continue;
                    if (name == "chest_id") {
                        chest.chest_id_ = value;
                        continue;
                    }
                    chest.rewards_.push_back(game::component::ItemReward{entt::hashed_string(name.c_str()), value});
                }
            }
        }
        registry_.emplace_or_replace<game::component::ChestComponent>(entity_id_, std::move(chest));
        return;
    }

    const bool is_crop = tag_str && tag_str.value() == tiled::TILE_OBJ_TYPE_CROP;
    const bool is_tree =
        (tag_str && tag_str.value() == tiled::TILE_OBJ_TYPE_TREE) || (anim_str && anim_str.value() == tiled::ANIM_ID_AXE);
    const bool is_rock =
        (tag_str && tag_str.value() == tiled::TILE_OBJ_TYPE_ROCK) ||
        (anim_str && anim_str.value() == tiled::ANIM_ID_PICKAXE);

    if (is_crop) {
        return;
    }

    if (is_tree) {
        game::component::ResourceNodeComponent node{};
        node.type_ = game::component::ResourceNodeType::Tree;
        node.hits_to_break_ = 5;
        node.drop_item_id_ = "material_timber"_hs;
        node.drop_min_ = 1;
        node.drop_max_ = 3;
        if (anim_str) {
            node.hit_animation_id_ = entt::hashed_string(anim_str->c_str());
        }
        registry_.emplace_or_replace<game::component::ResourceNodeComponent>(entity_id_, node);
        return;
    }

    if (is_rock) {
        game::component::ResourceNodeComponent node{};
        node.type_ = game::component::ResourceNodeType::Rock;
        node.hits_to_break_ = 3;
        node.drop_item_id_ = "material_stone"_hs;
        node.drop_min_ = 1;
        node.drop_max_ = 3;
        if (anim_str) {
            node.hit_animation_id_ = entt::hashed_string(anim_str->c_str());
        }
        registry_.emplace_or_replace<game::component::ResourceNodeComponent>(entity_id_, node);
        return;
    }

}

void EntityBuilder::buildMapTrigger() {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));
    auto size = glm::vec2(object_json_->value("width", 0.0f), object_json_->value("height", 0.0f));
    const auto& properties = object_json_->value("properties", nlohmann::json::array_t());
    int self_id = -1;       // 自身点ID（即当前地图中，地图切换触发器的id）
    int target_id = -1;     // 目标点ID（即另一个地图中，地图切换触发器的id）
    std::string target_map_name = ""; // 目标地图名称（不含 .tmj）；实际文件路径由 WorldState 统一拼接
    std::string start_offset = "";    // 起始偏移方向，可选值为"left"、"right"、"top/up"、"bottom/down"
    for (const auto& property : properties) {
        if (property.value("name", "") == tiled::TRIGGER_PROP_SELF_ID && property.value("type", "") == "int") {
            self_id = property.value("value", -1);
        }   
        else if (property.value("name", "") == tiled::TRIGGER_PROP_TARGET_ID && property.value("type", "") == "int") {
            target_id = property.value("value", -1);
        }   
        else if (property.value("name", "") == tiled::TRIGGER_PROP_TARGET_MAP && property.value("type", "") == "string") {
            target_map_name = property.value("value", "");
        }   
        else if (property.value("name", "") == tiled::TRIGGER_PROP_START_OFFSET && property.value("type", "") == "string") {
            start_offset = property.value("value", "");
        }   
    }
    if (self_id == -1 || target_id == -1 || target_map_name.empty() || start_offset.empty()) {
        spdlog::error("MapTrigger: 地图切换触发器创建失败，缺少必要属性");
        return;
    }
    game::component::StartOffset offset_enum = game::component::StartOffset::None;
    if (start_offset == tiled::START_OFFSET_LEFT) offset_enum = game::component::StartOffset::Left;
    else if (start_offset == tiled::START_OFFSET_RIGHT) offset_enum = game::component::StartOffset::Right;
    else if (start_offset == tiled::START_OFFSET_TOP || start_offset == tiled::START_OFFSET_UP) offset_enum = game::component::StartOffset::Top;
    else if (start_offset == tiled::START_OFFSET_BOTTOM || start_offset == tiled::START_OFFSET_DOWN) offset_enum = game::component::StartOffset::Bottom;
    else {
        spdlog::warn("MapTrigger: 未知 start_offset='{}'，将使用 None", start_offset);
    }

    entity_id_ = registry_.create();
    engine::utils::Rect rect{position, size};

    game::component::MapTrigger trigger{};
    trigger.rect_ = rect;
    trigger.map_id_ = map_id_;
    trigger.self_id_ = self_id;
    trigger.target_id_ = target_id;
    trigger.target_map_ = entt::hashed_string(target_map_name.c_str());
    trigger.target_map_name_ = target_map_name;
    trigger.start_offset_ = offset_enum;

    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<game::component::MapTrigger>(entity_id_, trigger);

    // 记录到 WorldState 以便跨地图查询
    if (map_id_ != entt::null) {
        auto** world_state_ptr = registry_.ctx().find<game::world::WorldState*>();
        if (world_state_ptr && *world_state_ptr) {
            if (auto* map_state = (*world_state_ptr)->getMapStateMutable(map_id_)) {
                game::world::TriggerInfo info{};
                info.rect = rect;
                info.self_id = self_id;
                info.target_id = target_id;
                info.target_map = trigger.target_map_;
                info.target_map_name = target_map_name;
                info.start_offset = offset_enum;
                map_state->triggers.push_back(info);
            }
        }
    }
    spdlog::info("MapTrigger: 地图切换触发器创建完成，self_id: {}, target_id: {}, target_map_name: {}", self_id, target_id, target_map_name);
}

void EntityBuilder::buildRestArea() {
    if (!object_json_) {
        return;
    }

    const glm::vec2 position{
        object_json_->value("x", 0.0f),
        object_json_->value("y", 0.0f),
    };
    const glm::vec2 size{
        object_json_->value("width", 0.0f),
        object_json_->value("height", 0.0f),
    };

    if (size.x <= 0.0f || size.y <= 0.0f) {
        spdlog::error("RestArea: 创建失败，缺少有效尺寸 width/height: {}, {}", size.x, size.y);
        return;
    }

    entity_id_ = registry_.create();
    if (auto name = object_json_->value("name", ""); !name.empty()) {
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
    }

    engine::utils::Rect rect{position, size};
    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<game::component::RestArea>(entity_id_, rect);

    auto& spatial_index = context_.getSpatialIndexManager();
    registerRectToStaticGrid(spatial_index,
                             level_loader_.getMapSize(),
                             level_loader_.getTileSize(),
                             rect,
                             entity_id_,
                             game::defs::spatial_layer::REST,
                             engine::component::TileType::INTERACT,
                             "RestArea");

    spdlog::info("RestArea: 创建完成，pos=({}, {}), size=({}, {})", rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
}

void EntityBuilder::buildClosetArea() {
    if (!object_json_) {
        return;
    }

    const glm::vec2 position{
        object_json_->value("x", 0.0f),
        object_json_->value("y", 0.0f),
    };
    const glm::vec2 size{
        object_json_->value("width", 0.0f),
        object_json_->value("height", 0.0f),
    };

    if (size.x <= 0.0f || size.y <= 0.0f) {
        spdlog::error("ClosetArea: 创建失败，缺少有效尺寸 width/height: {}, {}", size.x, size.y);
        return;
    }

    entity_id_ = registry_.create();
    if (auto name = object_json_->value("name", ""); !name.empty()) {
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
    }

    engine::utils::Rect rect{position, size};
    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<game::component::ClosetArea>(entity_id_, rect);

    auto& spatial_index = context_.getSpatialIndexManager();
    registerRectToStaticGrid(spatial_index,
                             level_loader_.getMapSize(),
                             level_loader_.getTileSize(),
                             rect,
                             entity_id_,
                             game::defs::spatial_layer::CLOSET,
                             engine::component::TileType::INTERACT,
                             "ClosetArea");

    spdlog::info("ClosetArea: 创建完成，pos=({}, {}), size=({}, {})", rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
}

void EntityBuilder::attachMapId() {
    if (map_id_ == entt::null || entity_id_ == entt::null) {
        return;
    }
    registry_.emplace_or_replace<game::component::MapId>(entity_id_, map_id_);
}

void EntityBuilder::buildPointLight() {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));
    const auto& properties = object_json_->value("properties", nlohmann::json::array_t());
    const auto [night_only, day_only] = parseTimeVisibilityFlags(properties);
    float radius = 0.0f;
    for (const auto& property : properties) {
        if (property.value("name", "") == "radius" && property.value("type", "") == "float") {
            radius = property.value("value", 0.0f);
        }
    }
    if (radius <= 0.0f) {
        spdlog::error("PointLight: 点光创建失败，缺少必要属性: radius: {}", radius);
        return;
    }
    spdlog::info("PointLight: 点光创建成功，radius: {}", radius);

    entity_id_ = registry_.create();
    if (auto name = object_json_->value("name", ""); !name.empty()) {
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
    }

    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<engine::component::TransformDirtyTag>(entity_id_);

    engine::component::PointLightComponent light{};
    light.radius = radius;
    registry_.emplace<engine::component::PointLightComponent>(entity_id_, light);

    applyTimeVisibilityTags(registry_, entity_id_, night_only, day_only, "PointLight");
}

void EntityBuilder::buildSpotLight() {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));
    const auto& properties = object_json_->value("properties", nlohmann::json::array_t());
    const auto [night_only, day_only] = parseTimeVisibilityFlags(properties);
    float radius = 0.0f;
    float inner_deg = 0.0f;
    float outer_deg = 0.0f;
    float direction_deg = 0.0f;
    for (const auto& property : properties) {
        if (property.value("name", "") == "spot" && property.value("type", "") == "class") {
            radius = property.value("/value/radius"_json_pointer, 0.0f);
            inner_deg = property.value("/value/inner_deg"_json_pointer, 0.0f);
            outer_deg = property.value("/value/outer_deg"_json_pointer, 0.0f);
            direction_deg = property.value("/value/direction_deg"_json_pointer, 0.0f);
        }
    }
    if (radius <= 0.0f || outer_deg <= 0.0f) {
        spdlog::error("SpotLight: 聚光创建失败，缺少必要属性: radius: {}, outer_deg: {}", radius, outer_deg);
        return;
    }
    spdlog::info("SpotLight: 聚光创建成功，radius: {}, outer_deg: {}, inner_deg: {}, direction_deg: {}", radius, outer_deg, inner_deg, direction_deg);

    entity_id_ = registry_.create();
    if (auto name = object_json_->value("name", ""); !name.empty()) {
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
    }

    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<engine::component::TransformDirtyTag>(entity_id_);

    const float radians = direction_deg * glm::pi<float>() / 180.0f;
    glm::vec2 direction{std::cos(radians), std::sin(radians)}; // 0° 向右，顺时针增加（y 向下）
    if (glm::length(direction) > 0.0f) {
        direction = glm::normalize(direction);
    } else {
        direction = {1.0f, 0.0f};
    }

    engine::component::SpotLightComponent light{};
    light.radius = radius;
    light.direction = direction;

    engine::utils::SpotLightOptions options{};
    if (inner_deg > 0.0f) {
        options.inner_angle_deg = inner_deg;
    }
    if (outer_deg > 0.0f) {
        options.outer_angle_deg = outer_deg;
    }
    if (options.inner_angle_deg > options.outer_angle_deg) {
        std::swap(options.inner_angle_deg, options.outer_angle_deg);
    }
    light.options = options;

    registry_.emplace<engine::component::SpotLightComponent>(entity_id_, light);

    applyTimeVisibilityTags(registry_, entity_id_, night_only, day_only, "SpotLight");
}

void EntityBuilder::buildEmissiveRect() {
    auto position = glm::vec2(object_json_->value("x", 0.0f), object_json_->value("y", 0.0f));
    auto size = glm::vec2(object_json_->value("width", 0.0f), object_json_->value("height", 0.0f));
    const auto& properties = object_json_->value("properties", nlohmann::json::array_t());
    const auto [night_only, day_only] = parseTimeVisibilityFlags(properties);
    float intensity = 1.0f;
    for (const auto& property : properties) {
        if (property.value("name", "") == "intensity" && property.value("type", "") == "float") {
            intensity = property.value("value", 1.0f);
        }
    }

    entity_id_ = registry_.create();
    if (auto name = object_json_->value("name", ""); !name.empty()) {
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
    }

    registry_.emplace<engine::component::TransformComponent>(entity_id_, position);
    registry_.emplace<engine::component::TransformDirtyTag>(entity_id_);

    engine::component::EmissiveRectComponent emissive{};
    emissive.size = size;
    emissive.params.intensity = intensity;
    registry_.emplace<engine::component::EmissiveRectComponent>(entity_id_, emissive);

    applyTimeVisibilityTags(registry_, entity_id_, night_only, day_only, "EmissiveRect");
}

}   // namespace game::loader
