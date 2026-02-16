#include "player_control_system.h"
#include "game/component/tags.h"
#include "game/component/state_component.h"
#include "game/component/actor_component.h"
#include "game/component/target_component.h"
#include "game/component/inventory_component.h"
#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include "game/defs/events.h"
#include "game/data/item_catalog.h"
#include "engine/component/velocity_component.h"
#include "engine/component/transform_component.h"
#include "engine/render/camera.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/component/tags.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace {

inline std::string toolToString(game::defs::Tool tool) {
    switch (tool) {
        case game::defs::Tool::Hoe: return "Hoe";
        case game::defs::Tool::WateringCan: return "Watering Can";
        case game::defs::Tool::Pickaxe: return "Pickaxe";
        case game::defs::Tool::Axe: return "Axe";
        case game::defs::Tool::Sickle: return "Sickle";
        case game::defs::Tool::None: return "None";
        default: return "Unknown";
    }
}

inline game::component::Direction resolveDirection(glm::vec2 direction) {
    // 根据方向计算朝向，优先选择主要方向（绝对值更大的轴）
    const auto abs_x = std::abs(direction.x);
    const auto abs_y = std::abs(direction.y);
    
    if (abs_x > abs_y) {
        // 水平方向为主
        return direction.x < 0.0f 
            ? game::component::Direction::Left 
            : game::component::Direction::Right;
    } else {
        // 垂直方向为主
        return direction.y < 0.0f 
            ? game::component::Direction::Up 
            : game::component::Direction::Down;
    }
}

inline game::component::Action resolveAction(game::defs::Tool tool) {
    switch (tool) {
        case game::defs::Tool::Hoe: return game::component::Action::Hoe;
        case game::defs::Tool::WateringCan: return game::component::Action::Watering;
        case game::defs::Tool::Sickle: return game::component::Action::Sickle;
        case game::defs::Tool::Pickaxe: return game::component::Action::Pickaxe;
        case game::defs::Tool::Axe: return game::component::Action::Axe;
        case game::defs::Tool::None: return game::component::Action::Idle;
        default: return game::component::Action::Idle;
    }
}

} // namespace

namespace game::system {

PlayerControlSystem::~PlayerControlSystem() {
    input_manager_.onAction("mouse_left"_hs).disconnect<&PlayerControlSystem::onMouseLeftClick>(this);
    input_manager_.onAction("mouse_right"_hs).disconnect<&PlayerControlSystem::onMouseRightClick>(this);
    disconnectHotbarSlots(std::make_index_sequence<10>{});  // 展开为 index_sequence<0,1,...,9>
    input_manager_.onAction("player_light"_hs).disconnect<&PlayerControlSystem::onPlayerLightToggle>(this);
    dispatcher_.disconnect(this);
}

PlayerControlSystem::PlayerControlSystem(entt::registry& registry,
    entt::dispatcher& dispatcher,
    engine::input::InputManager& input_manager,
    engine::render::Camera& camera,
    engine::spatial::SpatialIndexManager& spatial_index_manager,
    game::data::ItemCatalog* item_catalog)
    : registry_(registry), dispatcher_(dispatcher), input_manager_(input_manager), camera_(camera), spatial_index_manager_(spatial_index_manager), item_catalog_(item_catalog) {
    input_manager_.onAction("mouse_left"_hs).connect<&PlayerControlSystem::onMouseLeftClick>(this);
    input_manager_.onAction("mouse_right"_hs).connect<&PlayerControlSystem::onMouseRightClick>(this);
    connectHotbarSlots(std::make_index_sequence<10>{});  // 展开为 index_sequence<0,1,...,9>
    input_manager_.onAction("player_light"_hs).connect<&PlayerControlSystem::onPlayerLightToggle>(this);
    dispatcher_.sink<game::defs::SwitchToolEvent>().connect<&PlayerControlSystem::onSwitchToolEvent>(this);
    dispatcher_.sink<game::defs::SwitchSeedEvent>().connect<&PlayerControlSystem::onSwitchSeedEvent>(this);
    dispatcher_.sink<game::defs::HotbarActivateRequest>().connect<&PlayerControlSystem::onHotbarActivateRequest>(this);
    dispatcher_.sink<game::defs::HotbarChanged>().connect<&PlayerControlSystem::onHotbarChanged>(this);
}

bool PlayerControlSystem::onPlayerLightToggle() {
    dispatcher_.trigger(game::defs::ToggleLightRequest{"player_follow_light"_hs});
    return true;
}

void PlayerControlSystem::update(float /* delta_time */) {
    if (player_entity_ == entt::null) {
        if (auto view = registry_.view<game::component::PlayerTag>(); view.size() == 1) {
            player_entity_ = *view.begin();
            if (!registry_.valid(player_entity_)) return;
        }
    }
    if (!registry_.valid(player_entity_)) return;

    ensureHotbar();

    if (target_entity_ == entt::null) {
        if (auto view = registry_.view<game::component::TargetComponent>(); view.size() == 1) {
            target_entity_ = *view.begin();
            if (!registry_.valid(target_entity_)) return;
        }
    }

    // 每帧流水线：
    // 1) logical mouse -> world mouse（用于目标选择/交互）
    // 2) move_* -> velocity/state（用于移动与动画）
    // 3) tool/seed + mouse tile -> target cursor（用于“玩家意图”的可视化）
    const auto mouse_world_position = computeMouseWorldPosition();
    updateMovementIntent();
    updateTargetAndSelection(mouse_world_position);
}

glm::vec2 PlayerControlSystem::computeMouseWorldPosition() const {
    const auto logical_mouse_position = input_manager_.getLogicalMousePosition();
    return camera_.screenToWorld(logical_mouse_position);
}

void PlayerControlSystem::updateMovementIntent() {
    auto [velocity, state] = registry_.get<engine::component::VelocityComponent,
                                          game::component::StateComponent>(player_entity_);
    if (registry_.all_of<game::component::ActionLockedTag>(player_entity_)) {
        velocity.velocity_ = glm::vec2(0.0f, 0.0f);
        return;
    }
    const auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);

