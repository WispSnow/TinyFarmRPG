#include "game_time.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <format>
#include <cmath>
#include <spdlog/spdlog.h>

namespace game::data {

std::shared_ptr<GameTime> GameTime::loadFromConfig(std::string_view config_path) {
    auto game_time = std::make_shared<GameTime>();
    
    auto path = std::filesystem::path(config_path);
    std::ifstream file(path);
    
    if (!file.is_open()) {
        spdlog::warn("游戏时间配置文件 '{}' 未找到，使用默认配置。", config_path);
        return game_time;  // 返回使用默认配置的实例
    }

    try {
        nlohmann::json json;
        file >> json;
        
        if (!game_time->loadConfigFromJson(json)) {
            spdlog::warn("加载游戏时间配置失败，使用默认配置。");
            return game_time;
        }
        
        spdlog::info("成功从 '{}' 加载游戏时间配置。", config_path);
        return game_time;
    } catch (const std::exception& e) {
        spdlog::error("读取游戏时间配置文件 '{}' 时出错：{}。使用默认配置。", config_path, e.what());
        return game_time;
    }
}

bool GameTime::loadConfigFromJson(const nlohmann::json& json) {
    try {
        // 加载时间流逝速度
        if (json.contains("minutes_per_real_second")) {
            config_.minutes_per_real_second_ = json.at("minutes_per_real_second").get<float>();
        }
        
        // 加载各时段时间点
        if (json.contains("time_periods")) {
            const auto& periods = json.at("time_periods");

            auto loadPeriod = [&](const char* name, float& start, float& end) {
                if (periods.contains(name)) {
                    const auto& p = periods.at(name);
                    start = p.value("start", start);
                    end   = p.value("end", end);
                }
            };

            loadPeriod("dawn",  config_.dawn_start_hour_,  config_.dawn_end_hour_);
            loadPeriod("day",   config_.day_start_hour_,   config_.day_end_hour_);
            loadPeriod("dusk",  config_.dusk_start_hour_,  config_.dusk_end_hour_);
            loadPeriod("night", config_.night_start_hour_, config_.night_end_hour_);
        }
        
        // 加载初始时间（可选）
        if (json.contains("initial_time")) {
            const auto& initial = json.at("initial_time");
            day_ = initial.value("day", 1u);
            hour_ = initial.value("hour", 6.0f);
            minute_ = initial.value("minute", 0.0f);
        }
        
        // 计算初始时段
        time_of_day_ = calculateTimeOfDay(hour_);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("解析游戏时间配置 JSON 时出错：{}", e.what());
        return false;
    }
}

bool GameTime::loadEmissiveVisibilityFromLightConfig(std::string_view config_path) {
    auto path = std::filesystem::path(config_path);
    std::ifstream file(path);

    if (!file.is_open()) {
        spdlog::warn("光照配置文件 '{}' 未找到，自发光显隐将使用默认时间窗口。", config_path);
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;

        const auto& visibility = json.value("emissive_visibility", nlohmann::json::object());
        const auto& dark_time = visibility.value("dark_time", nlohmann::json::object());

        emissive_dark_start_hour_ = dark_time.value("start", emissive_dark_start_hour_);
        emissive_dark_end_hour_ = dark_time.value("end", emissive_dark_end_hour_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("读取光照配置文件 '{}' 时出错：{}。自发光显隐将使用默认时间窗口。", config_path, e.what());
        return false;
    }
}

TimeOfDay GameTime::calculateTimeOfDay(float hour) const {
    // 处理跨天的情况（夜晚可能跨天）
    float normalized_hour = hour;
    if (normalized_hour < 0.0f) {
        normalized_hour += 24.0f;
    } else if (normalized_hour >= 24.0f) {
        normalized_hour -= 24.0f;
    }
    
    // 检查夜晚（可能跨天）
    if (config_.night_start_hour_ > config_.night_end_hour_) {
        // 夜晚跨天：20:00-4:00
        if (normalized_hour >= config_.night_start_hour_ || normalized_hour < config_.night_end_hour_) {
            return TimeOfDay::Night;
        }
    } else {
        // 夜晚不跨天
        if (normalized_hour >= config_.night_start_hour_ && normalized_hour < config_.night_end_hour_) {
            return TimeOfDay::Night;
        }
    }
    
    // 检查其他时段
    if (normalized_hour >= config_.dawn_start_hour_ && normalized_hour < config_.dawn_end_hour_) {
        return TimeOfDay::Dawn;
    }
    
    if (normalized_hour >= config_.day_start_hour_ && normalized_hour < config_.day_end_hour_) {
        return TimeOfDay::Day;
    }
    
    if (normalized_hour >= config_.dusk_start_hour_ && normalized_hour < config_.dusk_end_hour_) {
        return TimeOfDay::Dusk;
    }
    
    // 默认返回夜晚（处理边界情况）
    return TimeOfDay::Night;
}

bool GameTime::isDarkForEmissives() const {
    float hour_with_minutes = hour_ + minute_ / 60.0f;
    hour_with_minutes = std::fmod(hour_with_minutes, 24.0f);
    if (hour_with_minutes < 0.0f) {
        hour_with_minutes += 24.0f;
    }

    if (emissive_dark_start_hour_ > emissive_dark_end_hour_) {
        return hour_with_minutes >= emissive_dark_start_hour_ || hour_with_minutes < emissive_dark_end_hour_;
    }
    return hour_with_minutes >= emissive_dark_start_hour_ && hour_with_minutes < emissive_dark_end_hour_;
}

std::string GameTime::getFormattedTime() const {
    return std::format("Day {}, {:02d}:{:02d}", day_, static_cast<int>(hour_), static_cast<int>(minute_));
}

} // namespace game::data
