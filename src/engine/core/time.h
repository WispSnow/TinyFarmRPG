#pragma once
#include <SDL3/SDL_stdinc.h>    // 用于 Uint64

namespace engine::core {

/**
 * @brief 管理游戏循环中的时间，计算帧间时间差 (DeltaTime)。
 *
 * 使用 SDL 的高精度性能计数器来确保时间测量的准确性。
 * 提供获取缩放和未缩放 DeltaTime 的方法，以及设置时间缩放因子的能力。
 */
class Time final{
private:
    Uint64 last_time_ = 0;                  ///< @brief 上一渲染帧时间戳（用于计算 frame delta）
    Uint64 frame_start_time_ = 0;           ///< @brief 当前渲染帧开始时间戳（用于渲染帧率限制）
    double unscaled_delta_time_ = 0.0;      ///< @brief 未缩放的渲染帧间时间差（秒）
    double scaled_delta_time_ = 0.0;        ///< @brief 缩放后的渲染帧间时间差（秒）
    double time_scale_ = 1.0;               ///< @brief 时间缩放因子（作用于 accumulator 输入）

    // 渲染帧率限制相关
    int render_target_fps_ = 0;             ///< @brief 渲染目标 FPS（0 表示不限制）
    double render_target_frame_time_ = 0.0; ///< @brief 渲染目标每帧时间（秒）

    // 固定逻辑步长相关
    double fixed_delta_time_ = 1.0 / 60.0;  ///< @brief 固定逻辑步长（秒）
    double accumulator_ = 0.0;              ///< @brief 逻辑时间累积器（秒）
    int max_ticks_per_frame_ = 5;           ///< @brief 单渲染帧允许的最大逻辑 tick 数
    int fixed_ticks_this_frame_ = 0;        ///< @brief 本渲染帧已执行的逻辑 tick 数
    bool catch_up_clamped_this_frame_ = false;  ///< @brief 本渲染帧是否触发了追赶裁剪
    Uint64 dropped_fixed_ticks_total_ = 0;      ///< @brief 累计被裁剪的逻辑 tick 数

public:
    Time();

    // 简单起见，直接删除拷贝、移动和赋值运算符
    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;
    Time(Time&&) = delete;
    Time& operator=(Time&&) = delete;

    /**
     * @brief 每帧开始时调用，更新内部时间状态并计算 DeltaTime。
     */
    void update();

    /**
     * @brief 获取经过时间缩放调整后的渲染帧间时间差 (DeltaTime)。
     *
     * @return float 缩放后的 DeltaTime（秒）。
     */
    float getDeltaTime() const;

    /**
     * @brief 获取未经过时间缩放的原始渲染帧间时间差。
     *
     * @return float 未缩放的 DeltaTime（秒）。
     */
    float getUnscaledDeltaTime() const;

    /**
     * @brief 设置时间缩放因子。
     *
     * @param scale 时间缩放值。1.0 为正常速度，< 1.0 为慢动作，> 1.0 为快进。
     *              不允许负值。
     */
    void setTimeScale(float scale);

    /**
     * @brief 获取当前的时间缩放因子。
     *
     * @return float 当前的时间缩放因子。
     */
    float getTimeScale() const;

    /**
     * @brief 设置渲染目标帧率。
     *
     * @param fps 目标每秒帧数。设置为 0 表示不限制帧率。负值将被视为 0。
     */
    void setTargetFps(int fps);

    /**
     * @brief 获取当前设置的渲染目标帧率。
     *
     * @return int 目标 FPS，0 表示不限制。
     */
    int getTargetFps() const;

    /**
     * @brief 设置固定逻辑步长（秒）。
     * @param delta_seconds 固定步长，必须 > 0。非法值将被忽略。
     */
    void setFixedDeltaTime(float delta_seconds);

    /**
     * @brief 获取固定逻辑步长（秒）。
     */
    float getFixedDeltaTime() const;

    /**
     * @brief 设置每个渲染帧允许执行的最大逻辑 tick 数。
     * @param max_ticks 需 >= 1。非法值将被钳制为 1。
     */
    void setMaxTicksPerFrame(int max_ticks);

    /**
     * @brief 获取每个渲染帧允许执行的最大逻辑 tick 数。
     */
    int getMaxTicksPerFrame() const;

    /**
     * @brief 尝试消费一次固定逻辑 tick 时间片。
     *
     * 语义：
     * - 当 accumulator >= fixed_dt 且未超过 max_ticks_per_frame 时，消费成功并返回 true。
     * - 达到 max_ticks_per_frame 后若仍有 backlog，会裁剪超额累计并记录统计，返回 false。
     */
    bool tryConsumeFixedTick();

    /**
     * @brief 获取当前 accumulator 值（秒）。
     */
    float getAccumulator() const;

    /**
     * @brief 获取渲染插值系数 alpha（accumulator / fixed_dt，范围 [0, 1]）。
     */
    float getInterpolationAlpha() const;

    /**
     * @brief 清空 accumulator（用于场景切换后防止残余时间爆发）。
     */
    void clearAccumulator();

    /**
     * @brief 获取本渲染帧已执行的固定逻辑 tick 数。
     */
    int getFixedTicksThisFrame() const;

    /**
     * @brief 获取本渲染帧是否触发追赶裁剪。
     */
    bool wasCatchUpClampedThisFrame() const;

    /**
     * @brief 获取累计被裁剪掉的固定逻辑 tick 数。
     */
    Uint64 getDroppedFixedTicksTotal() const;

private:
    /**
     * @brief update 中调用，用于限制渲染帧率。如果设置了 render_target_fps_ > 0 且当前帧执行时间小于目标帧时间，则调用 SDL_DelayNS() 等待剩余时间。
     * 
     * @param current_delta_time 当前渲染帧执行时间（秒）
     */
    void limitFrameRate(double current_delta_time);
};

} // namespace engine::core
