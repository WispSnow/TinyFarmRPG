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

struct AddItemRequest {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int count{0};
    int preferred_slot_index{-1};   ///< @brief 如果>=0，优先尝试放入指定槽位
};

struct RemoveItemRequest {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int count{0};
    int slot_index{-1};   ///< @brief 如果>=0，仅从指定槽位扣除
};

struct UseItemRequest {
    entt::entity target{entt::null};
    int inventory_slot_index{-1};
    int count{1};
    bool show_prompt{false};        ///< @brief 是否需要弹出提示（物品栏=true，快捷栏=false）
};

struct InventorySyncRequest {
    entt::entity target{entt::null};
};

struct InventoryMoveRequest {
    entt::entity target{entt::null};
    int from_slot{-1};
    int to_slot{-1};
    bool allow_merge{true};
};

struct InventorySetActivePageRequest {
    entt::entity target{entt::null};
    int active_page{0};
};

struct InventorySlotUpdate {
    int slot_index{0};
    entt::id_type item_id{entt::null};
    int count{0};
};

struct InventoryChanged {
    entt::entity target{entt::null};
    std::vector<InventorySlotUpdate> slots{};
    bool full_sync{false};
    int active_page{0};
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

struct HotbarBindRequest {
    entt::entity target{entt::null};
    int hotbar_index{-1};
    int inventory_slot{-1};
};

struct HotbarUnbindRequest {
    entt::entity target{entt::null};
    int hotbar_index{-1};
};

struct HotbarActivateRequest {
    entt::entity target{entt::null};
    int hotbar_index{-1};
};

struct HotbarSyncRequest {
    entt::entity target{entt::null};
    bool full_sync{true};
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

// 交互请求（InteractionSystem 的“扩展点”）
// - InteractionSystem 只负责：选目标 + 发请求（不写具体交互逻辑）
// - 具体交互由订阅者系统各自处理（Dialogue/Chest/Rest...）
// - 想加新交互对象时：优先新增订阅者，而不是改 InteractionSystem
struct InteractRequest {
    entt::entity player{entt::null};
    entt::entity target{entt::null};
};

struct AdvanceTimeRequest {
    int hours{0};
};

struct ToggleLightRequest {
    entt::id_type light_type_id{entt::null};
};

} // namespace game::defs
