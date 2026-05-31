#include "game_time.h"

#include "engine/utils/json_file_loader.h"
#include "engine/utils/json_helpers.h"

#include <nlohmann/json.hpp>
#include <format>
#include <cmath>
#include <spdlog/spdlog.h>

namespace game::data {

std::shared_ptr<GameTime> GameTime::loadFromConfig(std::string_view config_path) {
    auto game_time = std::make_shared<GameTime>();
    
    nlohmann::json json;
    if (!engine::utils::loadJsonObjectFile(config_path, json, "GameTime")) {
        spdlog::warn("加载游戏时间配置失败，使用默认配置。");
        return game_time;
    }

    if (!game_time->loadConfigFromJson(json)) {
        spdlog::warn("加载游戏时间配置失败，使用默认配置。");
        return game_time;
    }

    spdlog::info("成功从 '{}' 加载游戏时间配置。", config_path);
    return game_time;
}

bool GameTime::loadConfigFromJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        return false;
    }

    using engine::utils::json::findMember;
    using engine::utils::json::numberOr;

    config_.minutes_per_real_second_ =
        numberOr(json, "minutes_per_real_second", config_.minutes_per_real_second_);

    if (const auto* periods = findMember(json, "time_periods"); periods && periods->is_object()) {
        auto loadPeriod = [&](const char* name, float& start, float& end) {
            const auto* period = findMember(*periods, name);
            if (!period || !period->is_object()) {
                return;
            }
            start = numberOr(*period, "start", start);
            end = numberOr(*period, "end", end);
        };

        loadPeriod("dawn",  config_.dawn_start_hour_,  config_.dawn_end_hour_);
        loadPeriod("day",   config_.day_start_hour_,   config_.day_end_hour_);
        loadPeriod("dusk",  config_.dusk_start_hour_,  config_.dusk_end_hour_);
        loadPeriod("night", config_.night_start_hour_, config_.night_end_hour_);
    }

    if (const auto* initial = findMember(json, "initial_time"); initial && initial->is_object()) {
        day_ = numberOr(*initial, "day", day_);
        hour_ = numberOr(*initial, "hour", hour_);
        minute_ = numberOr(*initial, "minute", minute_);
    }

    time_of_day_ = calculateTimeOfDay(hour_);
    return true;
}

bool GameTime::loadEmissiveVisibilityFromLightConfig(std::string_view config_path) {
    nlohmann::json json;
    if (!engine::utils::loadJsonObjectFile(config_path, json, "GameTime::emissive_visibility")) {
        spdlog::warn("光照配置文件 '{}' 读取失败，自发光显隐将使用默认时间窗口。", config_path);
        return false;
    }

    const auto* visibility = engine::utils::json::findMember(json, "emissive_visibility");
    if (!visibility || !visibility->is_object()) {
        return true;
    }

    const auto* dark_time = engine::utils::json::findMember(*visibility, "dark_time");
    if (!dark_time || !dark_time->is_object()) {
        return true;
    }

    emissive_dark_start_hour_ =
        engine::utils::json::numberOr(*dark_time, "start", emissive_dark_start_hour_);
    emissive_dark_end_hour_ =
        engine::utils::json::numberOr(*dark_time, "end", emissive_dark_end_hour_);
    return true;
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
