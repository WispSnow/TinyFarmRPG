#pragma once

#include "engine/utils/defs.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <entt/entity/fwd.hpp>
#include <string_view>
#include <vector>

namespace game::system {

/**
 * @brief 光照配置结构
 */
struct LightingConfig {
    // 过渡时段
    struct {
        float sunrise_start{5.0f};
        float sunrise_end{7.0f};
        float sunset_start{17.0f};
        float sunset_end{19.0f};
    } transition_periods;

    // 太阳配置
    struct {
        float warmth_variation{0.2f};
        float intensity_base_min{0.4f};
        float intensity_base_max{0.8f};
        float offset_min{0.1f};
        float offset_max{0.2f};
        float softness{0.22f};
    } sun;

    // 月亮配置
    struct {
        glm::vec3 color{0.65f, 0.7f, 0.9f};
        float intensity{0.2f};
        float offset{0.25f};
        float softness{0.38f};
    } moon;

    // 环境光关键帧
    struct AmbientKeyframe {
        float hour;
        glm::vec3 color;
    };
    std::vector<AmbientKeyframe> ambient_keyframes;

    /**
     * @brief 从 JSON 文件加载配置
     *
     * @param config_path 配置文件路径
     * @return true 加载成功
     * @return false 加载失败
     */
    bool loadFromFile(std::string_view config_path);
};

/**
 * @brief 昼夜光照系统
 *
 * 根据游戏时间计算并应用昼夜光照效果，包括方向光和环境光。
 */
class DayNightSystem {
    entt::registry& registry_;

    // 光照配置
    LightingConfig config_;

    // 太阳光照参数
    glm::vec2 sun_direction_{0.0f, -1.0f};     ///< 太阳方向
    engine::utils::FColor sun_color_{engine::utils::FColor::white()};  ///< 太阳光颜色
    float sun_intensity_{1.0f};                ///< 太阳光强度
    float sun_offset_{0.5f};                   ///< 太阳光偏移
    float sun_softness_{0.25f};                ///< 太阳光柔和度
    float sun_midday_blend_{0.0f};             ///< 太阳正午混合强度

    // 月亮光照参数
    glm::vec2 moon_direction_{0.0f, -1.0f};    ///< 月亮方向
    engine::utils::FColor moon_color_{engine::utils::FColor::white()};  ///< 月光颜色
    float moon_intensity_{0.0f};               ///< 月光强度
    float moon_offset_{0.5f};                  ///< 月光偏移
    float moon_softness_{0.35f};               ///< 月光柔和度
    float moon_midday_blend_{0.0f};            ///< 月亮正午混合强度

    // 环境光
    glm::vec3 ambient_{0.3f, 0.3f, 0.3f};     ///< 环境光颜色

public:
    explicit DayNightSystem(entt::registry& registry);
    ~DayNightSystem() = default;

    /**
     * @brief 加载光照配置
     *
     * @param config_path 配置文件路径
     * @return true 加载成功
     * @return false 加载失败
     */
    bool loadConfig(std::string_view config_path);

    /**
     * @brief 更新光照参数（根据当前游戏时间）
     */
    void update();

private:
    /**
     * @brief 根据时间更新光照参数
     *
     * @param hour_with_minutes 包含分钟的小时数（例如 8.5 表示 8:30），用于平滑过渡
     */
    void updateLightingParams(float hour_with_minutes);

    /**
     * @brief 计算太阳淡入淡出强度
     * @param normalized_hour 标准化的小时数 (0-24)
     * @return 太阳强度因子 (0.0-1.0)
     */
    [[nodiscard]] float calculateSunFade(float normalized_hour) const;

    /**
     * @brief 计算月亮淡入淡出强度
     * @param normalized_hour 标准化的小时数 (0-24)
     * @return 月亮强度因子 (0.0-1.0)
     */
    [[nodiscard]] float calculateMoonFade(float normalized_hour) const;

    /**
     * @brief 插值环境光颜色
     * @param normalized_hour 标准化的小时数 (0-24)
     */
    void interpolateAmbient(float normalized_hour);

    /**
     * @brief 更新太阳光照状态
     * @param hour_with_minutes 包含分钟的小时数
     * @param sun_fade 太阳强度因子
     */
    void updateSunState(float hour_with_minutes, float sun_fade);

    /**
     * @brief 更新月亮光照状态
     * @param hour_with_minutes 包含分钟的小时数
     * @param moon_fade 月亮强度因子
     */
    void updateMoonState(float hour_with_minutes, float moon_fade);
};

} // namespace game::system
