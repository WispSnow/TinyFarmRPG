#include "auto_tile_library.h"
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <bit>
#include <unordered_map>
#include <vector>

namespace engine::resource {

void AutoTileLibrary::addRule(entt::hashed_string rule_hash_str, AutoTileRule rule) {
    rule.name_ = rule_hash_str.data();
    rules_.emplace(rule_hash_str.value(), std::move(rule));
}

void AutoTileLibrary::addRule(std::string_view rule_name, AutoTileRule rule) {
    rule.name_ = rule_name;
    rules_.emplace(entt::hashed_string(rule_name.data()).value(), std::move(rule));
}

void AutoTileLibrary::setSrcRect(entt::id_type rule_id, uint8_t mask, const engine::utils::Rect& rect) {
    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        spdlog::error("AutoTileLibrary::setSrcRect: 规则集 {} 不存在。", rule_id);
        return;
    }
    it->second.lookup_table_[mask] = rect;
    it->second.mask_defined_.set(mask);
    it->second.manual_mask_defined_.set(mask);
}

const engine::utils::Rect* AutoTileLibrary::getSrcRect(entt::id_type rule_id, uint8_t mask) const {
    auto it = rules_.find(rule_id);
    if (it == rules_.end()) return nullptr;
    return &it->second.lookup_table_[mask];
}

void AutoTileLibrary::setRuleTopology(entt::id_type rule_id, AutoTileTopology topology) {
    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        spdlog::error("AutoTileLibrary::setRuleTopology: 规则集 {} 不存在。", rule_id);
        return;
    }
    it->second.topology_ = topology;
}

bool AutoTileLibrary::hasRule(entt::id_type rule_id) const {
    return rules_.find(rule_id) != rules_.end();
}

const AutoTileRule* AutoTileLibrary::getRule(entt::id_type rule_id) const {
    auto it = rules_.find(rule_id);
    if (it == rules_.end()) return nullptr;
    return &it->second;
}

namespace {

// 位定义 (与 auto_tile_system.cpp 中的 NEIGHBOR_OFFSETS 顺序一致):
// 位0: 上,    位1: 右上,  位2: 右,    位3: 右下
// 位4: 下,    位5: 左下,  位6: 左,    位7: 左上
constexpr uint8_t EDGE_BITS = 0x55;   // 0b01010101 - 边缘位 (0,2,4,6)
constexpr uint8_t CORNER_BITS = 0xAA; // 0b10101010 - 角落位 (1,3,5,7)

// 角落位的相邻边缘位掩码：检查角落是否有效需要两边都为1
// 右上角(位1): 需要上(位0)和右(位2) -> 0b00000101
// 右下角(位3): 需要右(位2)和下(位4) -> 0b00010100
// 左下角(位5): 需要下(位4)和左(位6) -> 0b01010000
// 左上角(位7): 需要左(位6)和上(位0) -> 0b01000001
constexpr uint8_t CORNER_NE_EDGES = 0x05; // 右上角相邻边
constexpr uint8_t CORNER_SE_EDGES = 0x14; // 右下角相邻边
constexpr uint8_t CORNER_SW_EDGES = 0x50; // 左下角相邻边
constexpr uint8_t CORNER_NW_EDGES = 0x41; // 左上角相邻边

/**
 * @brief 规范化mask：根据拓扑类型移除无效/不相关的位
 * 
 * 对于Corner类型：只保留角落位，边缘位设为0
 * 对于Edge类型：只保留边缘位，角落位设为0
 * 对于Mixed类型：边缘位保留，角落位只在两相邻边都为1时保留
 */
uint8_t normalizeMask(uint8_t mask, AutoTileTopology topology) {
    switch (topology) {
        case AutoTileTopology::CORNER: {
            // Corner类型只关心角落，忽略边缘
            uint8_t normalized = 0;
            
            // 检查每个角落：只有当两个相邻边都为1时才保留该角落位
            if ((mask & CORNER_NE_EDGES) == CORNER_NE_EDGES) {
                normalized |= (mask & 0x02); // 右上角(位1)
            }
            if ((mask & CORNER_SE_EDGES) == CORNER_SE_EDGES) {
                normalized |= (mask & 0x08); // 右下角(位3)
            }
            if ((mask & CORNER_SW_EDGES) == CORNER_SW_EDGES) {
                normalized |= (mask & 0x20); // 左下角(位5)
            }
            if ((mask & CORNER_NW_EDGES) == CORNER_NW_EDGES) {
                normalized |= (mask & 0x80); // 左上角(位7)
            }
            
            return normalized;
        }
            
        case AutoTileTopology::EDGE:
            // Edge类型只关心边缘，忽略角落
            return mask & EDGE_BITS;
            
        case AutoTileTopology::MIXED:
        default: {
            // Mixed类型：角落只有在两个相邻边都存在时才有效
            uint8_t normalized = mask & EDGE_BITS; // 首先保留边缘位
            
            // 检查每个角落：只有当两个相邻边都为1时才保留该角落位
            if ((mask & CORNER_NE_EDGES) == CORNER_NE_EDGES) {
                normalized |= (mask & 0x02); // 右上角(位1)
            }
            if ((mask & CORNER_SE_EDGES) == CORNER_SE_EDGES) {
                normalized |= (mask & 0x08); // 右下角(位3)
            }
            if ((mask & CORNER_SW_EDGES) == CORNER_SW_EDGES) {
                normalized |= (mask & 0x20); // 左下角(位5)
            }
            if ((mask & CORNER_NW_EDGES) == CORNER_NW_EDGES) {
                normalized |= (mask & 0x80); // 左上角(位7)
            }
            
            return normalized;
        }
    }
}

/**
 * @brief 计算两个mask之间的匹配分数
 * 
 * 边缘位的权重高于角落位，因为边缘决定了图块的主要连接方向
 */
int computeMatchScore(uint8_t mask_a, uint8_t mask_b, AutoTileTopology topology) {
    // 对于Corner和Edge类型，所有相关位权重相同
    if (topology == AutoTileTopology::CORNER) {
        const uint8_t corner_match = ~(mask_a ^ mask_b) & CORNER_BITS;
        return std::popcount(static_cast<unsigned>(corner_match));
    }
    if (topology == AutoTileTopology::EDGE) {
        const uint8_t edge_match = ~(mask_a ^ mask_b) & EDGE_BITS;
        return std::popcount(static_cast<unsigned>(edge_match));
    }
    
    // 对于Mixed类型，边缘位权重是角落位的2倍
    const uint8_t edge_match = ~(mask_a ^ mask_b) & EDGE_BITS;
    const uint8_t corner_match = ~(mask_a ^ mask_b) & CORNER_BITS;
    return std::popcount(static_cast<unsigned>(edge_match)) * 2 + 
           std::popcount(static_cast<unsigned>(corner_match));
}

} // namespace

