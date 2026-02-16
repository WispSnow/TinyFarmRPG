#pragma once

#include "game/defs/events.h"
#include "game/defs/constants.h"
#include "game/component/hotbar_component.h"
#include "game/data/item_catalog.h"
#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <glm/vec2.hpp>
#include <entt/entity/entity.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/fwd.hpp>

#include "engine/input/input_manager.h"

namespace engine::render {
    class Camera;
}

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace game::system {

class PlayerControlSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::input::InputManager& input_manager_;
    engine::render::Camera& camera_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
    game::data::ItemCatalog* item_catalog_{nullptr};

    // 缓存和玩家相关的关键变量
    entt::entity player_entity_{entt::null};
    entt::entity target_entity_{entt::null};
    game::component::HotbarComponent* hotbar_{nullptr};  ///< @brief 快捷栏组件引用

public:
    PlayerControlSystem(entt::registry& registry, 
        entt::dispatcher& dispatcher, 
        engine::input::InputManager& input_manager, 
        engine::render::Camera& camera,
        engine::spatial::SpatialIndexManager& spatial_index_manager,
        game::data::ItemCatalog* item_catalog);
    ~PlayerControlSystem();
    void update(float delta_time);

private:

    /// @brief 每帧流水线：鼠标 world 坐标 -> 移动意图 -> 目标选择/显示
    [[nodiscard]] glm::vec2 computeMouseWorldPosition() const;
    void updateMovementIntent();
    void updateTargetAndSelection(glm::vec2 mouse_world_position);
    void syncTargetComponent(glm::vec2 target_world_center);

    glm::vec2 getMoveDirection() const;
    std::optional<glm::vec2> resolveTargetPosition(glm::vec2 mouse_world_position) const;

    // 输入控制回调函数
    bool onMouseLeftClick();
    bool onMouseRightClick();
    bool onPlayerLightToggle();

    // 快捷栏动作 ID 表（与 config/input.json 中的 hotbar_1..hotbar_10 对齐）
    static constexpr std::array<entt::id_type, 10> HOTBAR_ACTION_IDS = {
        entt::hashed_string{"hotbar_1"}.value(),
        entt::hashed_string{"hotbar_2"}.value(),
        entt::hashed_string{"hotbar_3"}.value(),
        entt::hashed_string{"hotbar_4"}.value(),
        entt::hashed_string{"hotbar_5"}.value(),
        entt::hashed_string{"hotbar_6"}.value(),
        entt::hashed_string{"hotbar_7"}.value(),
        entt::hashed_string{"hotbar_8"}.value(),
        entt::hashed_string{"hotbar_9"}.value(),
        entt::hashed_string{"hotbar_10"}.value(),
    };

    /// 快捷栏槽位回调：N 为编译期常量，用于生成 10 个不同的函数指针，供 connect 分别绑定。
    template <int N>
    bool onHotbarSlot() { switchToHotbarSlot(N); return true; }

    /// 批量连接 10 个快捷栏输入（调用处传 std::make_index_sequence<10>{}）。
    /// Is... 在编译期展开为 0,1,...,9；逗号折叠 (expr, ...) 对每个 Is 执行一次 connect。
    template <std::size_t... Is>
    void connectHotbarSlots(std::index_sequence<Is...>) {
        (input_manager_.onAction(HOTBAR_ACTION_IDS[Is])
            .template connect<&PlayerControlSystem::onHotbarSlot<static_cast<int>(Is)>>(this), ...);
    }

    /// 批量断开 10 个快捷栏输入，与 connectHotbarSlots 对称。
    template <std::size_t... Is>
    void disconnectHotbarSlots(std::index_sequence<Is...>) {
        (input_manager_.onAction(HOTBAR_ACTION_IDS[Is])
            .template disconnect<&PlayerControlSystem::onHotbarSlot<static_cast<int>(Is)>>(this), ...);
    }

    // 快捷栏相关方法
    void switchToHotbarSlot(int slot_index);
    void applySelectionForActiveSlot();
    void clearSelection();
    bool ensureHotbar();
    bool hasUsableSeedStack() const;

    // 事件回调函数
    void onSwitchToolEvent(const game::defs::SwitchToolEvent& event);
    void onSwitchSeedEvent(const game::defs::SwitchSeedEvent& event);
    void onHotbarActivateRequest(const game::defs::HotbarActivateRequest& event);
    void onHotbarChanged(const game::defs::HotbarChanged& event);
};

} // namespace game::system
