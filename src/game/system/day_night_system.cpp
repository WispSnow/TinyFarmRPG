#include "day_night_system.h"
#include "engine/utils/json_file_loader.h"
#include "engine/utils/json_helpers.h"
#include "game/data/game_time.h"
#include "engine/render/lighting_state.h"
#include "game/world/world_state.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>
#include <cmath>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <utility>

namespace {
    constexpr entt::id_type SUN_DIRECTIONAL_LIGHT = entt::hashed_string{"sun"}.value();
    constexpr entt::id_type MOON_DIRECTIONAL_LIGHT = entt::hashed_string{"moon"}.value();

    template <typename T>
    [[nodiscard]] T nestedNumberOr(const nlohmann::json& json,
                                   std::string_view path,
                                   std::string_view key,
                                   T fallback) {
        const auto* section = engine::utils::json::findMember(json, path);
        if (!section || !section->is_object()) {
            return fallback;
        }
        return engine::utils::json::numberOr(*section, key, fallback);
    }

    /**
     * @brief 计算正午混合强度
     * 
     * @param hour_with_minutes 包含分钟的小时数（例如 8.5 表示 8:30）
     * @return float 混合强度（0.0-1.0）
     */
    [[nodiscard]]
    inline float calculateMiddayBlend(float hour_with_minutes) {
        // 将时间转换为相对于 6:00 的偏移（模 24），确保为正数
        float offset = std::fmod(hour_with_minutes - 6.0f + 24.0f, 24.0f);
        
        // 使用正弦函数的绝对值实现平滑过渡
        // 将偏移映射到角度：0-24 小时映射到 0-2π
        // 6:00 (offset=0) -> sin(0) = 0 -> abs(0) = 0 -> 0
        // 12:00 (offset=6) -> sin(π/2) = 1 -> abs(1) = 1 -> 0.5
        // 18:00 (offset=12) -> sin(π) = 0 -> abs(0) = 0 -> 0
        // 0:00/24:00 (offset=18) -> sin(3π/2) = -1 -> abs(-1) = 1 -> 0.5
        
        float angle = (offset / 24.0f) * 2.0f * glm::pi<float>();  // 0-2π
        return 0.5f * std::abs(std::sin(angle));
    }

    /**
     * @brief 根据时间计算太阳/月亮方向
     * 
     * @param hour_with_minutes 包含分钟的小时数（例如 8.5 表示 8:30）
     * @return glm::vec2 归一化的方向向量
     */
    [[nodiscard]]
    inline glm::vec2 calculateSunDirection(float hour_with_minutes) {
        // 将24小时映射到0-1范围，0表示6:00（日出），0.5表示12:00（正午），1.0表示18:00（日落）
        // hour_with_minutes 包含分钟信息，提供更平滑的过渡
        float normalized_time = (hour_with_minutes - 6.0f) / 12.0f;  // 6:00-18:00映射到0-1
        normalized_time = glm::clamp(normalized_time, 0.0f, 1.0f);
        
        // 太阳从东(1,0)移动到西(-1,0)，高度从地平线(0,0)到高(0,-1)再到地平线(0,0)
        // 使用正弦函数模拟太阳高度随时间的变化（日出和日落时低，正午时高）
        float x = 1.0f - normalized_time * 2.0f;  // 1.0 -> -1.0（东到西）
        float y = - std::sin(normalized_time * glm::pi<float>());  // 0-> -1 ->0，高度变化，向下为正
        
        // 光线方向和太阳方向相反，所以取反
        return glm::normalize(glm::vec2(x, y));
    }
}