void AutoTileLibrary::fillMissingMasks(entt::id_type rule_id, AutoTileTopology topology) {
    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        spdlog::error("AutoTileLibrary::fillMissingMasks: 规则集 {} 不存在。", rule_id);
        return;
    }
    auto& rule = it->second;
    if (rule.mask_defined_.all()) {
        return;
    }

    // 收集手动定义的mask（原始数据）
    std::vector<uint8_t> existing_masks;
    existing_masks.reserve(256);
    for (uint16_t mask = 0; mask < 256; ++mask) {
        if (rule.manual_mask_defined_.test(mask)) {
            existing_masks.push_back(static_cast<uint8_t>(mask));
        }
    }
    if (existing_masks.empty()) {
        spdlog::warn("AutoTileLibrary::fillMissingMasks: 规则集 '{}' 没有可用的原始掩码。", rule.name_);
        return;
    }

    const AutoTileTopology effective_topology = (topology == AutoTileTopology::UNKNOWN) ? rule.topology_ : topology;
    
    // 为所有未定义的mask填充纹理
    for (uint16_t mask = 0; mask < 256; ++mask) {
        if (rule.mask_defined_.test(mask)) {
            continue;
        }
        
        // 规范化目标mask
        const uint8_t normalized_target = normalizeMask(static_cast<uint8_t>(mask), effective_topology);
        
        // 优先尝试精确匹配规范化后的mask
        if (auto it = std::find(existing_masks.begin(), existing_masks.end(), normalized_target); it != existing_masks.end()) {
            rule.lookup_table_[mask] = rule.lookup_table_[normalized_target];
            rule.mask_defined_.set(mask);
            continue;
        }
        
        // 如果没有精确匹配，寻找规范化后最接近的mask
        int best_score = -1;
        uint8_t best_mask = existing_masks.front();
        
        for (const auto mask_defined : existing_masks) {
            const int score = computeMatchScore(normalized_target, mask_defined, effective_topology);
            if (score > best_score) {
                best_score = score;
                best_mask = mask_defined;
            }
        }
        
        rule.lookup_table_[mask] = rule.lookup_table_[best_mask];
        rule.mask_defined_.set(mask);
    }
}

std::vector<AutoTileRuleDebugInfo> AutoTileLibrary::getDebugInfo() const {
    std::vector<AutoTileRuleDebugInfo> result;
    result.reserve(rules_.size());
    for (const auto& [rule_id, rule] : rules_) {
        AutoTileRuleDebugInfo info{};
        info.rule_id = rule_id;
        info.name = rule.name_;
        info.texture_id = rule.texture_id_;
        info.topology = rule.topology_;
        info.defined_mask_count = rule.mask_defined_.count();
        info.manual_mask_count = rule.manual_mask_defined_.count();
        if (info.defined_mask_count < 256) {
            info.missing_masks.reserve(256 - info.defined_mask_count);
            for (uint16_t mask = 0; mask < 256; ++mask) {
                if (!rule.mask_defined_.test(mask)) {
                    info.missing_masks.push_back(static_cast<uint8_t>(mask));
                }
            }
        }
        info.missing_mask_count = info.missing_masks.size();
        result.push_back(std::move(info));
    }
    return result;
}

void AutoTileLibrary::clearRules() {
    rules_.clear();
}

} // namespace engine::resource