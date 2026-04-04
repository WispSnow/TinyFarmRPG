#pragma once
#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"
#include <memory>
#include <string>
#include <string_view>
#include <nlohmann/json_fwd.hpp>    // nlohmann_json 提供的前向声明

namespace engine::core {

/**
 * @brief 管理应用程序的配置设置。
 *
 * 提供配置项的默认值，并支持从 JSON 文件加载/保存配置。
 * 如果加载失败或文件不存在，将使用默认值。
 */
class Config final {
public:
    // --- 默认配置值 --- (为了方便拓展，全部设置为公有)
    // 窗口设置
    std::string window_title_ = "MonsterWar";
    int window_width_ = 1280;
    int window_height_ = 720;
    float window_scale_ = 1.0f;            ///< @brief 窗口坐标尺寸缩放（影响 SDL_CreateWindow 的初始宽高；不改变逻辑分辨率）
    float window_logical_scale_ = 1.0f;    ///< @brief 逻辑分辨率缩放（logical_size = window_width/height * logical_scale；影响离屏渲染与 UI/输入坐标）
    bool window_resizable_ = true;

    // 图形设置
    bool vsync_enabled_ = true;             ///< @brief 是否启用垂直同步
    bool debug_ui_enabled_ = true;          ///< @brief 是否启用 ImGui 调试界面
    bool rmlui_debugger_enabled_ = true;    ///< @brief 是否启用 RmlUi Debugger 运行时开关
    engine::ui::rmlui::RmlUiTextureFilterMode rmlui_texture_filter_mode_{
        engine::ui::rmlui::RmlUiTextureFilterMode::Nearest}; ///< @brief RmlUi 图片和字体 atlas 的采样模式

    // 性能设置
    int target_fps_ = 144;                  ///< @brief 渲染目标 FPS 设置，0 表示不限制
    int logic_tick_hz_ = 60;                ///< @brief 固定逻辑更新频率（Hz），用于计算 fixed_dt
    int max_ticks_per_frame_ = 5;           ///< @brief 单渲染帧允许执行的最大逻辑 tick 数（catch-up 上限）
    bool render_interpolation_enabled_ = false; ///< @brief 是否启用渲染插值（Step 7 预留开关）

    [[nodiscard]] static std::unique_ptr<Config> create(std::string_view filepath);

    // 删除拷贝和移动语义
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    bool loadFromFile(std::string_view filepath);                   ///< @brief 从指定的 JSON 文件加载配置。成功返回 true，否则返回 false。
    [[nodiscard]] bool saveToFile(std::string_view filepath);       ///< @brief 将当前配置保存到指定的 JSON 文件。成功返回 true，否则返回 false。

private:
    explicit Config(std::string_view filepath);                     ///< @brief 构造函数，指定配置文件路径。
    void fromJson(const nlohmann::json& j);                           ///< @brief 从 JSON 对象反序列化配置。
    nlohmann::ordered_json toJson() const;                            ///< @brief 将当前配置转换为 JSON 对象（按顺序）。
};

} // namespace engine::core
