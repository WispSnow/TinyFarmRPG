#pragma once

#include "constants.h"
#include "crop_defs.h"
#include <glm/vec2.hpp>
#include <string>
#include <cstdint>
#include <entt/entity/entity.hpp>
#include <vector>

namespace game::defs {

struct UseToolEvent {
    Tool tool_{Tool::None};
    glm::vec2 world_pos_{0.0f, 0.0f};
};

struct SwitchToolEvent {
    Tool tool_{Tool::None};
};

struct SwitchSeedEvent {
    CropType seed_type_{CropType::Unknown};           ///< @brief 要切换到的种子类型，Unknown表示取消种子
    entt::id_type seed_item_id_{entt::null};          ///< @brief 当前种子物品ID
    int inventory_slot_index_{-1};                    ///< @brief 当前种子所在的物品栏槽位
};

struct UseSeedEvent {
    CropType seed_type_{CropType::Unknown};
    glm::vec2 world_pos_{0.0f, 0.0f};
    entt::entity source{entt::null};                  ///< @brief 发起者（通常是玩家）
    entt::id_type seed_item_id_{entt::null};          ///< @brief 被使用的种子物品ID
    int inventory_slot_index_{-1};                    ///< @brief 种子所在的物品栏槽位
};

struct InventorySlotUpdate {
    int slot_index{0};
    entt::id_type item_id{entt::null};
    int count{0};
};

enum class InventoryMoveKind : std::uint8_t {
    None = 0,
    MoveToEmpty,
    Swap,
    Merge
};

struct InventoryChanged {
    entt::entity target{entt::null};
    std::vector<InventorySlotUpdate> slots{};
    bool full_sync{false};
    int active_page{0};
    bool from_add{false};   ///< @brief 是否由“加物品”语义触发，用于保持 Hotbar 自动绑定策略稳定
    InventoryMoveKind move_kind{InventoryMoveKind::None}; ///< @brief move 语义（仅 onMoveItem 路径设置）
    int move_from_slot{-1};                               ///< @brief move 源槽位
    int move_to_slot{-1};                                 ///< @brief move 目标槽位
};

struct InventoryFullEvent {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int rejected{0};
};

struct HotbarSlotChanged {
    entt::entity target{entt::null};
    int slot_index{0};  ///< 高亮的槽位索引；-1 表示当前不高亮任何槽位
};

struct HotbarSlotUpdate {
    int hotbar_index{0};
    int inventory_slot_index{-1};
    entt::id_type item_id{entt::null};
    int count{0};
};

struct HotbarChanged {
    entt::entity target{entt::null};
    std::vector<HotbarSlotUpdate> slots{};
    bool full_sync{false};
    int active_slot{0};
};

struct DialogueShowEvent {
    entt::entity target{entt::null};
    std::string speaker{};
    std::string text{};
    glm::vec2 world_position{0.0f};
    std::uint8_t channel{0};   ///< 0=对话，1=通知（如拾取/开箱），2=物品提示（如使用/消耗）
};

struct DialogueMoveEvent {
    entt::entity target{entt::null};
    glm::vec2 world_position{0.0f};
    std::uint8_t channel{0};   ///< 0=对话，1=通知，2=物品提示
};

struct DialogueHideEvent {
    entt::entity target{entt::null};
    std::uint8_t channel{0};   ///< 0=对话，1=通知，2=物品提示
};

struct AdvanceTimeRequest {
    int hours{0};
};

struct ToggleLightRequest {
    entt::id_type light_type_id{entt::null};
};

} // namespace game::defs