    // 计算移动方向（归一化向量）
    const auto move_direction = getMoveDirection();
    // 设置速度（每秒移动的距离，不包含 delta_time）
    velocity.velocity_ = move_direction * actor.speed_;
    
    // 计算新的动作和方向
    auto new_action = state.action_;
    auto new_direction = state.direction_;
    
    if (glm::length(move_direction) > 1.0e-5f) {
        new_action = game::component::Action::Walk;
        new_direction = resolveDirection(move_direction);
    } else {
        new_action = game::component::Action::Idle;
    }
    
    // 如果状态更新，需要标记状态变化
    if (new_action != state.action_ || new_direction != state.direction_) {
        state.action_ = new_action;
        state.direction_ = new_direction;
        registry_.emplace_or_replace<game::component::StateDirtyTag>(player_entity_);
    }
}

void PlayerControlSystem::updateTargetAndSelection(glm::vec2 mouse_world_position) {
    if (!registry_.valid(player_entity_) || !registry_.valid(target_entity_)) return;

    // 动作锁定中保持当前显示状态（目标不刷新，但也不在此处变更显示）
    if (registry_.all_of<game::component::ActionLockedTag>(player_entity_)) return;
    
    const auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);
    
    const bool has_seed = actor.hold_seed_ != game::defs::CropType::Unknown;
    const bool has_tool = actor.tool_ != game::defs::Tool::None;

    if (!has_seed && !has_tool) {
        registry_.emplace_or_replace<engine::component::InvisibleTag>(target_entity_);
        return;
    }

    // 只有鼠标在可操作范围内才显示目标
    auto target_center = resolveTargetPosition(mouse_world_position);
    if (!target_center) {
        registry_.emplace_or_replace<engine::component::InvisibleTag>(target_entity_);
        return;
    }

    syncTargetComponent(*target_center);
}

void PlayerControlSystem::syncTargetComponent(glm::vec2 target_world_center) {
    if (!registry_.valid(target_entity_)) return;

    registry_.remove<engine::component::InvisibleTag>(target_entity_);
    auto& target_component = registry_.get<game::component::TargetComponent>(target_entity_);
    target_component.position_ = spatial_index_manager_.getRectAtWorldPos(target_world_center).pos;
}

glm::vec2 PlayerControlSystem::getMoveDirection() const {
    glm::vec2 direction = glm::vec2(0.0f, 0.0f);
    if (input_manager_.isActionDown("move_up"_hs)) {
        direction.y -= 1.0f;
    }
    if (input_manager_.isActionDown("move_down"_hs)) {
        direction.y += 1.0f;
    }
    if (input_manager_.isActionDown("move_left"_hs)) {
        direction.x -= 1.0f;
    }
    if (input_manager_.isActionDown("move_right"_hs)) {
        direction.x += 1.0f;
    }
    return glm::length(direction) > 1.0e-5f ? glm::normalize(direction) : glm::vec2(0.0f, 0.0f);
}

