#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <nlohmann/json_fwd.hpp>

namespace game::data {

/**
 * @brief 一天中的时段枚举
 */
enum class TimeOfDay : std::uint8_t {
    Dawn = 0,    ///< 黎明（4:00-8:00）
    Day,         ///< 白天（8:00-16:00）
    Dusk,        ///< 黄昏（16:00-20:00）
    Night,       ///< 夜晚（20:00-4:00）
    Count
};

/**
 * @brief 时间配置结构，用于从配置文件加载
 */
struct TimeConfig {
    float minutes_per_real_second_{1.0f};    ///< 每秒游戏时间流逝的分钟数
    float dawn_start_hour_{4.0f};            ///< 黎明开始时间（小时）
    float dawn_end_hour_{8.0f};              ///< 黎明结束时间（小时）
    float day_start_hour_{8.0f};             ///< 白天开始时间（小时）
    float day_end_hour_{16.0f};              ///< 白天结束时间（小时）
    float dusk_start_hour_{16.0f};           ///< 黄昏开始时间（小时）
    float dusk_end_hour_{20.0f};             ///< 黄昏结束时间（小时）
    float night_start_hour_{20.0f};          ///< 夜晚开始时间（小时）
    float night_end_hour_{4.0f};             ///< 夜晚结束时间（小时，可能跨天）
};

/**
 * @brief 游戏时间数据结构
 * 
 * 存储游戏内时间状态，包括当前时间、时间缩放、暂停状态等。
 * 通过 registry.ctx<GameTime>() 存储在注册表上下文中。
 */
struct GameTime {
    // 当前时间
    std::uint32_t day_{1};           ///< 当前天数（从1开始）
    float hour_{6.0f};               ///< 当前小时（0.0-24.0）
    float minute_{0.0f};             ///< 当前分钟（0.0-60.0）
    
    // 时间控制
    float time_scale_{1.0f};         ///< 时间缩放因子（1.0为正常速度）
    bool paused_{false};             ///< 是否暂停
    
    // 当前时段
    TimeOfDay time_of_day_{TimeOfDay::Dawn};  ///< 当前时段
    
    // 配置
    TimeConfig config_;              ///< 时间配置

    // 视觉规则：自发光/灯光显隐窗口（与玩法 TimeOfDay 不同概念）
    float emissive_dark_start_hour_{18.0f};   ///< 自发光开启窗口起始（小时，允许跨天）
    float emissive_dark_end_hour_{6.0f};      ///< 自发光开启窗口结束（小时，允许跨天）

    /**
     * @brief 从配置文件加载并创建 GameTime 实例
     * 
     * @param config_path 配置文件路径
     * @return std::shared_ptr<GameTime> 创建的 GameTime 实例，失败返回 nullptr
     */
    static std::shared_ptr<GameTime> loadFromConfig(std::string_view config_path);

    /**
     * @brief 从 JSON 对象加载配置
     * 
     * @param json JSON 对象
     * @return true 加载成功
     * @return false 加载失败
     */
    bool loadConfigFromJson(const nlohmann::json& json);

    /**
     * @brief 从 light_config.json 加载“自发光显隐”时间窗口（可选）
     */
    bool loadEmissiveVisibilityFromLightConfig(std::string_view config_path);

    /**
     * @brief 根据当前小时计算时段
     * 
     * @param hour 小时（0.0-24.0）
     * @return TimeOfDay 时段
     */
    [[nodiscard]] TimeOfDay calculateTimeOfDay(float hour) const;

    /**
     * @brief 当前时间是否处于“视觉上足够暗”的窗口（用于 NightOnly/DayOnly 光源显隐）
     */
    [[nodiscard]] bool isDarkForEmissives() const;

    /**
     * @brief 获取格式化的时间字符串
     * 
     * @return std::string 格式化的时间字符串（如 "Day 1, 06:30"）
     */
    [[nodiscard]] std::string getFormattedTime() const;
};

} // namespace game::data
