#pragma once
#include "engine/utils/math.h"
#include "resource_debug_info.h"
#include <entt/entity/entity.hpp>
#include <entt/core/hashed_string.hpp>
#include <array>
#include <bitset>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace engine::resource {

enum class AutoTileTopology : std::uint8_t {
    CORNER,
    EDGE,
    MIXED,
    UNKNOWN,
};

// 一个自动图块规则集
struct AutoTileRule {
    entt::id_type texture_id_{entt::null};
    std::string name_;
    // 索引是 8-bit Mask (0-255)
    // 值是对应的纹理源矩形
    // 使用 array 而不是 map，因为索引是连续的且固定为 256 个，访问速度 O(1) 且缓存友好
    std::array<engine::utils::Rect, 256> lookup_table_{};
    std::bitset<256> mask_defined_;
    std::bitset<256> manual_mask_defined_;
    AutoTileTopology topology_{AutoTileTopology::UNKNOWN};

    explicit AutoTileRule(entt::id_type texture_id, std::string_view name = "") : texture_id_(texture_id), name_(name) {}
};

class AutoTileLibrary {
    std::unordered_map<entt::id_type, AutoTileRule> rules_;     ///< @brief 规则集

public:
    // 添加一个规则集
    void addRule(entt::hashed_string rule_hash_str, AutoTileRule rule);
    void addRule(std::string_view rule_name, AutoTileRule rule);

    // 设置/查询接口
    void setSrcRect(entt::id_type rule_id, uint8_t mask, const engine::utils::Rect& rect);
    const engine::utils::Rect* getSrcRect(entt::id_type rule_id, uint8_t mask) const;
    void setRuleTopology(entt::id_type rule_id, AutoTileTopology topology);

    // 检查规则是否存在
    bool hasRule(entt::id_type rule_id) const;

    // 获取规则
    const AutoTileRule* getRule(entt::id_type rule_id) const;
    void fillMissingMasks(entt::id_type rule_id, AutoTileTopology topology);
    std::vector<AutoTileRuleDebugInfo> getDebugInfo() const;

    // 清空规则集
    void clearRules();
};

} // namespace engine::resource