std::optional<glm::vec2> PlayerControlSystem::resolveTargetPosition(glm::vec2 mouse_world_position) const {
    if (!registry_.valid(player_entity_)) return std::nullopt;

    const auto& player_transform = registry_.get<engine::component::TransformComponent>(player_entity_);
    const auto player_tile = spatial_index_manager_.getTileCoordAtWorldPos(player_transform.position_);
    const auto mouse_tile = spatial_index_manager_.getTileCoordAtWorldPos(mouse_world_position);

    const auto dx = mouse_tile.x - player_tile.x;
    const auto dy = mouse_tile.y - player_tile.y;
    if (std::abs(dx) > game::defs::TOOL_TARGET_TILE_RANGE || std::abs(dy) > game::defs::TOOL_TARGET_TILE_RANGE) {
        return std::nullopt;
    }

    const auto tile_rect = spatial_index_manager_.getRectAtWorldPos(mouse_world_position);
    return tile_rect.pos + tile_rect.size * 0.5f;
}

bool PlayerControlSystem::ensureHotbar() {
    if (hotbar_) return true;
    if (registry_.valid(player_entity_) && registry_.all_of<game::component::HotbarComponent>(player_entity_)) {
        hotbar_ = &registry_.get<game::component::HotbarComponent>(player_entity_);
    }
    return hotbar_ != nullptr;
}

void PlayerControlSystem::clearSelection() {
    dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::None});
    dispatcher_.trigger(game::defs::SwitchSeedEvent{game::defs::CropType::Unknown, entt::null, -1});
    // UI 高亮与“当前可用工具/物品”保持一致：清空选择时不高亮任何快捷栏槽位
    dispatcher_.trigger(game::defs::HotbarSlotChanged{player_entity_, -1});
}

void PlayerControlSystem::applySelectionForActiveSlot() {
    if (!registry_.valid(player_entity_) || !ensureHotbar()) {
        clearSelection();
        return;
    }
    if (!registry_.all_of<game::component::InventoryComponent>(player_entity_)) {
        clearSelection();
        return;
    }
    if (item_catalog_ == nullptr) {
        clearSelection();
        return;
    }

    const auto& inventory = registry_.get<game::component::InventoryComponent>(player_entity_);
    const int inventory_slot_index = hotbar_->getActiveInventorySlot();
    if (inventory_slot_index < 0 || inventory_slot_index >= inventory.slotCount()) {
        clearSelection();
        return;
    }

    const auto& stack = inventory.slot(inventory_slot_index);
    if (stack.empty()) {
        clearSelection();
        return;
    }

    const auto* item = item_catalog_->findItem(stack.item_id_);
    if (!item) {
        clearSelection();
        return;
    }

    if (item->category_ == game::data::ItemCategory::Tool && item->tool_type_ != game::defs::Tool::None) {
        dispatcher_.trigger(game::defs::SwitchSeedEvent{game::defs::CropType::Unknown, entt::null, -1});
        dispatcher_.trigger(game::defs::SwitchToolEvent{item->tool_type_});
    } else if (item->category_ == game::data::ItemCategory::Seed && item->crop_type_ != game::defs::CropType::Unknown) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::None});
        dispatcher_.trigger(game::defs::SwitchSeedEvent{item->crop_type_, stack.item_id_, inventory_slot_index});
    } else {
        clearSelection();
    }
}

bool PlayerControlSystem::hasUsableSeedStack() const {
    if (!registry_.valid(player_entity_)) return false;
    if (!registry_.all_of<game::component::ActorComponent, game::component::InventoryComponent>(player_entity_)) return false;
    const auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(player_entity_);
    if (actor.hold_seed_ == game::defs::CropType::Unknown) return false;
    if (actor.hold_seed_inventory_slot_ < 0 || actor.hold_seed_inventory_slot_ >= inventory.slotCount()) return false;
    const auto& stack = inventory.slot(actor.hold_seed_inventory_slot_);
    if (stack.empty() || stack.count_ <= 0) return false;
    if (actor.hold_seed_item_id_ != entt::null && stack.item_id_ != actor.hold_seed_item_id_) return false;
    return true;
}