namespace game::system {

bool LightingConfig::loadFromFile(std::string_view config_path) {
    nlohmann::json json;
    if (!engine::utils::loadJsonObjectFile(config_path, json, "LightingConfig", spdlog::level::err)) {
        return false;
    }

    LightingConfig next = *this;

    if (const auto* tp = engine::utils::json::findMember(json, "transition_periods"); tp && tp->is_object()) {
        next.transition_periods.sunrise_start =
            nestedNumberOr(*tp, "sunrise", "start", next.transition_periods.sunrise_start);
        next.transition_periods.sunrise_end =
            nestedNumberOr(*tp, "sunrise", "end", next.transition_periods.sunrise_end);
        next.transition_periods.sunset_start =
            nestedNumberOr(*tp, "sunset", "start", next.transition_periods.sunset_start);
        next.transition_periods.sunset_end =
            nestedNumberOr(*tp, "sunset", "end", next.transition_periods.sunset_end);
    }

    if (const auto* sun_json = engine::utils::json::findMember(json, "sun"); sun_json && sun_json->is_object()) {
        next.sun.warmth_variation = nestedNumberOr(*sun_json, "color", "warmth_variation", next.sun.warmth_variation);
        next.sun.intensity_base_min = nestedNumberOr(*sun_json, "intensity", "base_min", next.sun.intensity_base_min);
        next.sun.intensity_base_max = nestedNumberOr(*sun_json, "intensity", "base_max", next.sun.intensity_base_max);
        next.sun.offset_min = nestedNumberOr(*sun_json, "offset", "min", next.sun.offset_min);
        next.sun.offset_max = nestedNumberOr(*sun_json, "offset", "max", next.sun.offset_max);
        next.sun.softness = engine::utils::json::numberOr(*sun_json, "softness", next.sun.softness);
    }

    if (const auto* moon_json = engine::utils::json::findMember(json, "moon"); moon_json && moon_json->is_object()) {
        next.moon.color.r = nestedNumberOr(*moon_json, "color", "r", next.moon.color.r);
        next.moon.color.g = nestedNumberOr(*moon_json, "color", "g", next.moon.color.g);
        next.moon.color.b = nestedNumberOr(*moon_json, "color", "b", next.moon.color.b);
        next.moon.intensity = engine::utils::json::numberOr(*moon_json, "intensity", next.moon.intensity);
        next.moon.offset = engine::utils::json::numberOr(*moon_json, "offset", next.moon.offset);
        next.moon.softness = engine::utils::json::numberOr(*moon_json, "softness", next.moon.softness);
    }

    if (const auto* ambient = engine::utils::json::findMember(json, "ambient"); ambient && ambient->is_object()) {
        if (const auto* keyframes = engine::utils::json::findMember(*ambient, "keyframes"); keyframes && keyframes->is_array()) {
            next.ambient_keyframes.clear();
            for (const auto& keyframe_json : *keyframes) {
                if (!keyframe_json.is_object()) {
                    continue;
                }
                const auto* color = engine::utils::json::findMember(keyframe_json, "color");
                if (!color || !color->is_object()) {
                    continue;
                }

                AmbientKeyframe keyframe{};
                keyframe.hour = engine::utils::json::numberOr(keyframe_json, "hour", keyframe.hour);
                keyframe.color.r = engine::utils::json::numberOr(*color, "r", keyframe.color.r);
                keyframe.color.g = engine::utils::json::numberOr(*color, "g", keyframe.color.g);
                keyframe.color.b = engine::utils::json::numberOr(*color, "b", keyframe.color.b);
                next.ambient_keyframes.push_back(keyframe);
            }
        }
    }

    *this = std::move(next);
    spdlog::info("成功加载光照配置: {}", config_path);
    return true;
}

DayNightSystem::DayNightSystem(entt::registry& registry)
    : registry_(registry) {
    // 初始化默认配置
    config_.ambient_keyframes = {
        {4.0f, {0.08f, 0.08f, 0.12f}},
        {6.0f, {0.15f, 0.13f, 0.12f}},
        {9.0f, {0.28f, 0.28f, 0.26f}},
        {14.0f, {0.35f, 0.35f, 0.35f}},
        {18.0f, {0.22f, 0.20f, 0.18f}},
        {22.0f, {0.08f, 0.08f, 0.12f}}
    };
}

bool DayNightSystem::loadConfig(std::string_view config_path) {
    return config_.loadFromFile(config_path);
}

void DayNightSystem::update(const game::data::GameTime* game_time) {
    if (!game_time) {
        spdlog::warn("DayNightSystem::update: GameTime 参数为空");
        return;
    }

    bool is_world_map = true;
    glm::vec3 indoor_ambient{1.0f, 1.0f, 1.0f};
    game::world::WorldState* world_state = nullptr;
    if (auto** ws_ptr = registry_.ctx().find<game::world::WorldState*>()) {
        world_state = *ws_ptr;
    }

    if (world_state) {
        const entt::id_type current_map = world_state->getCurrentMap();
        if (const auto* map_state = world_state->getMapState(current_map)) {
            is_world_map = map_state->info.in_world;
            if (map_state->info.ambient_override) {
                indoor_ambient = *map_state->info.ambient_override;
            }
        }
    }

    // 将分钟转换为小时的小数部分，提供更平滑的过渡
    float hour_with_minutes = game_time->hour_ + game_time->minute_ / 60.0f;
    updateLightingParams(hour_with_minutes);

    auto* state_ptr = registry_.ctx().find<engine::render::GlobalLightingState>();
    if (!state_ptr) {
        spdlog::warn("DayNightSystem::update: 注册表上下文中未找到 GlobalLightingState");
        return;
    }

    auto& state = *state_ptr;
    state.ambient = is_world_map ? ambient_ : indoor_ambient;

    auto& sun = state.directional_lights[SUN_DIRECTIONAL_LIGHT];
    if (is_world_map) {
        sun.enabled = sun_intensity_ > 0.001f;
        sun.direction = sun_direction_;
        sun.options = {};
        sun.options.color = sun_color_;
        sun.options.intensity = sun_intensity_;
        sun.options.offset = sun_offset_;
        sun.options.softness = sun_softness_;
        sun.options.midday_blend = sun_midday_blend_;
    } else {
        sun.enabled = false;
    }

    auto& moon = state.directional_lights[MOON_DIRECTIONAL_LIGHT];
    if (is_world_map) {
        moon.enabled = moon_intensity_ > 0.001f;
        moon.direction = moon_direction_;
        moon.options = {};
        moon.options.color = moon_color_;
        moon.options.intensity = moon_intensity_;
        moon.options.offset = moon_offset_;
        moon.options.softness = moon_softness_;
        moon.options.midday_blend = moon_midday_blend_;
    } else {
        moon.enabled = false;
    }
}

namespace {
    [[nodiscard]] float smoothstep01(float t) {
        t = glm::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
} // namespace

float DayNightSystem::calculateSunFade(float normalized_hour) const {
    const float sunrise_start = config_.transition_periods.sunrise_start;
    const float sunrise_end = config_.transition_periods.sunrise_end;
    const float sunset_start = config_.transition_periods.sunset_start;
    const float sunset_end = config_.transition_periods.sunset_end;

    if (normalized_hour >= sunrise_start && normalized_hour < sunrise_end) {
        // 日出：从 0 淡入到 1
        float fade = (normalized_hour - sunrise_start) / (sunrise_end - sunrise_start);
        return smoothstep01(fade);
    } else if (normalized_hour >= sunrise_end && normalized_hour < sunset_start) {
        // 白天：完全可见
        return 1.0f;
    } else if (normalized_hour >= sunset_start && normalized_hour < sunset_end) {
        // 日落：从 1 淡出到 0
        float fade = 1.0f - (normalized_hour - sunset_start) / (sunset_end - sunset_start);
        return smoothstep01(fade);
    }
    return 0.0f;
}

float DayNightSystem::calculateMoonFade(float normalized_hour) const {
    const float sunrise_start = config_.transition_periods.sunrise_start;
    const float sunrise_end = config_.transition_periods.sunrise_end;
    const float sunset_start = config_.transition_periods.sunset_start;
    const float sunset_end = config_.transition_periods.sunset_end;

    if (normalized_hour >= sunset_start && normalized_hour < sunset_end) {
        // 月升：从 0 淡入到 1
        float fade = (normalized_hour - sunset_start) / (sunset_end - sunset_start);
        return smoothstep01(fade);
    } else if (normalized_hour >= sunset_end || normalized_hour < sunrise_start) {
        // 夜晚：完全可见
        return 1.0f;
    } else if (normalized_hour >= sunrise_start && normalized_hour < sunrise_end) {
        // 月落：从 1 淡出到 0
        float fade = 1.0f - (normalized_hour - sunrise_start) / (sunrise_end - sunrise_start);
        return smoothstep01(fade);
    }
    return 0.0f;
}

void DayNightSystem::interpolateAmbient(float normalized_hour) {
    if (config_.ambient_keyframes.empty()) {
        return;
    }

    // 找到环境光关键帧
    size_t amb_idx0 = 0, amb_idx1 = 1;
    for (size_t i = 0; i < config_.ambient_keyframes.size() - 1; ++i) {
        if (normalized_hour >= config_.ambient_keyframes[i].hour &&
            normalized_hour < config_.ambient_keyframes[i + 1].hour) {
            amb_idx0 = i;
            amb_idx1 = i + 1;
            break;
        }
    }

    // 处理跨午夜情况
    float hour_for_ambient = normalized_hour;
    if (normalized_hour >= config_.ambient_keyframes.back().hour ||
        normalized_hour < config_.ambient_keyframes.front().hour) {
        amb_idx0 = config_.ambient_keyframes.size() - 1;
        amb_idx1 = 0;
        if (normalized_hour < config_.ambient_keyframes.front().hour) {
            hour_for_ambient += 24.0f;
        }
    }

    const auto& amb_kf0 = config_.ambient_keyframes[amb_idx0];
    const auto& amb_kf1 = config_.ambient_keyframes[amb_idx1];
    float amb_span = amb_kf1.hour - amb_kf0.hour;
    if (amb_span <= 0.0f) amb_span += 24.0f;
    float amb_t = (hour_for_ambient - amb_kf0.hour) / amb_span;
    amb_t = smoothstep01(amb_t);
    ambient_ = glm::mix(amb_kf0.color, amb_kf1.color, amb_t);
}

void DayNightSystem::updateSunState(float hour_with_minutes, float sun_fade) {
    if (sun_fade <= 0.001f) {
        sun_intensity_ = 0.0f;
        return;
    }

    // 计算太阳方向
    sun_direction_ = calculateSunDirection(hour_with_minutes);

    // 太阳颜色随时间变化（使用配置中的 warmth_variation）
    float sun_time_factor = (hour_with_minutes - 6.0f) / 12.0f;  // 6:00-18:00 映射到 0-1
    sun_time_factor = glm::clamp(sun_time_factor, 0.0f, 1.0f);

    // 早晚更暖（橙色），正午更冷（白色）
    float warmth = std::abs(sun_time_factor - 0.5f) * 2.0f;  // 0（正午）到 1（早晚）
    float warmth_factor = config_.sun.warmth_variation;
    glm::vec3 sun_rgb = glm::vec3(
        1.0f,
        0.95f - warmth_factor * warmth,
        0.75f - warmth_factor * warmth
    );
    sun_color_ = engine::utils::FColor(sun_rgb.r, sun_rgb.g, sun_rgb.b);

    // 太阳强度：正午最强，早晚较弱（使用配置）
    float base_intensity = config_.sun.intensity_base_min +
                           config_.sun.intensity_base_max * std::sin(sun_time_factor * glm::pi<float>());
    sun_intensity_ = base_intensity * sun_fade;  // 应用淡入淡出

    sun_offset_ = config_.sun.offset_min +
                  config_.sun.offset_max * std::sin(sun_time_factor * glm::pi<float>());
    sun_softness_ = config_.sun.softness;
    sun_midday_blend_ = calculateMiddayBlend(hour_with_minutes) * sun_fade;
}

void DayNightSystem::updateMoonState(float hour_with_minutes, float moon_fade) {
    if (moon_fade <= 0.001f) {
        moon_intensity_ = 0.0f;
        return;
    }

    // 月亮方向（与太阳相差约12小时）
    float moon_hour = hour_with_minutes < 12.0f ? hour_with_minutes + 12.0f : hour_with_minutes - 12.0f;
    moon_direction_ = calculateSunDirection(moon_hour);

    // 月光颜色（使用配置）
    moon_color_ = engine::utils::FColor(
        config_.moon.color.r,
        config_.moon.color.g,
        config_.moon.color.b
    );

    // 月光强度（使用配置）
    moon_intensity_ = config_.moon.intensity * moon_fade;  // 应用淡入淡出

    moon_offset_ = config_.moon.offset;
    moon_softness_ = config_.moon.softness;
    moon_midday_blend_ = calculateMiddayBlend(hour_with_minutes) * moon_fade;
}

void DayNightSystem::updateLightingParams(float hour_with_minutes) {
    // 使用双光源（太阳+月亮）系统，在日出/日落时段进行交叉淡入淡出

    // 标准化小时到 0-24 范围
    float normalized_hour = std::fmod(hour_with_minutes, 24.0f);
    if (normalized_hour < 0.0f) normalized_hour += 24.0f;

    // 计算各光源强度
    float sun_fade = calculateSunFade(normalized_hour);
    float moon_fade = calculateMoonFade(normalized_hour);

    // 更新环境光
    interpolateAmbient(normalized_hour);

    // 更新太阳和月亮状态
    updateSunState(hour_with_minutes, sun_fade);
    updateMoonState(hour_with_minutes, moon_fade);
}

} // namespace game::system