bool PlayerControlSystem::onMouseLeftClick() {
    if (!registry_.valid(player_entity_)) return true;
    if (!registry_.valid(target_entity_)) return true;

    if (registry_.all_of<game::component::ActionLockedTag>(player_entity_)) {
        return true; // 动作锁定中，忽略点击
    }
    
    // 以点击瞬间的鼠标位置为准（避免目标位置滞后一帧）
    const auto mouse_world_position = computeMouseWorldPosition();
    
    const auto& player_transform = registry_.get<engine::component::TransformComponent>(player_entity_);
    auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);
    auto& state = registry_.get<game::component::StateComponent>(player_entity_);

    // 没有激活工具/种子则不触发任何动作
    if (actor.hold_seed_ == game::defs::CropType::Unknown && actor.tool_ == game::defs::Tool::None) {
        return true;
    }

    // 只有鼠标在可操作范围内才允许激活动作
    const auto target_center = resolveTargetPosition(mouse_world_position);
    if (!target_center) {
        return true;
    }

    // 立刻更新 TargetComponent（避免在 ActionLockedTag 后无法刷新 target，导致命中位置滞后）
    syncTargetComponent(*target_center);

    // 更新朝向
    glm::vec2 direction = *target_center - player_transform.position_;
    if (glm::length(direction) > 1.0e-5f) {
        direction = glm::normalize(direction);
        state.direction_ = resolveDirection(direction);
    }
    
    // 优先级1: 检查是否有种子（种子优先于工具，否则“拿着种子却挥锄头”的体验会很奇怪）
    if (actor.hold_seed_ != game::defs::CropType::Unknown) {
        if (!hasUsableSeedStack()) {
            spdlog::warn("尝试种植但当前激活槽已无可用种子，清空选择");
            clearSelection();
            return true;
        }
        registry_.emplace<game::component::ActionLockedTag>(player_entity_);
        registry_.emplace_or_replace<game::component::StateDirtyTag>(player_entity_);
        state.action_ = game::component::Action::Planting;
        // 动画事件会在 "seed_plant" 时触发 UseSeedEvent
        spdlog::info("触发种植动作，种子类型: {}", static_cast<int>(actor.hold_seed_));
        return true;
    }
    
    // 优先级2: 检查是否有工具
    if (actor.tool_ != game::defs::Tool::None) {
        registry_.emplace<game::component::ActionLockedTag>(player_entity_);
        registry_.emplace_or_replace<game::component::StateDirtyTag>(player_entity_);
        state.action_ = resolveAction(actor.tool_);
        // 动画事件会在 "tool_hit" 时触发 UseToolEvent
        spdlog::info("触发工具动作，工具类型: {}", static_cast<int>(actor.tool_));
        return true;
    }
    
    // 优先级3: 空手不做动作
    return true;
}

bool PlayerControlSystem::onMouseRightClick() {
    // 取消当前激活的工具/物品（工具 + 种子）
    clearSelection();
    return true;
}

void PlayerControlSystem::onSwitchToolEvent(const game::defs::SwitchToolEvent& event) {
    spdlog::info("尝试切换工具: {}", toolToString(event.tool_));
    if (!registry_.valid(player_entity_) || !registry_.valid(target_entity_)) return;

    auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);
    actor.tool_ = event.tool_;
}

void PlayerControlSystem::onSwitchSeedEvent(const game::defs::SwitchSeedEvent& event) {
    spdlog::info("尝试切换种子: {}", static_cast<int>(event.seed_type_));
    if (!registry_.valid(player_entity_) || !registry_.valid(target_entity_)) return;

    auto& actor = registry_.get<game::component::ActorComponent>(player_entity_);
    actor.hold_seed_ = event.seed_type_;
    actor.hold_seed_item_id_ = event.seed_item_id_;
    actor.hold_seed_inventory_slot_ = event.inventory_slot_index_;

    if (event.seed_type_ == game::defs::CropType::Unknown) {
        actor.hold_seed_item_id_ = entt::null;
        actor.hold_seed_inventory_slot_ = -1;
    }
}

void PlayerControlSystem::switchToHotbarSlot(int slot_index) {
    if (!registry_.valid(player_entity_)) return;
    if (!ensureHotbar()) return;
    if (slot_index < 0 || slot_index >= game::component::HotbarComponent::SLOT_COUNT) return;

    // 更新快捷栏激活槽位
    hotbar_->setActiveSlot(slot_index);

    // 触发激活槽位切换事件
    dispatcher_.trigger(game::defs::HotbarSlotChanged{player_entity_, slot_index});

    // 根据当前激活槽位内容切换工具/种子
    applySelectionForActiveSlot();
}

void PlayerControlSystem::onHotbarActivateRequest(const game::defs::HotbarActivateRequest& event) {
    if (!registry_.valid(player_entity_)) return;
    if (event.target != player_entity_) return;
    switchToHotbarSlot(event.hotbar_index);
}

void PlayerControlSystem::onHotbarChanged(const game::defs::HotbarChanged& event) {
    if (!registry_.valid(player_entity_)) return;
    if (event.target != player_entity_) return;
    if (!ensureHotbar()) return;

    bool affects_active = event.full_sync;
    const int active_slot = hotbar_->active_slot_index_;
    if (!affects_active) {
        affects_active = std::any_of(event.slots.begin(), event.slots.end(), [active_slot](const auto& s) {
            return s.hotbar_index == active_slot;
        });
    }
    if (affects_active) {
        applySelectionForActiveSlot();
    }
}

} // namespace game::system
